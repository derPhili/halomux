#pragma once

#include <mutex>

#include <Huenicorn/Core/Config.hpp>

#include <Huenicorn/Hue/Api/EntertainmentConfiguration.hpp>

#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Core/Runtime.hpp>


namespace Huenicorn::Network::Http::Server
{
  class SetupBackend;
}

namespace Huenicorn::Core
{
  class Runtime;

  class CoreService
  {
  public:
    CoreService(Runtime& runtime, Config& config);

    const std::string& version() const;

    /**
     * @brief Returns the path of the configuration file
     * 
     * @return const std::filesystem::path Configuration file path
     */
    const std::filesystem::path configFilePath() const;


    /**
     * @brief Registers Hue bridge address if valid
     * 
     * @param bridgeAddress Hue bridge address to register
     * @return true Hue bridge address is valid and was registered
     * @return false Hue bridge address is invalid and was discarded
     */
    bool validateBridgeAddress(
      const std::string& bridgeAddress
    );


    /**
     * @brief Registers Hue bridge credentials if valid
     * 
     * @param credentials Hue bridge credentials to register
     * @return true Hue bridge credentials are valid and were registered
     * @return false Hue bridge credentials are invalid and were discarded
     */
    bool validateCredentials(
      const Hue::Auth::Credentials& credentials
    );


    Serialization::Json registerNewUser();


    /**
     * @brief Returns the list of available entertainment configurations
     * 
     * @return const EntertainmentConfigurations& Available entertainment configurations
     */
    const Hue::Api::EntertainmentConfigurations& entertainmentConfigurations() const;


    /**
     * @brief Returns the ID of the current entertainment configuration
     * 
     * @return const std::string& Current entertainment configuration ID
     */
    std::optional<std::string> currentEntertainmentConfigurationId() const;



    /**
     * @brief Returns the current entertainment configuration
     * 
     * @return const EntertainmentConfiguration& Current entertainment configuration
     */
    const Hue::Api::EntertainmentConfiguration& currentEntertainmentConfiguration() const;



    /**
     * @brief Returns the resolution of the display
     * 
     * @return glm::ivec2 Display resolution
     */
    glm::ivec2 displayResolution() const;


    /**
     * @brief Returns a list of subsample resolutions
     * 
     * @return std::vector<glm::ivec2> Subsample resolutions
     */
    std::vector<glm::ivec2> subsampleResolutionCandidates() const;


    /**
     * @brief Returns the current subsample width
     * 
     * @return unsigned Current subsample width
     */
    unsigned subsampleWidth() const;


    /**
     * @brief Returns the current color streaming refresh rate
     * 
     * @return unsigned Refresh rate
     */
    unsigned refreshRate() const;


    /**
     * @brief Returns the maximal usable refresh rate: the lowest of the display's and the bridge's
     * 
     * @return unsigned Maximal refresh rate
     */
    unsigned maxRefreshRate() const;


    /**
     * @brief Returns the refresh rate of the display
     *
     * @return unsigned Display refresh rate
     */
    unsigned displayRefreshRate() const;


    /**
     * @brief Returns the current transition smoothing factor
     *
     * @return float Transition smoothing factor
     */
    float transitionSmoothing() const;


    // Setters
    /**
     * @brief Sets the current entertainment configuration
     * 
     * @param entertainmentConfigurationId ID of the entertainment configuration to select
     * @return true Successfully selected entertainment configuration
     * @return false Failed to select entertainment configuration
     */
    bool setEntertainmentConfiguration(
      const std::string& entertainmentConfigurationId
    );


    /**
     * @brief Set the Channel UV
     * 
     * @param channelId ID of the channel to affect
     * @param uv UV coordinate to set
     * @param uvCorner Corner to affect
     * @return std::optional<Imaging::UVs> Clamped UV value, empty if the channel doesn't exist
     */
    std::optional<Imaging::UVs> setChannelUV(
      uint8_t channelId,
      Imaging::UV&& uv,
      Imaging::UVCorner uvCorner
    );


    /**
     * @brief Sets the Channel Gamma Factor
     * 
     * @param channelId ID of the channel to affect
     * @param gammaFactor Gamma factor to apply
     * @return true Gamma factor was successuflly set
     * @return false Gamma factor could not be set
     */
    bool setChannelGammaFactor(
      uint8_t channelId,
      float gammaFactor
    );


    /**
     * @brief Set the width of the subsample image to compute
     * 
     * @param subsampleWidth Desired width for the image subsample
     */
    void setSubsampleWidth(
      unsigned subsampleWidth
    );


    /**
     * @brief Set the refresh rate for color streaming
     * 
     * @param refreshRate Desired refresh rate for color data streaming
     */
    void setRefreshRate(
      unsigned refreshRate
    );


    /**
     * @brief Set the transition smoothing factor
     *
     * @param transitionSmoothing Desired smoothing amount, in [0, 100]
     */
    void setTransitionSmoothing(
      float transitionSmoothing
    );


    /**
     * @brief Set the streaming activity of the channel referenced by ID
     * 
     * @param channelId ID of the channel to affect
     * @param active State of the streaming
     * @return true Streaming state was successfully set
     * @return false Streaming state could not be set
     */
    bool setChannelActivity(
      uint8_t channelId,
      bool active
    );


    /**
     * @brief Returns the list of the available channels
     * 
     * @return Channels Snapshot of the channels list
     */
    Hue::Api::Channels channels() const;


    /**
     * @brief Returns a snapshot of the per-channel colors currently being streamed
     *
     * @return Hue::Api::ChannelStreams Current channel colors snapshot
     */
    Hue::Api::ChannelStreams currentColors() const;


    /**
     * @brief Saves the current state of a user-defined configuration as profile
     * 
     */
    void saveProfile();


     /**
     * @brief Stops the main loop
     * 
     */
    void stop();

    /**
     * @brief Checks for setup validity
     *
     * @return true Setup finished success
     * @return false Setup was not completed
    */
    bool ensureInitialSetup();


    /**
     * @brief Aborts the initial setup if it is currently running
     *
     */
    void abortInitialSetup();


    // Private getter
    /**
     * @brief Returns the path to the current profile
     * 
     * @return std::filesystem::path Path to the current profile
     */
    std::filesystem::path profilePath() const;


    /**
     * @brief Loads the last profile
     * 
     * @return Loaded profile. Empty if not found
     */
    std::optional<Serialization::Json> getProfile();

  private:
    Runtime& m_runtime;
    Config& m_config;

    std::mutex m_setupBackendMutex;
    Network::Http::Server::SetupBackend* m_setupBackend{nullptr};
  };
}