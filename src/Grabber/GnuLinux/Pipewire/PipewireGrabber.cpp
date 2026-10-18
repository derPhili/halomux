#include <Halomux/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>

#include <Halomux/Grabber/GnuLinux/Pipewire/DmaBufPlanes.hpp>

#include <algorithm>
#include <utility>
#include <array>
#include <cstdlib>
#include <future>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <Halomux/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp>
#include <Halomux/Grabber/GnuLinux/WaylandOutputs.hpp>
#include <Halomux/Imaging/ImageProcessing.hpp>
#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpedantic"
  #pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpedantic"
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <spa/debug/types.h>
#include <spa/param/video/type-info.h>
#include <spa/utils/dict.h>
#include <spa/param/buffers.h>

#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif



#include <drm_fourcc.h>


using namespace std::chrono_literals;


namespace
{
  // Packed 4 bytes per pixel formats handled by both paths, preferred first
  const std::vector<spa_video_format> SupportedFormats = {
    SPA_VIDEO_FORMAT_BGRx,
    SPA_VIDEO_FORMAT_BGRA,
    SPA_VIDEO_FORMAT_RGBx,
    SPA_VIDEO_FORMAT_RGBA
  };


  uint32_t toDrmFormat(
    spa_video_format format
  )
  {
    // SPA names follow the byte order in memory, DRM ones the little-endian word layout
    switch(format){
      case SPA_VIDEO_FORMAT_BGRx: return DRM_FORMAT_XRGB8888;
      case SPA_VIDEO_FORMAT_BGRA: return DRM_FORMAT_ARGB8888;
      case SPA_VIDEO_FORMAT_RGBx: return DRM_FORMAT_XBGR8888;
      case SPA_VIDEO_FORMAT_RGBA: return DRM_FORMAT_ABGR8888;
      default: return DRM_FORMAT_INVALID;
    }
  }
}


namespace Halomux::Grabber
{
  PipewireGrabber::PipewireGrabber(
    Core::Config* config,
    bool askForMonitors,
    std::stop_token stopToken
  ):
  IGrabber(config)
  {
    m_pwData.config = config;
    m_capture.config = config;
    m_capture.askForMonitors = askForMonitors;

    std::promise<bool> fdReadyPromise;
    auto fdReadyFuture = fdReadyPromise.get_future();
    m_capture.fdReadyPromise = std::move(fdReadyPromise);
    // Created before the portal thread: sources may be attached to it from here at once
    m_capture.context = g_main_context_new();
    m_xdgThread.emplace(_initCapture, &m_capture);

    // From here on, a failure must end the portal thread before the exception leaves: a joinable
    // thread destroyed while unwinding would terminate the application
    try{
      // The screen selection dialog may stay open: a stop request closes it
      GSource* abortSource = nullptr;
      {
        std::stop_callback abortOnStop(stopToken, [this, &abortSource](){
          // Runs in the thread requesting the stop (or right here if it already was): the abort
          // itself runs in the portal thread, through its context
          abortSource = g_idle_source_new();
          g_source_set_callback(abortSource, _onAbortCapture, &m_capture, nullptr);
          g_source_attach(abortSource, m_capture.context);
        });

        fdReadyFuture.wait();
      }

      if(abortSource){
        g_source_destroy(abortSource);
        g_source_unref(abortSource);
      }

      if(!fdReadyFuture.get()){
        if(m_capture.cancelled){
          throw Grabber::GrabberCancelled("Screen selection cancelled");
        }

        // Such as no portal yet at login: worth trying again
        throw Grabber::GrabberUnavailable("The screen capture portal failed to provide the monitors");
      }

      // Names the streams in the log, and the monitors once their sizes are known
      const auto outputs = listWaylandOutputs();

      for(size_t i = 0; i < m_capture.streams.size(); i++){
        auto stream = std::make_unique<StreamData>();
        stream->owner = &m_pwData;
        stream->index = i;
        stream->node = m_capture.streams[i].node;
        stream->portalGeometry = m_capture.streams[i].geometry;
        stream->label = "monitor " + std::to_string(i + 1);

        for(const auto& output : outputs){
          if(stream->portalGeometry == output.geometry && !output.name.empty()){
            stream->label = output.name;
          }
        }

        m_pwData.streams.push_back(std::move(stream));
      }

      auto configDataReadyFuture = m_pwData.screenDataReadyPromise.get_future();
      m_loopThread.emplace(
        [this](pw_main_loop* loop){return _startCapture(&m_capture, &m_pwData, loop);},
        [this](pw_main_loop*){_stopCapture(&m_pwData);}
      );

      // A stream that never negotiates must neither hang a screen selection nor the shutdown
      static constexpr auto StreamsTimeout = std::chrono::seconds(15);
      const auto deadline = std::chrono::steady_clock::now() + StreamsTimeout;
      while(configDataReadyFuture.wait_for(100ms) != std::future_status::ready){
        if(stopToken.stop_requested()){
          throw Grabber::GrabberCancelled("Screen capture aborted");
        }

        if(std::chrono::steady_clock::now() >= deadline){
          throw Grabber::GrabberUnavailable("The capture of the selected monitors didn't start");
        }
      }

      if(!configDataReadyFuture.get()){
        throw Grabber::GrabberUnavailable("Failed to initialize Pipewire capture");
      }

      // Every stream has its format
      std::vector<std::pair<int, int>> pixelSizes;
      for(const auto& stream : m_pwData.streams){
        pixelSizes.emplace_back(stream->pixelWidth.load(), stream->pixelHeight.load());
      }

      const auto fallback = fallbackGeometry(pixelSizes);
      for(size_t i = 0; i < m_pwData.streams.size(); i++){
        MonitorInfo monitor;
        monitor.geometry = m_pwData.streams[i]->portalGeometry.value_or(fallback[i]);
        monitor.pixelWidth = pixelSizes[i].first;
        monitor.pixelHeight = pixelSizes[i].second;
        m_monitors.push_back(std::move(monitor));
      }

      nameMonitors(m_monitors, outputs);

      for(const auto& monitor : m_monitors){
        Core::Logger::log("Capturing ", monitor.description, (monitor.name.empty() ? "" : " (" + monitor.name + ")"), ", ", monitor.pixelWidth, "x", monitor.pixelHeight, " at ", monitor.geometry.x, ",", monitor.geometry.y);
      }

      // Saved only now: a token of a selection that doesn't capture would ask again on every start
      if(m_capture.restoreToken.has_value() && config->restoreToken() != m_capture.restoreToken){
        config->setRestoreToken(*m_capture.restoreToken);
        Core::Logger::log("Remembered the screen selection");
      }
    }
    catch(...){
      _stop();
      throw;
    }
  }


