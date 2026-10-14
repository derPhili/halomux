#include <Huenicorn/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>

#include <array>
#include <cstdlib>
#include <sstream>
#include <future>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <Huenicorn/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp>
#include <Huenicorn/Imaging/ImageProcessing.hpp>
#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Core/Logger.hpp>

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


namespace Huenicorn::Grabber
{
  PipewireGrabber::PipewireGrabber(
    Core::Config* config
  ):
  IGrabber(config)
  {
    m_pwData.config = config;
    m_capture.config = config;

    std::promise<bool> fdReadyPromise;
    auto fdReadyFuture = fdReadyPromise.get_future();
    m_capture.fdReadyPromise = std::move(fdReadyPromise);
    m_xdgThread.emplace(_initCapture, &m_capture);
    fdReadyFuture.wait();

    if(!fdReadyFuture.get()){
      _stop();
      throw Grabber::GrabberCancelled("Failed to get monitor file descriptor");
    }

    auto configDataReadyFuture = m_pwData.screenDataReadyPromise.get_future();
    m_pipewireThread.emplace(_pipewireThread, &m_capture, &m_pwData);
    configDataReadyFuture.wait();

    if(!configDataReadyFuture.get()){
      _stop();
      throw Grabber::GrabberUnavailable("Failed to initialize Pipewire capture");
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
    return {m_pwData.format.info.raw.size.width, m_pwData.format.info.raw.size.height};
  }


  IGrabber::RefreshRate PipewireGrabber::displayRefreshRate() const
  {
    // Highest framerate offered by the compositor, regardless of the limit requested
    // through setFrameRateLimit()
    if(unsigned displayMaxFramerate = m_pwData.displayMaxFramerate; displayMaxFramerate > 0){
      return displayMaxFramerate;
    }

    static constexpr IGrabber::RefreshRate FallbackRefreshRate = 60;
    return FallbackRefreshRate;
  }


  void PipewireGrabber::setChannelRegions(
    const ChannelRegions& regions
  )
  {
    auto lock = std::lock_guard(m_pwData.channels.mutex);
    m_pwData.channels.regions = regions;
  }


  bool PipewireGrabber::grabChannelColors(
    ChannelColors& colors
  )
  {
    auto lock = std::lock_guard(m_pwData.channels.mutex);
    colors = m_pwData.channels.colors;
    return m_pwData.channels.hasColors;
  }


  void PipewireGrabber::_onCoreInfoCallback(
    void* userData,
    const pw_core_info* info
  )
  {
    (void)userData;
    (void)info;
    //PipewireData* pw = static_cast<PipewireData*>(userData);
    //update_pw_versions(pw, info->version);
    //Logger::debug("Pipewire version : ", std::string(info->version));
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
    //PipewireData* pw = static_cast<PipewireData*>(userData);

    std::stringstream ss;
    ss << "[pipewire] Error id: " << id << " seq: " << seq << " res: " << res << "" << g_strerror(res) << " " << message << std::endl;
    Core::Logger::error(ss.str());

    //pw_thread_loop_signal(pw->loop, FALSE);
  }


  void PipewireGrabber::_onStreamProcess(
    void* userdata
  )
  {
    PipewireData* pw = static_cast<PipewireData*>(userdata);
    pw_buffer* pwBuffer;

    if((pwBuffer = pw_stream_dequeue_buffer(pw->stream)) == NULL){
      //pw_log_warn("out of buffers");
      return;
    }

    const auto width = static_cast<int>(pw->format.info.raw.size.width);
    const auto height = static_cast<int>(pw->format.info.raw.size.height);

    // Only 4 bytes per pixel formats are negotiated (see SupportedFormats)
    Imaging::PixelFormat pixelFormat;
    switch(pw->format.info.raw.format){
      case SPA_VIDEO_FORMAT_BGRx:
      case SPA_VIDEO_FORMAT_BGRA:
        pixelFormat = Imaging::PixelFormat::BGRA;
        break;

      case SPA_VIDEO_FORMAT_RGBx:
      case SPA_VIDEO_FORMAT_RGBA:
        pixelFormat = Imaging::PixelFormat::RGBA;
        break;

      default:
        pw_stream_queue_buffer(pw->stream, pwBuffer);
        return;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];
    const bool corrupted = data.chunk != nullptr && (data.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED);

    if(width == 0 || height == 0 || corrupted){
      pw_stream_queue_buffer(pw->stream, pwBuffer);
      return;
    }

    ChannelRegions regions;
    {
      auto lock = std::lock_guard(pw->channels.mutex);
      regions = pw->channels.regions;
    }

    ChannelColors colors;
    const bool computed = data.type == SPA_DATA_DmaBuf
      ? _processDmaBuf(pw, pwBuffer, width, height, regions, colors)
      : _processSharedMemory(pw, pwBuffer, width, height, pixelFormat, regions, colors);

    if(computed){
      auto lock = std::lock_guard(pw->channels.mutex);
      pw->channels.colors = std::move(colors);
      pw->channels.hasColors = true;
    }

    pw_stream_queue_buffer(pw->stream, pwBuffer);
  }


  bool PipewireGrabber::_processDmaBuf(
    PipewireData* pw,
    pw_buffer* pwBuffer,
    int width,
    int height,
    const ChannelRegions& regions,
    ChannelColors& colors
  )
  {
    if(!pw->gpu || pw->dmaBufFailed){
      return false;
    }

    const spa_data& data = pwBuffer->buffer->datas[0];
    if(data.chunk == nullptr || data.fd < 0){
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
        .fd = static_cast<int>(data.fd),
        .offset = data.chunk->offset,
        .stride = static_cast<uint32_t>(data.chunk->stride),
        .width = width,
        .height = height,
        .drmFormat = toDrmFormat(pw->format.info.raw.format),
        .modifier = pw->format.info.raw.modifier
      });
    }

