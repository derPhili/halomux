#pragma once

#include <atomic>

#include <Halomux/Grabber/IGrabber.hpp>
#include <Halomux/Pipewire/LoopThread.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#pragma GCC diagnostic pop

#include <Halomux/Grabber/GnuLinux/Pipewire/EglDownscaler.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp>


namespace Halomux::Grabber
{
  /**
   * Pipewire implementation of screen grabber instanciated for Wayland support
  */
  class PipewireGrabber : public IGrabber
  {
  private:
    /**
     * @brief Per Pipewire buffer resources, attached to the buffer's user_data
     *
     */
    struct BufferData
    {
      // Read-only mapping of a MemFd buffer (CPU path)
      void* map{nullptr};
      size_t mapSize{0};
      const uint8_t* data{nullptr};

      // Imported DMA-BUF (GPU path), created on first use
      std::unique_ptr<EglDownscaler::Image> gpuImage;
    };


    /**
     * @brief DRM format modifiers the GPU can import for a video format
     *
     */
    struct DmaBufFormat
    {
      spa_video_format format;
      std::vector<uint64_t> modifiers;
    };


    struct PipewireData;


    /**
     * @brief Capture of one monitor
     *
     */
    struct StreamData
    {
      PipewireData* owner{nullptr};
      size_t index{0};
      uint32_t node{0};
      // Position and size given by the portal, if any
      std::optional<MonitorGeometry> portalGeometry;
      // Connector name for the log, or "monitor N"
      std::string label;

      pw_stream* stream{nullptr};
      spa_hook listener{};
      spa_video_info format{};
      bool hasFormat{false};
      // Pixel size of the negotiated format, read by other threads: later negotiations (such as
      // the modifier's fixation) rewrite format meanwhile
      std::atomic<int> pixelWidth{0};
      std::atomic<int> pixelHeight{0};

      // Highest framerate the compositor offered (written by the Pipewire thread)
      std::atomic<unsigned> displayMaxFramerate{0};

      // GPU path (Pipewire thread only)
      bool dmaBufFailed{false};
      bool usingDmaBuf{false};
      // Modifier last logged, once fixated
      std::optional<uint64_t> loggedModifier;
      // Last value given to pw_stream_set_active()
      std::atomic<bool> streaming{true};
      // The stream ended while capturing, such as when the monitor was unplugged
      std::atomic<bool> lost{false};

      // Guarded by PipewireData::channelsMutex
      ChannelRegions regions;
      ChannelColors colors;
      bool hasColors{false};

      // Processing path of the last frame, read by the web UI
      std::atomic<bool> lastFrameOnGpu{false};
      std::string cpuReason; // Guarded by PipewireData::processingInfoMutex
    };


    struct PipewireData
    {
      spa_hook coreListener;
      pw_main_loop* loop{nullptr};
      pw_context* context{nullptr};
      pw_core* core{nullptr};
      std::vector<std::unique_ptr<StreamData>> streams;
      std::promise<bool> screenDataReadyPromise;
      bool promiseSetAlready{false};
      Core::Config* config;

      // Channel regions and their colors, shared between the Pipewire thread and the streaming loop
      mutable std::mutex channelsMutex;

      // Current framerate limit, 0 for none (Pipewire thread)
      unsigned frameRateLimit{0};
      // Whether the lamps don't follow the screen (Pipewire thread)
      bool paused{false};
      // Set when the streams are being disconnected (Pipewire thread)
      bool stopping{false};

      // GPU path (Pipewire thread only), shared by the streams
      std::unique_ptr<EglDownscaler> gpu;
      std::vector<DmaBufFormat> dmaBufFormats;
      spa_source* renegotiateEvent{nullptr};

      mutable std::mutex processingInfoMutex;
      std::string gpuName;
      // Why the CPU is used when the GPU isn't available at all
      std::string cpuReason{"Shared memory"};
    };