  PipewireGrabber::~PipewireGrabber()
  {
    _stop();
  }


  const std::string& PipewireGrabber::name() const
  {
    static const std::string s_identifier = "PipewireGrabber";
    return s_identifier;
  }



  IGrabber::Resolution PipewireGrabber::displayResolution() const
  {
    Resolution widest{0, 0};
    for(const auto& monitor : m_monitors){
      if(monitor.pixelWidth > widest.x){
        widest = {monitor.pixelWidth, monitor.pixelHeight};
      }
    }

    return widest;
  }


  IGrabber::RefreshRate PipewireGrabber::displayRefreshRate() const
  {
    // Highest framerate offered by the compositor, regardless of the limit requested
    // through setFrameRateLimit()
    unsigned displayMaxFramerate = 0;
    for(const auto& stream : m_pwData.streams){
      displayMaxFramerate = std::max(displayMaxFramerate, stream->displayMaxFramerate.load());
    }

    if(displayMaxFramerate > 0){
      return displayMaxFramerate;
    }

    static constexpr IGrabber::RefreshRate FallbackRefreshRate = 60;
    return FallbackRefreshRate;
  }


  void PipewireGrabber::setChannelRegions(
    const ChannelRegions& regions
  )
  {
    {
      auto lock = std::lock_guard(m_pwData.channelsMutex);
      for(auto& stream : m_pwData.streams){
        stream->regions.clear();
      }

      for(const auto& region : regions){
        if(region.monitor < m_pwData.streams.size()){
          m_pwData.streams[region.monitor]->regions.push_back(region);
        }
      }
    }

    if(m_pwData.loop){
      pw_loop_invoke(pw_main_loop_get_loop(m_pwData.loop), _onUpdateRegions, 0, nullptr, 0, false, &m_pwData);
    }
  }


  bool PipewireGrabber::grabChannelColors(
    ChannelColors& colors
  )
  {
    auto lock = std::lock_guard(m_pwData.channelsMutex);

    colors.clear();
    bool hasRegions = false;
    bool hasColors = false;
    for(const auto& stream : m_pwData.streams){
      if(stream->regions.empty()){
        continue;
      }

      hasRegions = true;
      if(stream->hasColors){
        hasColors = true;
        colors.insert(colors.end(), stream->colors.begin(), stream->colors.end());
      }
    }

    // Without regions there is nothing to wait for
    return hasColors || !hasRegions;
  }


  IGrabber::ProcessingInfo PipewireGrabber::processingInfo() const
  {
    std::vector<const StreamData*> active;
    {
      auto lock = std::lock_guard(m_pwData.channelsMutex);
      for(const auto& stream : m_pwData.streams){
        if(stream->streaming && !stream->regions.empty()){
          active.push_back(stream.get());
        }
      }
    }

    if(active.empty() && !m_pwData.streams.empty()){
      active.push_back(m_pwData.streams.front().get());
    }

    auto lock = std::lock_guard(m_pwData.processingInfoMutex);

    for(const StreamData* stream : active){
      if(!stream->lastFrameOnGpu){
        const std::string& reason = stream->cpuReason.empty() ? m_pwData.cpuReason : stream->cpuReason;
        if(m_monitors.size() > 1 && stream->index < m_monitors.size()){
          return {.gpu = false, .details = m_monitors[stream->index].description + ": " + reason};
        }

        return {.gpu = false, .details = reason};
      }
    }

    return {.gpu = true, .details = m_pwData.gpuName + ", DMA-BUF"};
  }


