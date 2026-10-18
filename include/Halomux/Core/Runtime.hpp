#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

#include <Halomux/Audio/AudioAnalyzer.hpp>
#include <Halomux/Core/CoreService.hpp>
#include <Halomux/Core/AreaSession.hpp>
#include <Halomux/Core/ChannelStore.hpp>
#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/MonitorSelection.hpp>
#include <Halomux/Grabber/IGrabber.hpp>
#include <Halomux/Network/Http/Server/WebUIBackend.hpp>
#include <Halomux/Stream/Streamer.hpp>
#include <Halomux/Timing/LoopRegulator.hpp>
#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Core
{
  class CoreService;

  /**
   * @brief Main execution scope of Halomux that handles application components and executes streaming loop.
   * 
   */
  class Runtime
  {
    friend CoreService;

    // Type definitions
    struct ThreadedRestService
    {
      std::optional<Halomux::Network::Http::Server::WebUIBackend> server;
      std::jthread thread;
    };

  public:
    // Constructor
    /**
     * @brief Runtime constructor
     * 
     * @param version Current version of Halomux
     * @param configRoot Path to the configuration directory
     */
    Runtime(
      const std::string& version,
      const std::filesystem::path& configRoot
    );


    // Getters
    /**
     * @brief Returns the current version of Halomux
     * 
     * @return const std::string& Current version of Halomux
     */
    const std::string& version() const;


    // Methods
    /**
     * @brief Initiates Halomux and starts the main loop
     * 
     */
    void start();


    /**
     * @brief Stops the main loop
     * 
     */
    void stop();


  private:



    // Private methods

    /**
     * @brief Initializes config and profile
     * 
     * @return true Succeeded to load a suitable configuration
     * @return false Failed to load a suitable configuration
     */
    bool _initSettings();


    /**
     * Brief Initializes a grabber based on the graphical session
     * 
     * @return true Relevant grabber was found and initialized
     * @return false could not initialize a grabber
    */
    bool _initGrabber();


    /**
     * Brief Initializes the web UI
     *
     * @return true The web UI is listening
     * @return false The web UI could not be started
    */
    bool _initWebUI(
      bool spawnBrowser
    );


    /**
     * @brief Returns the current grabber, which a screen selection may replace at any time
     *
     * @return std::shared_ptr<Grabber::IGrabber> Grabber, kept alive by the caller while used
     */
    std::shared_ptr<Grabber::IGrabber> grabber() const;


    /**
     * @brief Asks for the monitors to capture in the background, and switches to them once confirmed
     *
     * @return true The screen selection started
     * @return false A screen selection is already pending
     */
    bool _reselectMonitors();


    /**
     * @brief Loads the lamps of a newly selected area, with their saved settings
     *
     * @param areaId Entertainment area
     * @param members Lamps of each of its channels
     */
    void _onAreaSelected(
      const std::string& areaId,
      const Hue::Api::ChannelMembers& members
    );


    /**
     * @brief Starts the main loop calling for processing at given refresh rate
     * 
     */
    void _startStreamingLoop();


    /**
     * @brief Computes the channels colors from grabbed frames and streams them to the bridge
     * 
     */
    void _update();


    /**
     * @brief Stops streaming to the entertainment area
     *
     */
    void _shutdown();


    // Attributes
    const std::string& m_version;
    std::filesystem::path m_configRoot;
    Core::Config m_config;
    std::unique_ptr<CoreService> m_coreService;

    std::atomic<bool> m_keepLooping{true};
    // Requested by stop(): aborts the screen selection dialog of the start
    std::stop_source m_stopSource;
    bool m_openedSetup{false};
    std::unique_ptr<Timing::LoopRegulator> m_loopRegulator;

    // Lamps, their streams and the captured monitors, shared with the web UI threads
    ChannelStore m_channelStore;
    // The entertainment area and the stream to it. Declared after the lamps, which its checks
    // replace when they select an area: stopped first
    std::unique_ptr<AreaSession> m_areaSession;
    // Last check for monitors lost while capturing (streaming thread)
    std::chrono::steady_clock::time_point m_lastLostMonitorsCheck{};

    // Current colors snapshot, for external consumers (e.g. REST API)
    mutable std::mutex m_currentColorsMutex;
    Hue::Api::ChannelStreams m_currentColorsSnapshot;

    // Service and flags
    ThreadedRestService m_webUIService;

    //  Image Processing
    mutable std::mutex m_grabberMutex;
    // The only owner of the grabber: declared after m_config, so destroyed before the
    // configuration its threads read
    std::shared_ptr<Grabber::IGrabber> m_grabber;
    // Set when a screen selection replaced the grabber, for the streaming thread
    std::atomic<bool> m_grabberReplaced{false};
    std::atomic<MonitorSelection> m_monitorSelection{MonitorSelection::Idle};
    Grabber::IGrabber::ChannelRegions m_channelRegions;
    // Capture is paused while sync is disabled (streaming thread)
    bool m_capturePaused{false};

    // Sound transients, only while Config::audioReactive() is enabled (streaming thread)
    std::unique_ptr<Audio::AudioAnalyzer> m_audioAnalyzer;

    // Start of the running preview (steady clock ticks), 0 when none
    std::atomic<int64_t> m_previewStart{0};

    // Screen selection in progress, joined (and aborted) before the members it uses go away.
    // Started by a web UI thread, stopped by the thread handling a signal.
    std::mutex m_reselectMutex;
    std::jthread m_reselectThread;
  };
}