  public:
    // Constructor / destructor
    /**
     * PipewireGrabber constructor
    */
    PipewireGrabber(
      Core::Config* config,
      bool askForMonitors = false,
      std::stop_token stopToken = {}
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
    virtual void setChannelRegions(
      const ChannelRegions& regions
    ) override;


    virtual bool grabChannelColors(
      ChannelColors& colors
    ) override;


    virtual ProcessingInfo processingInfo() const override;

    virtual Monitors monitors() const override;


    /**
     * @brief Renegotiates the stream so the compositor sends at most frameRate frames per second
     *
     * @param frameRate Maximal framerate, 0 to remove the limit
     */
    virtual void setFrameRateLimit(
      RefreshRate frameRate
    ) override;


    /**
     * @brief Pauses or resumes the screencast stream
     *
     * @param paused Whether to stop capturing
     */
    virtual void setPaused(
      bool paused
    ) override;


  private:
    // Methods
    /**
     * @brief Builds the EnumFormat parameters of the stream
     *
     * DMA-BUF formats come first (preferred), followed by a shared memory fallback.
     *
     * @param builder Builder to write the parameters with
     * @param stream Stream (its DMA-BUF state, the shared capabilities and framerate limit)
     * @return std::vector<const spa_pod*> Built parameters
     */
    static std::vector<const spa_pod*> _buildFormatParams(
      spa_pod_builder* builder,
      const StreamData* stream
    );


    /**
     * @brief Builds a single EnumFormat parameter
     *
     * @param builder Builder to write the parameter with
     * @param formats Video formats to offer, the first one being preferred
     * @param modifiers DRM modifiers to offer (DMA-BUF), empty for shared memory
     * @param maxFramerate Maximal framerate to request, 0 for none
     * @return const spa_pod* Built parameter
     */
    static const spa_pod* _buildFormatParam(
      spa_pod_builder* builder,
      const std::vector<spa_video_format>& formats,
      const std::vector<uint64_t>& modifiers,
      unsigned maxFramerate
    );


    /**
     * @brief Sends the current EnumFormat parameters to the compositor (Pipewire thread)
     *
     * @param stream Stream to renegotiate
     * @return int Pipewire result
     */
    static int _updateFormatParams(
      StreamData* stream
    );


    /**
     * @brief Runs the streams followed by a region, and stops the others (Pipewire thread)
     *
     * @param pw Pipewire data
     */
    static void _updateStreamActivity(
      PipewireData* pw
    );


    /**
     * @brief Returns the pixel width of the widest monitor whose format is known (Pipewire thread)
     *
     * @param pw Pipewire data
     * @return int Width, 0 if no format is known yet
     */
    static int _referencePixelWidth(
      const PipewireData* pw
    );


    static void _onRenegotiate(
      void* userData,
      uint64_t count
    );


    /**
     * @brief Averages the channel regions of a DMA-BUF frame on the GPU
     *
     * @return true Colors were computed
     */
    static bool _processDmaBuf(
      StreamData* stream,
      pw_buffer* pwBuffer,
      int width,
      int height,
      int subsampleWidth,
      const ChannelRegions& regions,
      ChannelColors& colors
    );


    /**
     * @brief Averages the channel regions of a shared memory frame on the CPU
     *
     * @return true Colors were computed
     */
    static bool _processSharedMemory(
      StreamData* stream,
      pw_buffer* pwBuffer,
      int width,
      int height,
      int subsampleWidth,
      Imaging::PixelFormat pixelFormat,
      const ChannelRegions& regions,
      ChannelColors& colors
    );

    static int _onUpdateFrameRateLimit(
      spa_loop* loop,
      bool async,
      uint32_t seq,
      const void* data,
      size_t size,
      void* userData
    );

    static int _onUpdateRegions(
      spa_loop* loop,
      bool async,
      uint32_t seq,
      const void* data,
      size_t size,
      void* userData
    );

    static int _onUpdatePaused(
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

    /**
     * @brief Notices a stream ending while capturing, such as when its monitor is unplugged
     *
     */
    static void _onStreamStateChanged(
      void* userdata,
      pw_stream_state old,
      pw_stream_state state,
      const char* error
    );


    static void _onStreamParamChanged(
      void* userdata,
      uint32_t id,
      const spa_pod* param
    );

    /**
     * @brief Gives up the screen selection, in the portal thread
     *
     * @param userData Capture
     * @return gboolean G_SOURCE_REMOVE
     */
    static gboolean _onAbortCapture(
      void* userData
    );


    /**
     * Screencast portal thread
    */
    static void _initCapture(
      XdgDesktopPortal::Capture* capture
    );


    /**
     * @brief Connects to the portal's Pipewire remote and starts a stream per monitor (loop thread)
     *
     * @return true The capture started, the loop runs
     */
    static bool _startCapture(
      XdgDesktopPortal::Capture* capture,
      PipewireData* pw,
      pw_main_loop* loop
    );


    /**
     * @brief Stops the streams and releases the GPU path (loop thread)
     *
     */
    static void _stopCapture(
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
    // Its thread runs the streams; reset before the data it uses goes away (see _teardownPipewire())
    std::optional<Pipewire::LoopThread> m_loopThread;
    XdgDesktopPortal::Capture m_capture;
    PipewireData m_pwData;
    // Captured monitors, known once every stream has its format
    Monitors m_monitors;
  };
}