  Monitors PipewireGrabber::monitors() const
  {
    Monitors monitors = m_monitors;

    auto lock = std::lock_guard(m_pwData.processingInfoMutex);
    for(size_t i = 0; i < monitors.size() && i < m_pwData.streams.size(); i++){
      const StreamData& stream = *m_pwData.streams[i];
      monitors[i].lost = stream.lost;
      monitors[i].active = stream.streaming && !stream.lost;
      monitors[i].gpu = stream.lastFrameOnGpu;
      monitors[i].processingDetails = stream.lastFrameOnGpu
        ? m_pwData.gpuName + ", DMA-BUF"
        : (stream.cpuReason.empty() ? m_pwData.cpuReason : stream.cpuReason);
    }

    return monitors;
  }


  void PipewireGrabber::_onCoreErrorCallback(
    void* userData,
    uint32_t id,
    int seq,
    int res,
    const char* message
  )
  {
    (void)userData;

    // res is a negative errno
    Core::Logger::error("[pipewire] Error id: ", id, " seq: ", seq, " res: ", res, " (", g_strerror(-res), ") ", message);
  }


  void PipewireGrabber::_onStreamProcess(
    void* userdata
  )
  {
    StreamData* stream = static_cast<StreamData*>(userdata);
    PipewireData* pw = stream->owner;

    // Several frames wait when processing fell behind: only the newest is worth it, the older
    // ones go back at once instead of adding their delay
    pw_buffer* pwBuffer = nullptr;
    while(pw_buffer* next = pw_stream_dequeue_buffer(stream->stream)){
      if(pwBuffer){
        pw_stream_queue_buffer(stream->stream, pwBuffer);
      }
      pwBuffer = next;
    }

    if(!pwBuffer){
      // Out of buffers
      return;
    }

    const auto width = static_cast<int>(stream->format.info.raw.size.width);
    const auto height = static_cast<int>(stream->format.info.raw.size.height);

    // Only 4 bytes per pixel formats are negotiated (see SupportedFormats)
    Imaging::PixelFormat pixelFormat;
    switch(stream->format.info.raw.format){
      case SPA_VIDEO_FORMAT_BGRx:
      case SPA_VIDEO_FORMAT_BGRA:
        pixelFormat = Imaging::PixelFormat::BGRA;
        break;

      case SPA_VIDEO_FORMAT_RGBx:
      case SPA_VIDEO_FORMAT_RGBA:
        pixelFormat = Imaging::PixelFormat::RGBA;
        break;

      default:
        pw_stream_queue_buffer(stream->stream, pwBuffer);
        return;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];
    const bool corrupted = data.chunk != nullptr && (data.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED);
    // Compositors flag a buffer without new content (such as for a cursor move only) with size 0,
    // on both paths: it may still hold an older frame
    const bool empty = data.chunk != nullptr && data.chunk->size == 0;

    if(width == 0 || height == 0 || corrupted || empty){
      pw_stream_queue_buffer(stream->stream, pwBuffer);
      return;
    }

    ChannelRegions regions;
    {
      auto lock = std::lock_guard(pw->channelsMutex);
      regions = stream->regions;
    }

    // Every monitor is subsampled in the same ratio as the configured width of the widest one
    const int subsampleWidth = subsampleWidthFor(pw->config->subsampleWidth(), _referencePixelWidth(pw), width);

    ChannelColors colors;
    const bool computed = data.type == SPA_DATA_DmaBuf
      ? _processDmaBuf(stream, pwBuffer, width, height, subsampleWidth, regions, colors)
      : _processSharedMemory(stream, pwBuffer, width, height, subsampleWidth, pixelFormat, regions, colors);

    if(computed){
      stream->lastFrameOnGpu = data.type == SPA_DATA_DmaBuf;

      auto lock = std::lock_guard(pw->channelsMutex);
      stream->colors = std::move(colors);
      stream->hasColors = true;
    }

    pw_stream_queue_buffer(stream->stream, pwBuffer);
  }