    const int subsampleWidth = std::min(static_cast<int>(pw->config->subsampleWidth()), width);
    const int subsampleHeight = std::max(1, static_cast<int>(static_cast<int64_t>(height) * subsampleWidth / width));

    // Regions are taken from the subsample, exactly like on the CPU path
    std::vector<cv::Rect> rects;
    rects.reserve(regions.size());
    for(const auto& [channelId, uvs] : regions){
      rects.push_back(Imaging::ImageProcessing::subImageRect(subsampleWidth, subsampleHeight, uvs));
    }

    std::vector<cv::Vec4b> rgbaColors;
    if(!bufferData->gpuImage || !pw->gpu->averageRegions(*bufferData->gpuImage, subsampleWidth, subsampleHeight, rects, rgbaColors)){
      // Fall back to shared memory buffers for good
      Core::Logger::warn("[gpu] GPU downscaling failed, falling back to the CPU path");
      pw->dmaBufFailed = true;
      pw_loop_signal_event(pw_main_loop_get_loop(pw->loop), pw->renegotiateEvent);
      return false;
    }

    colors.clear();
    colors.reserve(regions.size());
    for(size_t i = 0; i < regions.size(); i++){
      const cv::Vec4b& rgba = rgbaColors[i];
      colors.emplace_back(regions[i].first, Imaging::Color(rgba[0], rgba[1], rgba[2]));
    }

