#pragma once

#include <atomic>

#include <Huenicorn/Grabber/IGrabber.hpp>

#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#pragma GCC diagnostic pop

#include <Huenicorn/Grabber/GnuLinux/Pipewire/EglDownscaler.hpp>
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
     * Channel regions and their colors, shared between the Pipewire thread and the streaming loop
    */
    struct SharedChannels
    {
      std::mutex mutex;
      ChannelRegions regions;
      ChannelColors colors;
      bool hasColors{false};
    };


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


    struct PipewireData
    {
      spa_hook coreListener;
      pw_main_loop* loop{nullptr};
      pw_context* context{nullptr};
      pw_stream* stream{nullptr};
      spa_video_info format;
      SharedChannels channels;
      std::promise<bool> screenDataReadyPromise;
      bool promiseSetAlready{false};
      Core::Config* config;

      // Highest framerate the compositor offered (written by the Pipewire thread)
      std::atomic<unsigned> displayMaxFramerate{0};

      // Current framerate limit, 0 for none (Pipewire thread)
      unsigned frameRateLimit{0};

      // GPU path (Pipewire thread only)
      std::unique_ptr<EglDownscaler> gpu;
      std::vector<DmaBufFormat> dmaBufFormats;
      bool dmaBufFailed{false};
      bool usingDmaBuf{false};
      spa_source* renegotiateEvent{nullptr};
    };


  public:
    // Constructor / destructor
    /**
     * PipewireGrabber constructor
    */
    PipewireGrabber(
      Core::Config* config
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
     * @brief Builds the EnumFormat parameters of the stream
     *
     * DMA-BUF formats come first (preferred), followed by a shared memory fallback.
     *
     * @param builder Builder to write the parameters with
     * @param pw Pipewire data (DMA-BUF capabilities and framerate limit)
     * @return std::vector<const spa_pod*> Built parameters
     */
    static std::vector<const spa_pod*> _buildFormatParams(
      spa_pod_builder* builder,
      const PipewireData* pw
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
     * @param pw Pipewire data
     * @return int Pipewire result
     */
    static int _updateFormatParams(
      PipewireData* pw
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
      PipewireData* pw,
      pw_buffer* pwBuffer,
      int width,
      int height,
      const ChannelRegions& regions,
      ChannelColors& colors
    );


    /**
     * @brief Averages the channel regions of a shared memory frame on the CPU
     *
     * @return true Colors were computed
     */
    static bool _processSharedMemory(
      PipewireData* pw,
      pw_buffer* pwBuffer,
      int width,
      int height,
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

    static void _onStreamParamChanged(
      void* userdata,
      uint32_t id,
      const spa_pod* param
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