  bool PipewireGrabber::_processDmaBuf(
    StreamData* stream,
    pw_buffer* pwBuffer,
    int width,
    int height,
    int subsampleWidth,
    const ChannelRegions& regions,
    ChannelColors& colors
  )
  {
    PipewireData* pw = stream->owner;
    if(!pw->gpu || stream->dmaBufFailed){
      return false;
    }

    // One data per plane: compressed formats have several
    const auto planes = dmaBufPlanes(*pwBuffer->buffer);
    if(!planes.has_value()){
      return false;
    }

    auto* bufferData = static_cast<BufferData*>(pwBuffer->user_data);
    if(!bufferData){
      bufferData = new BufferData();
      pwBuffer->user_data = bufferData;
    }

    // Imported once per buffer: Pipewire cycles through a fixed set of DMA-BUFs
    if(!bufferData->gpuImage){
      bufferData->gpuImage = pw->gpu->importDmaBuf({
        .planes = *planes,
        .width = width,
        .height = height,
        .drmFormat = toDrmFormat(stream->format.info.raw.format),
        .modifier = stream->format.info.raw.modifier
      });
    }

    subsampleWidth = std::min(subsampleWidth, width);
    const int subsampleHeight = std::max(1, static_cast<int>(static_cast<int64_t>(height) * subsampleWidth / width));

    // Regions are taken from the subsample, exactly like on the CPU path
    std::vector<cv::Rect> rects;
    rects.reserve(regions.size());
    for(const auto& region : regions){
      rects.push_back(Imaging::ImageProcessing::subImageRect(subsampleWidth, subsampleHeight, region.uvs));
    }

    std::vector<cv::Vec4b> rgbaColors;
    if(!bufferData->gpuImage || !pw->gpu->averageRegions(*bufferData->gpuImage, subsampleWidth, subsampleHeight, rects, rgbaColors, stream->index)){
      // Fall back to shared memory buffers for good
      Core::Logger::warn("[gpu] GPU downscaling failed on ", stream->label, ", falling back to the CPU path");
      stream->dmaBufFailed = true;
      {
        auto lock = std::lock_guard(pw->processingInfoMutex);
        stream->cpuReason = "GPU downscaling failed, shared memory";
      }
      stream->renegotiate = true;
      pw_loop_signal_event(pw_main_loop_get_loop(pw->loop), pw->renegotiateEvent);
      return false;
    }

    colors.clear();
    colors.reserve(regions.size());
    for(size_t i = 0; i < regions.size(); i++){
      const cv::Vec4b& rgba = rgbaColors[i];
      colors.emplace_back(regions[i].channelId, Imaging::Color(rgba[0], rgba[1], rgba[2]));
    }

    return true;
  }


  bool PipewireGrabber::_processSharedMemory(
    StreamData* /*stream*/,
    pw_buffer* pwBuffer,
    int width,
    int height,
    int subsampleWidth,
    Imaging::PixelFormat pixelFormat,
    const ChannelRegions& regions,
    ChannelColors& colors
  )
  {
    const spa_data& data = pwBuffer->buffer->datas[0];

    // Prefer our own read-only mapping of MemFd buffers (see _onStreamAddBuffer)
    const auto* bufferData = static_cast<const BufferData*>(pwBuffer->user_data);
    const uint8_t* readPtr = (bufferData && bufferData->data)
      ? bufferData->data
      : static_cast<const uint8_t*>(data.data);

    if(readPtr == nullptr || data.chunk == nullptr || data.chunk->size == 0){
      return false;
    }

    const size_t step = data.chunk->stride > 0
      ? static_cast<size_t>(data.chunk->stride)
      : static_cast<size_t>(width) * 4;

    // View on the Pipewire memory, which becomes invalid after queue_buffer()
    const cv::Mat frameView(height, width, CV_8UC4, const_cast<uint8_t*>(readPtr) + data.chunk->offset, step);
    const Imaging::ImageData frameViewData{.imageMatrix = frameView, .format = pixelFormat};

    // Downscale straight from the shared memory instead of copying the full
    // resolution frame first: only the subsample is copied out.
    Imaging::ImageData subsample;

    if(width > subsampleWidth){
      Imaging::ImageProcessing::rescaleSampled(frameViewData, subsample, subsampleWidth);
    }
    else{
      subsample = {.imageMatrix = frameView.clone(), .format = pixelFormat};
    }

    Imaging::ImageProcessing::toBgr(subsample, subsample);

    std::vector<Imaging::UVs> uvs;
    uvs.reserve(regions.size());
    for(const auto& region : regions){
      uvs.push_back(region.uvs);
    }

    const auto regionColors = Imaging::ImageProcessing::averageRegions(subsample, uvs);

    colors.clear();
    colors.reserve(regions.size());
    for(size_t i = 0; i < regions.size(); i++){
      colors.emplace_back(regions[i].channelId, regionColors[i]);
    }

    return true;
  }


  void PipewireGrabber::_onStreamAddBuffer(
    void* /*userdata*/,
    pw_buffer* buffer
  )
  {
    // Some xdg-desktop-portal / pipewire combinations advertise SPA_DATA_MemFd
    // buffers without auto-mapping them with PROT_READ even when
    // PW_STREAM_FLAG_MAP_BUFFERS is set. Reading via the provided
    // datas[0].data pointer then segfaults. Map the fd ourselves, once for
    // the lifetime of the buffer.
    const spa_data& data = buffer->buffer->datas[0];
    if(data.type != SPA_DATA_MemFd || data.fd < 0){
      return;
    }

    // The buffer data starts at mapoffset within the fd
    const size_t mapSize = data.mapoffset + static_cast<size_t>(data.maxsize);
    void* map = mmap(nullptr, mapSize, PROT_READ, MAP_SHARED, static_cast<int>(data.fd), 0);
    if(map == MAP_FAILED){
      return;
    }

    auto* bufferData = new BufferData();
    bufferData->map = map;
    bufferData->mapSize = mapSize;
    bufferData->data = static_cast<const uint8_t*>(map) + data.mapoffset;
    buffer->user_data = bufferData;
  }


