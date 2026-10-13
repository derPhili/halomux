#pragma once

#include <atomic>

#include <Huenicorn/Grabber/IGrabber.hpp>

#include <mutex>
#include <optional>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#pragma GCC diagnostic pop

#include <Huenicorn/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp>


namespace Huenicorn::Grabber
{
  /**
   * Pipewire implementation of screen grabber instanciated for Wayland support
  */
  class PipewireGrabber : public IGrabber
  {
  private:
    /**
     * Thread-safe double buffer for grabbed frames
    */
    struct SafeDoubleBuffer
    {
      std::array<Imaging::ImageData, 2> frame;
      std::mutex mutex;
    };


    /**
     * Pipewire-specific data shared between processes
    */
    /**
     * @brief Read-only mapping of a Pipewire MemFd buffer, attached to the buffer's user_data
     *
     */
    struct BufferMapping
    {
      void* map;
      size_t size;
      const uint8_t* data;
    };


    struct PipewireData
    {
      spa_hook coreListener;
      pw_main_loop* loop{nullptr};
      pw_context* context{nullptr};
      pw_stream* stream{nullptr};
      spa_video_info format;
      SafeDoubleBuffer frameDoubleBuffer;
      std::promise<bool> screenDataReadyPromise;
      bool promiseSetAlready{false};
      Core::Config* config;

      // Highest framerate the compositor offered (written by the Pipewire thread)
      std::atomic<unsigned> displayMaxFramerate{0};

      // Gamescope direct-capture support: gamescope exposes its composited
      // output as a plain (non-portal-gated) Pipewire node named "gamescope".
      // xdg-desktop-portal's ScreenCast implementation isn't wired up inside
      // a gamescope session, which is why the portal path black-screens there.
      bool useGamescope{false};
      bool discoveryMode{false};
      int discoverySyncSeq{0};
      uint32_t gamescopeNodeId{0};
    };


  public:
    // Constructor / destructor
    /**
     * PipewireGrabber constructor
    */
    PipewireGrabber(
      Core::Config* config,
      bool useGamescope = false
    );


    /**
     * PipewireGrabber destructor
    */
    virtual ~PipewireGrabber();


    virtual const std::string& name() const override;

    /**
     * @brief Returns the resolution of the selected display
     * 
     * @return Resolution Resolution of the selected display
     */
    virtual Resolution displayResolution() const override;


    /**
     * @brief Returns the resolution of the selected display
     * 
     * @return Resolution Resolution of the selected display
     */
    virtual RefreshRate displayRefreshRate() const override;


    // Methods
    /**
     * @brief Returns a subsample of the last captured frame
     * 
     * @param imageData Subsample of the last captured frame
     */
    virtual void grabFrameSubsample(
      Imaging::ImageData& imageData
    ) override;


    /**
     * @brief Renegotiates the stream so the compositor sends at most frameRate frames per second
     *
     * @param frameRate Maximal framerate, 0 to remove the limit
     */
    virtual void setFrameRateLimit(
      RefreshRate frameRate
    ) override;


  private:
    // Methods
    /**
     * @brief Builds the EnumFormat parameter of the stream
     *
     * @param builder Builder to write the parameter with
     * @param maxFramerate Maximal framerate to request, 0 for none
     * @return const spa_pod* Built parameter
     */
    static const spa_pod* _buildFormatParam(
      spa_pod_builder* builder,
      unsigned maxFramerate
    );

    static int _onUpdateFrameRateLimit(
      spa_loop* loop,
      bool async,
      uint32_t seq,
      const void* data,
      size_t size,
      void* userData
    );

    static void _onCoreInfoCallback(
      void* userData,
      const pw_core_info* info
    );

    static void _onCoreDoneCallback(
      void* userData,
      uint32_t id,
      int seq
    );

    static void _onCoreErrorCallback(
      void* userData,
      uint32_t id,
      int seq,
      int res,
      const char* message
    );

    static void _onStreamProcess(
      void* userdata
    );

    static void _onStreamAddBuffer(
      void* userdata,
      pw_buffer* buffer
    );

    static void _onStreamRemoveBuffer(
      void* userdata,
      pw_buffer* buffer
    );

    static void _onStreamParamChanged(
      void* userdata,
      uint32_t id,
      const spa_pod* param
    );

    /**
     * Registry listener used to find gamescope's "gamescope" Pipewire node
    */
    static void _onRegistryGlobal(
      void* userdata,
      uint32_t id,
      uint32_t permissions,
      const char* type,
      uint32_t version,
      const spa_dict* props
    );

    /**
     * Screencast portal thread
    */
    static void _initCapture(
      XdgDesktopPortal::Capture* capture
    );


    /**
     * Pipewire thread
    */
    static void _pipewireThread(
      XdgDesktopPortal::Capture* capture,
      PipewireData* pw
    );


    /**
     * Proper shutdown for Pipewire session
    */
    void _teardownPipewire();


    /**
     * Terminates all inner executions
    */
    void _stop();


    // Attributes
    std::optional<std::thread> m_xdgThread;
    std::optional<std::thread> m_pipewireThread;
    XdgDesktopPortal::Capture m_capture;
    PipewireData m_pwData;
  };
}
