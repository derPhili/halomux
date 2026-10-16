#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include <Huenicorn/Audio/AudioAnalyzer.hpp>
#include <Huenicorn/Core/CoreService.hpp>
#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Core/MonitorSelection.hpp>
#include <Huenicorn/Hue/Api/EntertainmentConfigurationSelector.hpp>
#include <Huenicorn/Grabber/IGrabber.hpp>
#include <Huenicorn/Network/Http/Server/WebUIBackend.hpp>
#include <Huenicorn/Stream/Streamer.hpp>
#include <Huenicorn/Timing/LoopRegulator.hpp>
#include <Huenicorn/Serialization/Json.hpp>


namespace Huenicorn::Core
{
  class CoreService;

  /**
   * @brief Main execution scope of Huenicorn that handles application components and executes streaming loop.
   * 
   */
  class Runtime
  {
    friend CoreService;

    // Type definitions
    struct ThreadedRestService
    {
      std::optional<Huenicorn::Network::Http::Server::WebUIBackend> server;
      std::jthread thread;
    };

  public:
    // Constructor
    /**
     * @brief Runtime constructor
     * 
     * @param version Current version of Huenicorn
     * @param configRoot Path to the configuration directory
     */
    Runtime(
      const std::string& version,
      const std::filesystem::path& configRoot
    );


    // Getters
    /**
     * @brief Returns the current version of Huenicorn
     * 
     * @return const std::string& Current version of Huenicorn
     */
    const std::string& version() const;


    // Methods
    /**
     * @brief Initiates Huenicorn and starts the main loop
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
     * @brief Initializes the channels based on the profile settings
     * 
     * @param jsonProfile Data from the user-defined profile
     */
    void _initChannels(
      const Serialization::Json& jsonProfile
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
     * @brief Finds the captured monitor of each channel, with m_channelsMutex held
     *
     * Active channels of profiles from before monitors per lamp follow the first monitor.
     */
    void _resolveChannelMonitors();


    /**
     * @brief Returns the horizontal center of a channel's region on the whole desktop, with m_channelsMutex held
     *
     * @param channel Channel
     * @return float 0 at the left edge of the desktop, 1 at the right one, 0.5 if its monitor isn't captured
     */
    float _desktopCenterX(
      const Hue::Api::Channel& channel
    ) const;


    /**
     * @brief Enables the selected entertainment configuration, recreates streamer  and loads related profile
     * 
     * @param entertainmentConfigurationId ID of the entertainment configuration to enable
     * @return true The entertainment configuration was enabled
     * @return false The entertainment configuration could not be selected
     */
    bool _enableEntertainmentConfiguration(
      const std::string& entertainmentConfigurationId
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
     * @brief Disables streaming on current entertainment configuration
     * 
     */
    void _shutdown();


    // Attributes
    const std::string& m_version;
    std::filesystem::path m_configRoot;
    Core::Config m_config;
    std::unique_ptr<CoreService> m_coreService;

    std::atomic<bool> m_keepLooping{true};
    bool m_openedSetup{false};
    std::unique_ptr<Timing::LoopRegulator> m_loopRegulator;

    //  API structure wrapper
    // Serializes entertainment configuration changes requested by the web UI threads
    std::mutex m_selectorMutex;
    std::unique_ptr<Hue::Api::EntertainmentConfigurationSelector> m_selector;

    // Guards m_channels, m_channelStreams and m_monitors, shared between the streaming loop and the web UI threads
    mutable std::mutex m_channelsMutex;
    Hue::Api::Channels m_channels;
    Hue::Api::ChannelStreams m_channelStreams;
    // Captured monitors the channels' monitorIndex refers to
    Grabber::Monitors m_monitors;
    // Last check for monitors lost while capturing (streaming thread)
    std::chrono::steady_clock::time_point m_lastLostMonitorsCheck{};

    // Current colors snapshot, for external consumers (e.g. REST API)
    mutable std::mutex m_currentColorsMutex;
    Hue::Api::ChannelStreams m_currentColorsSnapshot;

    // Streamer
    std::mutex m_streamerMutex;
    std::unique_ptr<Stream::Streamer> m_streamer;

    // Service and flags
    ThreadedRestService m_webUIService;

    //  Image Processing
    mutable std::mutex m_grabberMutex;
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

    // Screen selection in progress, joined (and aborted) before the members it uses go away
    std::jthread m_reselectThread;
  };
}