  void PipewireGrabber::_onStreamRemoveBuffer(
    void* userdata,
    pw_buffer* buffer
  )
  {
    StreamData* stream = static_cast<StreamData*>(userdata);
    auto* bufferData = static_cast<BufferData*>(buffer->user_data);
    if(!bufferData){
      return;
    }

    if(bufferData->map){
      munmap(bufferData->map, bufferData->mapSize);
    }

    if(bufferData->gpuImage && stream->owner->gpu){
      stream->owner->gpu->releaseImage(*bufferData->gpuImage);
    }

    delete bufferData;
    buffer->user_data = nullptr;
  }


  void PipewireGrabber::_onStreamStateChanged(
    void* userdata,
    pw_stream_state /*old*/,
    pw_stream_state state,
    const char* error
  )
  {
    StreamData* stream = static_cast<StreamData*>(userdata);
    if(stream->owner->stopping || (state != PW_STREAM_STATE_ERROR && state != PW_STREAM_STATE_UNCONNECTED)){
      return;
    }

    if(stream->lost.exchange(true)){
      return;
    }

    Core::Logger::warn("Capture of ", stream->label, " ended", (error ? std::string(": ") + error : std::string()), ". Its lamps stay off while syncing");

    // Its last colors would be handed out forever
    auto lock = std::lock_guard(stream->owner->channelsMutex);
    stream->hasColors = false;
  }


  void PipewireGrabber::_onStreamParamChanged(
    void* userdata,
    uint32_t id,
    const spa_pod* param
  )
  {
    StreamData* stream = static_cast<StreamData*>(userdata);
    PipewireData* pw = stream->owner;

    if(param == NULL || id != SPA_PARAM_Format){
      return;
    }

    if(spa_format_parse(param, &stream->format.media_type, &stream->format.media_subtype) < 0){
      return;
    }

    if(stream->format.media_type != SPA_MEDIA_TYPE_video || stream->format.media_subtype != SPA_MEDIA_SUBTYPE_raw){
      return;
    }

    if(spa_format_video_raw_parse(param, &stream->format.info.raw) < 0){
      return;
    }

    // Variable framerate streams advertise their bound in max_framerate, fixed ones in framerate.
    // Keep the highest value seen: later negotiations are limited by setFrameRateLimit().
    for(const auto& fraction : {stream->format.info.raw.max_framerate, stream->format.info.raw.framerate}){
      if(fraction.num > 0 && fraction.denom > 0){
        unsigned framerate = fraction.num / fraction.denom;
        if(framerate > stream->displayMaxFramerate){
          stream->displayMaxFramerate = framerate;
        }
        break;
      }
    }

    // DMA-BUF formats carry a modifier: ask for the matching buffer type
    const spa_pod_prop* modifierProp = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier);
    const bool dmaBuf = modifierProp != nullptr;
    // The compositor first offers a choice of modifiers, then the one it allocates with: a plain
    // value or a choice of type None
    const bool modifierFixated = dmaBuf && (
      SPA_POD_TYPE(&modifierProp->value) != SPA_TYPE_Choice ||
      SPA_POD_CHOICE_TYPE(&modifierProp->value) == SPA_CHOICE_None
    );
    const int dataTypes = dmaBuf
      ? (1 << SPA_DATA_DmaBuf)
      : ((1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr));

    std::array<uint8_t, 1024> buffer;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
    const spa_pod* buffersParam = static_cast<const spa_pod*>(spa_pod_builder_add_object(
      &b,
      SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
      SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(dataTypes)
    ));
#pragma GCC diagnostic pop
    pw_stream_update_params(stream->stream, &buffersParam, 1);

    if(dmaBuf != stream->usingDmaBuf || !stream->hasFormat){
      stream->usingDmaBuf = dmaBuf;
      stream->loggedModifier.reset();
      if(!dmaBuf){
        Core::Logger::log("Capture path (", stream->label, "): CPU (shared memory)");
      }
    }

    if(modifierFixated && stream->loggedModifier != stream->format.info.raw.modifier){
      stream->loggedModifier = stream->format.info.raw.modifier;
      Core::Logger::log("Capture path (", stream->label, "): GPU (DMA-BUF, modifier 0x", std::hex, stream->format.info.raw.modifier, std::dec, ")");
    }

    stream->pixelWidth = static_cast<int>(stream->format.info.raw.size.width);
    stream->pixelHeight = static_cast<int>(stream->format.info.raw.size.height);
    stream->hasFormat = true;

