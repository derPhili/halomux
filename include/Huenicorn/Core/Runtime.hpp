#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>

#include <Huenicorn/Core/CoreService.hpp>
#include <Huenicorn/Core/Config.hpp>
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
    */
    void _initWebUI(
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
     * @brief Enables the selected entertainment configuration, recreates streamer  and loads related profile
     * 
     * @param entertainmentConfigurationId ID of the entertainment configuration to enable
     */
    void _enableEntertainmentConfiguration(
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


    /**
     * @brief Called to match stream m_channelStreams size with m_channels
     * 
     */
    void _updateStreamChannelsSize();


    // Attributes
    const std::string& m_version;
    std::filesystem::path m_configRoot;
    Core::Config m_config;
    std::unique_ptr<CoreService> m_coreService;

    bool m_keepLooping;
    bool m_openedSetup{false};
    std::unique_ptr<Timing::LoopRegulator> m_loopRegulator;

    //  API structure wrapper
    // Serializes entertainment configuration changes requested by the web UI threads
    std::mutex m_selectorMutex;
    std::unique_ptr<Hue::Api::EntertainmentConfigurationSelector> m_selector;

    // Guards m_channels and m_channelStreams, shared between the streaming loop and the web UI threads
    mutable std::mutex m_channelsMutex;
    Hue::Api::Channels m_channels;
    Hue::Api::ChannelStreams m_channelStreams;

    // Current colors snapshot, for external consumers (e.g. REST API)
    mutable std::mutex m_currentColorsMutex;
    Hue::Api::ChannelStreams m_currentColorsSnapshot;

    // Streamer
    std::mutex m_streamerMutex;
    std::unique_ptr<Stream::Streamer> m_streamer;

    // Service and flags
    ThreadedRestService m_webUIService;

    //  Image Processing
    Grabber::IGrabber* m_grabber;
    Imaging::ImageData m_frameData;
  };
}
