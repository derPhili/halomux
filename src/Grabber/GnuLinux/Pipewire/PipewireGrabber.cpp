#include <Huenicorn/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>

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

#if defined(__clang__)
  #pragma clang diagnostic pop
#elif defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif



using namespace std::chrono_literals;


namespace Huenicorn::Grabber
{
  PipewireGrabber::PipewireGrabber(
    Core::Config* config,
    bool useGamescope
  ):
  IGrabber(config)
  {
    m_pwData.config = config;
    m_capture.config = config;
    m_pwData.useGamescope = useGamescope;

    if(useGamescope){
      // Gamescope sessions don't run an xdg-desktop-portal ScreenCast backend,
      // so there's no session/fd to negotiate. The Pipewire thread connects
      // directly to the local socket and finds gamescope's node itself.
      Core::Logger::log("Detected gamescope session. Capturing gamescope's Pipewire node directly instead of using the XDG portal.");
    }
    else{
      std::promise<bool> fdReadyPromise;
      auto fdReadyFuture = fdReadyPromise.get_future();
      m_capture.fdReadyPromise = std::move(fdReadyPromise);
      m_xdgThread.emplace(_initCapture, &m_capture);
      fdReadyFuture.wait();

      if(!fdReadyFuture.get()){
        _stop();
        throw Grabber::GrabberCancelled("Failed to get monitor file descriptor");
      }
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
    // Variable framerate streams advertise their bound in max_framerate, fixed ones in framerate
    for(const auto& fraction : {m_pwData.format.info.raw.max_framerate, m_pwData.format.info.raw.framerate}){
      if(fraction.num > 0 && fraction.denom > 0){
        return static_cast<IGrabber::RefreshRate>(fraction.num / fraction.denom);
      }
    }

    static constexpr IGrabber::RefreshRate FallbackRefreshRate = 60;
    return FallbackRefreshRate;
  }


  void PipewireGrabber::grabFrameSubsample(
    Imaging::ImageData& imageData
  )
  {
    auto lock = std::lock_guard(m_pwData.frameDoubleBuffer.mutex);
    imageData = m_pwData.frameDoubleBuffer.frame.at(0);
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


  void PipewireGrabber::_onCoreDoneCallback(
    void* userData,
    uint32_t id,
    int seq
  )
  {
    PipewireData* pw = static_cast<PipewireData*>(userData);

    // Gamescope node discovery: the registry has advertised all currently
    // available globals by the time this sync's "done" fires, so it's safe
    // to stop the loop and check whether "gamescope" showed up.
    if(pw->discoveryMode && id == PW_ID_CORE && seq == pw->discoverySyncSeq){
      pw_main_loop_quit(pw->loop);
    }
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

    spa_buffer* spaBuffer = pwBuffer->buffer;
    if(spaBuffer->datas[0].data == NULL){
      pw_stream_queue_buffer(pw->stream, pwBuffer);
      return;
    }

    const auto width = static_cast<int>(pw->format.info.raw.size.width);
    const auto height = static_cast<int>(pw->format.info.raw.size.height);

    // Only 4 bytes per pixel formats are negotiated (see _pipewireThread)
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

    if(width == 0 || height == 0){
      pw_stream_queue_buffer(pw->stream, pwBuffer);
      return;
    }

    auto* chunk = spaBuffer->datas[0].chunk;

    if(chunk == nullptr || chunk->size == 0){
      pw_stream_queue_buffer(pw->stream, pwBuffer);
      return;
    }

    const size_t step = chunk->stride > 0
      ? static_cast<size_t>(chunk->stride)
      : static_cast<size_t>(width) * 4;

    // Some xdg-desktop-portal / pipewire combinations advertise SPA_DATA_MemFd
    // buffers without auto-mapping them with PROT_READ even when
    // PW_STREAM_FLAG_MAP_BUFFERS is set. Reading via the provided
    // datas[0].data pointer then segfaults. Map the fd ourselves for the
    // duration of this frame as a defensive fallback.
    void* readPtr = spaBuffer->datas[0].data;
    void* localMap = MAP_FAILED;
    size_t localMapSize = 0;
    const bool needRemap = spaBuffer->datas[0].type == SPA_DATA_MemFd
      && spaBuffer->datas[0].fd >= 0;

    if(needRemap){
      // The buffer data starts at mapoffset within the fd
      const size_t mapOffset = spaBuffer->datas[0].mapoffset;
      localMapSize = mapOffset + static_cast<size_t>(spaBuffer->datas[0].maxsize);
      localMap = mmap(nullptr, localMapSize, PROT_READ, MAP_SHARED,
                      static_cast<int>(spaBuffer->datas[0].fd), 0);
      if(localMap != MAP_FAILED){
        readPtr = static_cast<uint8_t*>(localMap) + mapOffset;
      }
    }

    cv::Mat rgbaFrame(height, width, CV_8UC4, static_cast<uint8_t*>(readPtr) + chunk->offset, step);

    // CRITICAL:
    // Pipewire memory becomes invalid after queue_buffer()
    cv::Mat ownedFrame = rgbaFrame.clone();

    if(localMap != MAP_FAILED){
      munmap(localMap, localMapSize);
    }

    Imaging::ImageData capturedFrame{
      .imageMatrix = cv::Mat(ownedFrame),
      .format = pixelFormat,
    };

    {
      auto lock = std::lock_guard(pw->frameDoubleBuffer.mutex);
      std::swap(pw->frameDoubleBuffer.frame[0], pw->frameDoubleBuffer.frame[1]);
      pw->frameDoubleBuffer.frame[0] = std::move(capturedFrame);
    }
    pw_stream_queue_buffer(pw->stream, pwBuffer);
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


    if(!pw->promiseSetAlready){
      pw->screenDataReadyPromise.set_value(true);
      pw->promiseSetAlready = true;
    }
  }


  void PipewireGrabber::_onRegistryGlobal(
    void* userdata,
    uint32_t id,
    uint32_t /*permissions*/,
    const char* type,
    uint32_t /*version*/,
    const spa_dict* props
  )
  {
    PipewireData* pw = static_cast<PipewireData*>(userdata);

    if(pw->gamescopeNodeId != 0 || props == nullptr){
      return;
    }

    if(std::string(type) != PW_TYPE_INTERFACE_Node){
      return;
    }

    const char* nodeName = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if(nodeName != nullptr && std::string(nodeName) == "gamescope"){
      pw->gamescopeNodeId = id;
    }
  }


  void PipewireGrabber::_initCapture(
    XdgDesktopPortal::Capture* capture
  )
  {
    XdgDesktopPortal::screencastPortalDesktopCaptureCreate(capture, XdgDesktopPortal::CaptureType::Monitor, true);

    GMainLoop* gmain = g_main_loop_new(NULL, FALSE);

    while(capture->updateXdgContext){
      g_main_context_iteration(g_main_loop_get_context(gmain), false);
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
    coreEvents.done = _onCoreDoneCallback;
    coreEvents.error = _onCoreErrorCallback;

    pw_stream_events streamEvents = {};
    streamEvents.version = PW_VERSION_STREAM_EVENTS;
    streamEvents.param_changed = _onStreamParamChanged;
    streamEvents.process = _onStreamProcess;

    pw->loop = pw_main_loop_new(NULL);
    pw->context = pw_context_new(pw_main_loop_get_loop(pw->loop), NULL, 0);

    auto core = pw->useGamescope
      ? pw_context_connect(pw->context, NULL, 0)
      : pw_context_connect_fd(pw->context, fcntl(static_cast<int>(capture->pwFd), F_DUPFD_CLOEXEC, 5), NULL, 0);

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

    if(pw->useGamescope){
      pw_registry_events registryEvents = {};
      registryEvents.version = PW_VERSION_REGISTRY_EVENTS;
      registryEvents.global = _onRegistryGlobal;

      pw_registry* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
      spa_hook registryListener{};
      pw_registry_add_listener(registry, &registryListener, &registryEvents, pw);

      pw->discoveryMode = true;
      pw->discoverySyncSeq = pw_core_sync(core, PW_ID_CORE, 0);
      pw_main_loop_run(pw->loop);
      pw->discoveryMode = false;

      spa_hook_remove(&registryListener);
      pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));

      if(pw->gamescopeNodeId == 0){
        Core::Logger::error("Could not find a Pipewire node named 'gamescope'. This doesn't look like a gamescope session, or gamescope's Pipewire support isn't available.");
        if(!pw->promiseSetAlready){
          pw->screenDataReadyPromise.set_value(false);
          pw->promiseSetAlready = true;
        }
        pw_core_disconnect(core);
        pw_context_destroy(pw->context);
        pw->context = nullptr;
        pw_main_loop_destroy(pw->loop);
        pw->loop = nullptr;
        return;
      }

      targetNode = pw->gamescopeNodeId;
    }

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

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    auto r1 = spa_rectangle(320, 240);
    auto r2 = spa_rectangle(1, 1);
    auto r3 = spa_rectangle(4096, 4096);

    auto f1 = spa_fraction(25, 1);
    auto f2 = spa_fraction(0, 1);
    auto f3 = spa_fraction(1000, 1);

    const spa_pod* params[1] = {
      static_cast<spa_pod*>(
        spa_pod_builder_add_object(
          &b,
          SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
          SPA_FORMAT_mediaType,       SPA_POD_Id(SPA_MEDIA_TYPE_video),
          SPA_FORMAT_mediaSubtype,    SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
          // The frame processing only handles packed 4 bytes per pixel formats.
          // The first value is the preferred (default) one.
          SPA_FORMAT_VIDEO_format,    SPA_POD_CHOICE_ENUM_Id(
            5,
            SPA_VIDEO_FORMAT_BGRx,
            SPA_VIDEO_FORMAT_BGRx,
            SPA_VIDEO_FORMAT_BGRA,
            SPA_VIDEO_FORMAT_RGBx,
            SPA_VIDEO_FORMAT_RGBA
          ),
          SPA_FORMAT_VIDEO_size,
          SPA_POD_CHOICE_RANGE_Rectangle(
            &r1,
            &r2,
            &r3
          ),
          SPA_FORMAT_VIDEO_framerate,
          SPA_POD_CHOICE_RANGE_Fraction(
            &f1,
            &f2,
            &f3
          )
        )
      )
    };
#pragma GCC diagnostic pop

    pw_stream_connect(pw->stream, PW_DIRECTION_INPUT, targetNode, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params, 1);

    pw_main_loop_run(pw->loop);

    pw_stream_disconnect(pw->stream);

    g_clear_pointer(&pw->stream, pw_stream_destroy);

    g_clear_pointer(&pw->context, pw_context_destroy);
    g_clear_pointer(&pw->loop, pw_main_loop_destroy);
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

    // Stop XdgPortal (not used when capturing gamescope's node directly)
    if(!m_pwData.useGamescope){
      m_capture.updateXdgContext = false; // Making sure
      XdgDesktopPortal::screencastPortalCaptureDestroy(&m_capture);
      if(m_xdgThread.has_value()){
        m_xdgThread.value().join();
        m_xdgThread.reset();
      }
    }

    // Waiting for thread to finish
    if(m_pipewireThread.has_value()){
      m_pipewireThread.value().join();
      m_pipewireThread.reset();
    }
  }
}