    // Ready once every monitor has its format
    const bool allFormats = std::all_of(pw->streams.begin(), pw->streams.end(), [](const auto& other){return other->hasFormat;});
    if(allFormats && !pw->promiseSetAlready){
      pw->screenDataReadyPromise.set_value(true);
      pw->promiseSetAlready = true;
    }
  }


  gboolean PipewireGrabber::_onAbortCapture(
    void* userData
  )
  {
    auto* capture = static_cast<XdgDesktopPortal::Capture*>(userData);
    Core::Logger::log("Screen selection aborted");

    // Closes the pending portal request, and with it the dialog
    XdgDesktopPortal::abort(capture);
    return G_SOURCE_REMOVE;
  }


  gboolean PipewireGrabber::_onDestroyCapture(
    void* userData
  )
  {
    auto* capture = static_cast<XdgDesktopPortal::Capture*>(userData);

    // In the portal thread, like every callback of the capture
    XdgDesktopPortal::screencastPortalCaptureDestroy(capture);
    capture->running = false;
    return G_SOURCE_REMOVE;
  }


  void PipewireGrabber::_initCapture(
    XdgDesktopPortal::Capture* capture
  )
  {
    // Replies and signals of the portal calls made here are dispatched to this context only
    g_main_context_push_thread_default(capture->context);

    XdgDesktopPortal::screencastPortalDesktopCaptureCreate(capture, XdgDesktopPortal::CaptureType::Monitor, true);

    // Runs until the grabber stops (see _onDestroyCapture()): the portal dialog may stay open
    // for a long time, and the session is closed from here
    while(capture->running){
      g_main_context_iteration(capture->context, TRUE);
    }

    // Calls cancelled by the teardown complete right away, but through this context: dispatched
    // here, they release their references to it instead of keeping it, and themselves, forever
    while(capture->pendingCalls > 0){
      g_main_context_iteration(capture->context, TRUE);
    }

    g_main_context_pop_thread_default(capture->context);
  }


  bool PipewireGrabber::_startCapture(
    XdgDesktopPortal::Capture* capture,
    PipewireData* pw,
    pw_main_loop* loop
  )
  {
    // Registered as listeners: outlive the core and the streams
    static const pw_core_events coreEvents = [] {
      pw_core_events events = {};
      events.version = PW_VERSION_CORE_EVENTS;
      events.error = _onCoreErrorCallback;
      return events;
    }();

    static const pw_stream_events streamEvents = [] {
      pw_stream_events events = {};
      events.version = PW_VERSION_STREAM_EVENTS;
      events.state_changed = _onStreamStateChanged;
      events.param_changed = _onStreamParamChanged;
      events.process = _onStreamProcess;
      events.add_buffer = _onStreamAddBuffer;
      events.remove_buffer = _onStreamRemoveBuffer;
      return events;
    }();

    // Read by the other threads once the ready promise is set
    pw->loop = loop;
    pw->context = pw_context_new(pw_main_loop_get_loop(loop), NULL, 0);
    pw->renegotiateEvent = pw_loop_add_event(pw_main_loop_get_loop(loop), _onRenegotiate, pw);

    // The GL context lives in this thread, like every Pipewire callback using it
    if(const char* disableGpu = std::getenv("HALOMUX_DISABLE_GPU"); disableGpu && std::string(disableGpu) == "1"){
      Core::Logger::log("[gpu] Disabled by HALOMUX_DISABLE_GPU");
      auto lock = std::lock_guard(pw->processingInfoMutex);
      pw->cpuReason = "GPU disabled by HALOMUX_DISABLE_GPU, shared memory";
    }
    else{
      pw->gpu = std::make_unique<EglDownscaler>();
      if(pw->gpu->init()){
        {
          auto lock = std::lock_guard(pw->processingInfoMutex);
          pw->gpuName = pw->gpu->renderer();
        }

        for(spa_video_format format : SupportedFormats){
          auto modifiers = pw->gpu->supportedModifiers(toDrmFormat(format));
          if(!modifiers.empty()){
            pw->dmaBufFormats.push_back({format, std::move(modifiers)});
          }
        }
      }
      else{
        Core::Logger::warn("[gpu] GPU downscaling unavailable, using the CPU path");
        pw->gpu.reset();
        auto lock = std::lock_guard(pw->processingInfoMutex);
        pw->cpuReason = "GPU unavailable, shared memory";
      }
    }

    pw->core = pw_context_connect_fd(pw->context, fcntl(static_cast<int>(capture->pwFd), F_DUPFD_CLOEXEC, 5), NULL, 0);

    if(pw->core == nullptr){
      Core::Logger::error("[pipewire] Failed to connect to Pipewire");
      if(!pw->promiseSetAlready){
        pw->screenDataReadyPromise.set_value(false);
        pw->promiseSetAlready = true;
      }
      return false;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    pw_core_add_listener(pw->core, &pw->coreListener, &coreEvents, pw);

    // One stream per captured monitor, on the same connection
    for(auto& stream : pw->streams){
      auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Video",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Screen",
        NULL
      );

      const std::string streamName = "HalomuxVideo" + std::to_string(stream->index + 1);
      stream->stream = pw_stream_new(pw->core, streamName.c_str(), props);
      pw_stream_add_listener(stream->stream, &stream->listener, &streamEvents, stream.get());

      std::array<uint8_t, 8192> buffer;
      spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());

      // Unconstrained at first, so the display's actual maximal framerate is learnt
      // (see displayRefreshRate()). setFrameRateLimit() renegotiates it afterwards.
      auto params = _buildFormatParams(&b, stream.get());

      pw_stream_connect(stream->stream, PW_DIRECTION_INPUT, stream->node, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params.data(), static_cast<uint32_t>(params.size()));
    }