    return true;
  }


  bool PipewireGrabber::_processSharedMemory(
    PipewireData* pw,
    pw_buffer* pwBuffer,
    int width,
    int height,
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
    const int subsampleWidth = static_cast<int>(pw->config->subsampleWidth());

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
      uvs.push_back(region.second);
    }

    const auto regionColors = Imaging::ImageProcessing::averageRegions(subsample, uvs);

    colors.clear();
    colors.reserve(regions.size());
    for(size_t i = 0; i < regions.size(); i++){
      colors.emplace_back(regions[i].first, regionColors[i]);
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
    PipewireData* pw = static_cast<PipewireData*>(userdata);
    auto* bufferData = static_cast<BufferData*>(buffer->user_data);
    if(!bufferData){
      return;
    }

    if(bufferData->map){
      munmap(bufferData->map, bufferData->mapSize);
    }

    if(bufferData->gpuImage && pw->gpu){
      pw->gpu->releaseImage(*bufferData->gpuImage);
    }

    delete bufferData;
    buffer->user_data = nullptr;
  }


  void PipewireGrabber::_onStreamParamChanged(
    void* userdata,
    uint32_t id,
    const spa_pod* param
  )
  {
    //Logger::log("Params changed !");
    PipewireData* pw = static_cast<PipewireData*>(userdata);

    if(param == NULL || id != SPA_PARAM_Format){
      return;
    }

    if(spa_format_parse(param, &pw->format.media_type, &pw->format.media_subtype) < 0){
      return;
    }

    if(pw->format.media_type != SPA_MEDIA_TYPE_video || pw->format.media_subtype != SPA_MEDIA_SUBTYPE_raw){
      return;
    }

    if(spa_format_video_raw_parse(param, &pw->format.info.raw) < 0){
      return;
    }

    // Variable framerate streams advertise their bound in max_framerate, fixed ones in framerate.
    // Keep the highest value seen: later negotiations are limited by setFrameRateLimit().
    for(const auto& fraction : {pw->format.info.raw.max_framerate, pw->format.info.raw.framerate}){
      if(fraction.num > 0 && fraction.denom > 0){
        unsigned framerate = fraction.num / fraction.denom;
        if(framerate > pw->displayMaxFramerate){
          pw->displayMaxFramerate = framerate;
        }
        break;
      }
    }

    /*
    std::stringstream ss;
    Logger::debug("got video format:");

    ss.str("  format: ");
    ss << pw->format.info.raw.format << spa_debug_type_find_name(spa_type_video_format, pw->format.info.raw.format);
    Logger::debug(ss.str());

    ss.str("  size: ");
    ss << pw->format.info.raw.size.width << "x" << pw->format.info.raw.size.height;
    Logger::debug(ss.str());

    ss.str("  framerate: ");
    ss << pw->format.info.raw.framerate.num << "/" << pw->format.info.raw.framerate.denom;
    Logger::debug(ss.str());
    */


    // DMA-BUF formats carry a modifier: ask for the matching buffer type
    const bool dmaBuf = spa_pod_find_prop(param, nullptr, SPA_FORMAT_VIDEO_modifier) != nullptr;
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
    pw_stream_update_params(pw->stream, &buffersParam, 1);

    if(dmaBuf != pw->usingDmaBuf || !pw->promiseSetAlready){
      pw->usingDmaBuf = dmaBuf;
      if(dmaBuf){
        Core::Logger::log("Capture path: GPU (DMA-BUF, modifier 0x", std::hex, pw->format.info.raw.modifier, std::dec, ")");
      }
      else{
        Core::Logger::log("Capture path: CPU (shared memory)");
      }
    }

    if(!pw->promiseSetAlready){
      pw->screenDataReadyPromise.set_value(true);
      pw->promiseSetAlready = true;
    }
  }


  void PipewireGrabber::_initCapture(
    XdgDesktopPortal::Capture* capture
  )
  {
    XdgDesktopPortal::screencastPortalDesktopCaptureCreate(capture, XdgDesktopPortal::CaptureType::Monitor, true);

    GMainLoop* gmain = g_main_loop_new(NULL, FALSE);

    // Block until an event is dispatched: the portal dialog may stay open for a long time.
    // _stop() wakes the context up after clearing updateXdgContext.
    while(capture->updateXdgContext){
      g_main_context_iteration(g_main_loop_get_context(gmain), TRUE);
    }

    g_main_loop_unref(gmain);
  }


  void PipewireGrabber::_pipewireThread(
    XdgDesktopPortal::Capture* capture,
    PipewireData* pw
  )
  {
    //capture->updateXdgContext = false;

    pw_init(NULL, NULL);
    pw_core_events coreEvents = {};
    coreEvents.version = PW_VERSION_CORE_EVENTS;
    coreEvents.info = _onCoreInfoCallback;
    coreEvents.error = _onCoreErrorCallback;

    pw_stream_events streamEvents = {};
    streamEvents.version = PW_VERSION_STREAM_EVENTS;
    streamEvents.param_changed = _onStreamParamChanged;
    streamEvents.process = _onStreamProcess;
    streamEvents.add_buffer = _onStreamAddBuffer;
    streamEvents.remove_buffer = _onStreamRemoveBuffer;

    pw->loop = pw_main_loop_new(NULL);
    pw->context = pw_context_new(pw_main_loop_get_loop(pw->loop), NULL, 0);
    pw->renegotiateEvent = pw_loop_add_event(pw_main_loop_get_loop(pw->loop), _onRenegotiate, pw);

    // The GL context lives in this thread, like every Pipewire callback using it
    if(const char* disableGpu = std::getenv("HUENICORN_DISABLE_GPU"); disableGpu && std::string(disableGpu) == "1"){
      Core::Logger::log("[gpu] Disabled by HUENICORN_DISABLE_GPU");
    }
    else{
      pw->gpu = std::make_unique<EglDownscaler>();
      if(pw->gpu->init()){
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
      }
    }

    auto core = pw_context_connect_fd(pw->context, fcntl(static_cast<int>(capture->pwFd), F_DUPFD_CLOEXEC, 5), NULL, 0);

    if(core == nullptr){
      Core::Logger::error("[pipewire] Failed to connect to Pipewire");
      if(!pw->promiseSetAlready){
        pw->screenDataReadyPromise.set_value(false);
        pw->promiseSetAlready = true;
      }
      pw_context_destroy(pw->context);
      pw->context = nullptr;
      pw_main_loop_destroy(pw->loop);
      pw->loop = nullptr;
      return;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    pw_core_add_listener(core, &pw->coreListener, &coreEvents, pw);

    uint32_t targetNode = capture->pwNode;

    auto props = pw_properties_new(
      PW_KEY_MEDIA_TYPE, "Video",
      PW_KEY_MEDIA_CATEGORY, "Capture",
      PW_KEY_MEDIA_ROLE, "Screen",
      NULL
    );

    std::string streamName = "HuenicornStream";

    pw->stream = pw_stream_new_simple(
      pw_main_loop_get_loop(pw->loop),
      streamName.c_str(),
      props,
      &streamEvents,
      pw
    );

    std::array<uint8_t, 8192> buffer;
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());

    // Unconstrained at first, so the display's actual maximal framerate is learnt
    // (see displayRefreshRate()). setFrameRateLimit() renegotiates it afterwards.
    auto params = _buildFormatParams(&b, pw);
#pragma GCC diagnostic pop

    pw_stream_connect(pw->stream, PW_DIRECTION_INPUT, targetNode, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params.data(), static_cast<uint32_t>(params.size()));

    pw_main_loop_run(pw->loop);

    pw_stream_disconnect(pw->stream);

    // Releases the buffers, and with them the imported DMA-BUFs, while the GL context still exists
    g_clear_pointer(&pw->stream, pw_stream_destroy);
    pw->gpu.reset();
    pw_loop_destroy_source(pw_main_loop_get_loop(pw->loop), pw->renegotiateEvent);
    pw->renegotiateEvent = nullptr;

    g_clear_pointer(&pw->context, pw_context_destroy);
    g_clear_pointer(&pw->loop, pw_main_loop_destroy);
  }


// The SPA builder macros rely on C compound literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  std::vector<const spa_pod*> PipewireGrabber::_buildFormatParams(
    spa_pod_builder* builder,
    const PipewireData* pw
  )
  {
    std::vector<const spa_pod*> params;

    if(!pw->dmaBufFailed){
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
    PipewireData* pw
  )
  {
    if(!pw->stream){
      return 0;
    }

    std::array<uint8_t, 8192> buffer;
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
    auto params = _buildFormatParams(&b, pw);

    return pw_stream_update_params(pw->stream, params.data(), static_cast<uint32_t>(params.size()));
  }


  void PipewireGrabber::_onRenegotiate(
    void* userData,
    uint64_t /*count*/
  )
  {
    // Runs in the Pipewire thread, outside of the process callback
    _updateFormatParams(static_cast<PipewireData*>(userData));
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

    return _updateFormatParams(pw);
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


  void PipewireGrabber::_teardownPipewire()
  {
    if(m_pwData.loop){
      // Quit the loop
      pw_main_loop_quit(m_pwData.loop);
    }

    // Waiting for thread to finish
    if(m_pipewireThread.has_value()){
      m_pipewireThread.value().join();
      m_pipewireThread.reset();
    }

    if(m_capture.pwFd > 0){
      close(static_cast<int>(m_capture.pwFd));
      m_capture.pwFd = 0;
    }

    pw_deinit();
  }


  void PipewireGrabber::_stop()
  {
    // Stop Pipewire session
    _teardownPipewire();

    // Stop XdgPortal
    m_capture.updateXdgContext = false; // Making sure
    g_main_context_wakeup(nullptr);
    XdgDesktopPortal::screencastPortalCaptureDestroy(&m_capture);
    if(m_xdgThread.has_value()){
      m_xdgThread.value().join();
      m_xdgThread.reset();
    }

    // Waiting for thread to finish
    if(m_pipewireThread.has_value()){
      m_pipewireThread.value().join();
      m_pipewireThread.reset();
    }
  }
}