#pragma GCC diagnostic pop

    return true;
  }


  void PipewireGrabber::_stopCapture(
    PipewireData* pw
  )
  {
    // Releases the buffers, and with them the imported DMA-BUFs, while the GL context still exists
    // Disconnecting isn't a monitor lost while capturing
    pw->stopping = true;
    for(auto& stream : pw->streams){
      if(stream->stream){
        pw_stream_disconnect(stream->stream);
        spa_hook_remove(&stream->listener);
        g_clear_pointer(&stream->stream, pw_stream_destroy);
      }
    }

    pw->gpu.reset();

    if(pw->renegotiateEvent){
      pw_loop_destroy_source(pw_main_loop_get_loop(pw->loop), pw->renegotiateEvent);
      pw->renegotiateEvent = nullptr;
    }

    if(pw->core){
      spa_hook_remove(&pw->coreListener);
      pw_core_disconnect(pw->core);
      pw->core = nullptr;
    }

    g_clear_pointer(&pw->context, pw_context_destroy);
  }


// The SPA builder macros rely on C compound literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  std::vector<const spa_pod*> PipewireGrabber::_buildFormatParams(
    spa_pod_builder* builder,
    const StreamData* stream
  )
  {
    const PipewireData* pw = stream->owner;
    std::vector<const spa_pod*> params;

    if(!stream->dmaBufFailed){
      for(const auto& dmaBufFormat : pw->dmaBufFormats){
        params.push_back(_buildFormatParam(builder, {dmaBufFormat.format}, dmaBufFormat.modifiers, pw->frameRateLimit));
      }
    }

    params.push_back(_buildFormatParam(builder, SupportedFormats, {}, pw->frameRateLimit));

    return params;
  }


  const spa_pod* PipewireGrabber::_buildFormatParam(
    spa_pod_builder* builder,
    const std::vector<spa_video_format>& formats,
    const std::vector<uint64_t>& modifiers,
    unsigned maxFramerate
  )
  {
    auto r1 = spa_rectangle(320, 240);
    auto r2 = spa_rectangle(1, 1);
    auto r3 = spa_rectangle(8192, 8192);

    auto f1 = spa_fraction(25, 1);
    auto f2 = spa_fraction(0, 1);
    auto f3 = spa_fraction(1000, 1);

    spa_pod_frame objectFrame;
    spa_pod_builder_push_object(builder, &objectFrame, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat);

    spa_pod_builder_add(
      builder,
      SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
      SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
      0
    );

    // Choice of formats, the first value being the preferred (default) one
    spa_pod_frame choiceFrame;
    spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_format, 0);
    spa_pod_builder_push_choice(builder, &choiceFrame, SPA_CHOICE_Enum, 0);
    spa_pod_builder_id(builder, formats.front());
    for(spa_video_format format : formats){
      spa_pod_builder_id(builder, format);
    }
    spa_pod_builder_pop(builder, &choiceFrame);

    if(!modifiers.empty()){
      // The compositor picks the modifier it can allocate
      spa_pod_builder_prop(builder, SPA_FORMAT_VIDEO_modifier, SPA_POD_PROP_FLAG_MANDATORY | SPA_POD_PROP_FLAG_DONT_FIXATE);
      spa_pod_builder_push_choice(builder, &choiceFrame, SPA_CHOICE_Enum, 0);
      spa_pod_builder_long(builder, static_cast<int64_t>(modifiers.front()));
      for(uint64_t modifier : modifiers){
        spa_pod_builder_long(builder, static_cast<int64_t>(modifier));
      }
      spa_pod_builder_pop(builder, &choiceFrame);
    }

    spa_pod_builder_add(
      builder,
      SPA_FORMAT_VIDEO_size,      SPA_POD_CHOICE_RANGE_Rectangle(&r1, &r2, &r3),
      SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&f1, &f2, &f3),
      0
    );

    if(maxFramerate > 0){
      // Variable framerate streams (KWin) send at most maxFramerate frames per second,
      // still delivering the latest content once the minimal frame time has elapsed.
      auto maxDefault = spa_fraction(maxFramerate, 1);
      auto maxMin = spa_fraction(1, 1);
      auto maxMax = spa_fraction(maxFramerate, 1);

      spa_pod_builder_add(
        builder,
        SPA_FORMAT_VIDEO_maxFramerate, SPA_POD_CHOICE_RANGE_Fraction(&maxDefault, &maxMin, &maxMax),
        0
      );
    }

    return static_cast<const spa_pod*>(spa_pod_builder_pop(builder, &objectFrame));
  }


  int PipewireGrabber::_updateFormatParams(
    StreamData* stream
  )
  {
    if(!stream->stream){
      return 0;
    }

    std::array<uint8_t, 8192> buffer;
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
    auto params = _buildFormatParams(&b, stream);

    return pw_stream_update_params(stream->stream, params.data(), static_cast<uint32_t>(params.size()));
  }


  void PipewireGrabber::_updateStreamActivity(
    PipewireData* pw
  )
  {
    for(auto& stream : pw->streams){
      bool hasRegions;
      {
        auto lock = std::lock_guard(pw->channelsMutex);
        hasRegions = !stream->regions.empty();

        if(pw->paused || !hasRegions){
          // Don't hand out the colors of a frame captured before stopping
          stream->hasColors = false;
        }
      }

      const bool streaming = hasRegions && !pw->paused;
      if(streaming != stream->streaming && stream->stream){
        pw_stream_set_active(stream->stream, streaming);
      }
      stream->streaming = streaming;
    }
  }


  int PipewireGrabber::_referencePixelWidth(
    const PipewireData* pw
  )
  {
    int width = 0;
    for(const auto& stream : pw->streams){
      width = std::max(width, static_cast<int>(stream->format.info.raw.size.width));
    }

    return width;
  }


  void PipewireGrabber::_onRenegotiate(
    void* userData,
    uint64_t /*count*/
  )
  {
    // Runs in the Pipewire thread, outside of the process callbacks. Only the streams that failed:
    // the others would glitch for nothing.
    for(auto& stream : static_cast<PipewireData*>(userData)->streams){
      if(std::exchange(stream->renegotiate, false)){
        _updateFormatParams(stream.get());
      }
    }
  }


  int PipewireGrabber::_onUpdateFrameRateLimit(
    spa_loop* /*loop*/,
    bool /*async*/,
    uint32_t /*seq*/,
    const void* data,
    size_t /*size*/,
    void* userData
  )
  {
    // Runs in the Pipewire thread
    PipewireData* pw = static_cast<PipewireData*>(userData);
    pw->frameRateLimit = *static_cast<const unsigned*>(data);

    int result = 0;
    for(auto& stream : pw->streams){
      if(int streamResult = _updateFormatParams(stream.get()); streamResult < 0){
        result = streamResult;
      }
    }

    return result;
  }


  int PipewireGrabber::_onUpdatePaused(
    spa_loop* /*loop*/,
    bool /*async*/,
    uint32_t /*seq*/,
    const void* data,
    size_t /*size*/,
    void* userData
  )
  {
    // Runs in the Pipewire thread, so no frame is being processed meanwhile
    PipewireData* pw = static_cast<PipewireData*>(userData);
    pw->paused = *static_cast<const bool*>(data);
    _updateStreamActivity(pw);

    return 0;
  }


  int PipewireGrabber::_onUpdateRegions(
    spa_loop* /*loop*/,
    bool /*async*/,
    uint32_t /*seq*/,
    const void* /*data*/,
    size_t /*size*/,
    void* userData
  )
  {
    // Runs in the Pipewire thread: monitors without regions stop sending frames
    _updateStreamActivity(static_cast<PipewireData*>(userData));

    return 0;
  }


#pragma GCC diagnostic pop


  void PipewireGrabber::setFrameRateLimit(
    RefreshRate frameRate
  )
  {
    if(!m_pwData.loop){
      return;
    }

    unsigned maxFramerate = frameRate;
    pw_loop_invoke(pw_main_loop_get_loop(m_pwData.loop), _onUpdateFrameRateLimit, 0, &maxFramerate, sizeof(maxFramerate), false, &m_pwData);
  }


  void PipewireGrabber::setPaused(
    bool paused
  )
  {
    if(!m_pwData.loop){
      return;
    }

    pw_loop_invoke(pw_main_loop_get_loop(m_pwData.loop), _onUpdatePaused, 0, &paused, sizeof(paused), false, &m_pwData);
  }


  void PipewireGrabber::_teardownPipewire()
  {
    // Quits the loop safely and waits for its thread, which stops the streams
    m_loopThread.reset();

    if(m_capture.pwFd > 0){
      close(static_cast<int>(m_capture.pwFd));
      m_capture.pwFd = 0;
    }
  }


  void PipewireGrabber::_stop()
  {
    // Stop Pipewire session
    _teardownPipewire();

    // Stop XdgPortal: closed in its thread, which then ends
    if(m_xdgThread.has_value()){
      GSource* destroySource = g_idle_source_new();
      g_source_set_callback(destroySource, _onDestroyCapture, &m_capture, nullptr);
      g_source_attach(destroySource, m_capture.context);
      g_source_unref(destroySource);

      m_xdgThread.value().join();
      m_xdgThread.reset();
    }

    if(m_capture.context){
      g_main_context_unref(m_capture.context);
      m_capture.context = nullptr;
    }
  }
}
