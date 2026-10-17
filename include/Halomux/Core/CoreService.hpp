#pragma once

#include <mutex>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/MonitorSelection.hpp>
#include <Halomux/Grabber/IGrabber.hpp>

#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Runtime.hpp>


namespace Halomux::Network::Http::Server
{
  class SetupBackend;
}

namespace Halomux::Core
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
     * @brief Lists the entertainment areas of the registered bridge, during the initial setup
     *
     * @return Serialization::Json Array of {name, lamps}, empty if there are none or the bridge can't be reached
     */
    Serialization::Json bridgeEntertainmentAreas() const;


    /**
     * @brief Returns the list of available entertainment configurations
     * 
     * @return EntertainmentConfigurations Available entertainment configurations, empty until the bridge answered
     */
    Hue::Api::EntertainmentConfigurations entertainmentConfigurations() const;


    /**
     * @brief Returns the ID of the current entertainment configuration
     * 
     * @return const std::string& Current entertainment configuration ID
     */
    std::optional<std::string> currentEntertainmentConfigurationId() const;



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
     * @brief Returns where captured frames are processed (GPU or CPU)
     *
     * @return Grabber::IGrabber::ProcessingInfo Processing path and details
     */
    Grabber::IGrabber::ProcessingInfo processingInfo() const;


    /**
     * @brief Returns the current transition smoothing factor
     *
     * @return float Transition smoothing factor
     */
    float transitionSmoothing() const;


    /**
     * @brief Returns the strength of the brightness flash on sudden brightening
     *
     * @return float Value in percent
     */
    float highlightBoost() const;


    /**
     * @brief Returns whether transients in the PC sound also trigger the highlight boost
     *
     * @return true Sound reactive
     */
    bool audioReactive() const;


    /**
     * @brief Returns the global lamp brightness factor
     *
     * @return float Value in percent
     */
    float brightness() const;


    /**
     * @brief Returns whether the lamps follow the screen and sound, or show the static colors
     *
     * @return true Lamps follow the screen and sound
     */
    bool syncEnabled() const;


    /**
     * @brief Returns the colors shown while sync is disabled
     *
     * @return Config::StaticColors Color by channel ID
     */
    Config::StaticColors staticColors() const;


    /**
     * @brief Returns the scenes saved by the user
     *
     * @return Config::Scenes Static colors by scene name
     */
    Config::Scenes scenes() const;


    /**
     * @brief Returns the language of the user interfaces
     *
     * @return std::string "auto" (system language), "en" or "de"
     */
    std::string language() const;


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
     * @brief Moves a channel's region onto a captured monitor
     *
     * @param channelId ID of the channel to affect
     * @param monitor Index of the monitor, as in monitors()
     * @param uvs Region on that monitor, clamped to it
     * @return std::optional<Imaging::UVs> Clamped region, empty if the channel or the monitor doesn't exist
     */
    std::optional<Imaging::UVs> setChannelMonitor(
      uint8_t channelId,
      size_t monitor,
      const Imaging::UVs& uvs
    );


    /**
     * @brief Returns the captured monitors
     *
     * @return Grabber::Monitors Captured monitors, in the order channels refer to them
     */
    Grabber::Monitors monitors() const;


    /**
     * @brief Asks for the monitors to capture: the desktop shows its screen selection dialog
     *
     * The current monitors keep being captured until the selection is confirmed.
     *
     * @return true The screen selection started
     * @return false A screen selection is already pending
     */
    bool reselectMonitors();


    /**
     * @brief Returns the state of the last screen selection
     *
     * @return MonitorSelection Pending while the dialog is open, Cancelled if it was cancelled
     */
    MonitorSelection monitorSelection() const;


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
     * @brief Set the strength of the brightness flash on sudden brightening
     *
     * @param highlightBoost Value in percent
     */
    void setHighlightBoost(
      float highlightBoost
    );


    /**
     * @brief Sets whether transients in the PC sound also trigger the highlight boost
     *
     * @param audioReactive Sound reactive
     */
    void setAudioReactive(
      bool audioReactive
    );


    /**
     * @brief Plays the preview sequence on the lamps instead of the screen content
     *
     * @return float Duration of the preview [s]
     */
    float startPreview();


    /**
     * @brief Stops the running preview: the lamps follow the screen or show the static scene again
     *
     */
    void stopPreview();


    /**
     * @brief Returns the remaining time of the running preview
     *
     * @return float Remaining time [s], 0 if no preview runs
     */
    float previewRemaining() const;


    /**
     * @brief Set the global lamp brightness factor
     *
     * @param brightness Value in percent
     */
    void setBrightness(
      float brightness
    );


    /**
     * @brief Sets whether the lamps follow the screen and sound, or show the static colors
     *
     * @param syncEnabled Lamps follow the screen and sound
     */
    void setSyncEnabled(
      bool syncEnabled
    );


    /**
     * @brief Sets the colors shown while sync is disabled
     *
     * @param staticColors Color by channel ID
     */
    void setStaticColors(
      const Config::StaticColors& staticColors
    );


    /**
     * @brief Saves a scene, replacing the one with the same name
     *
     * @param name Scene name, trimmed
     * @param staticColors Color by channel ID
     * @return true The scene was saved
     * @return false The name is empty or too long
     */
    bool saveScene(
      const std::string& name,
      const Config::StaticColors& staticColors
    );


    /**
     * @brief Deletes a saved scene
     *
     * @param name Scene name
     * @return true The scene existed
     */
    bool deleteScene(
      const std::string& name
    );


    /**
     * @brief Sets the language of the user interfaces
     *
     * @param language "auto" (system language), "en" or "de"
     * @return true The language is supported
     */
    bool setLanguage(
      const std::string& language
    );


    /**
     * @brief Records that the user accepted the current version of the notice, with the current time
     *
     */
    void acceptNotice();


    /**
     * @brief Returns whether the current version of the notice was accepted
     *
     * @return true The notice was accepted, in its current version or a later one
     */
    bool noticeAccepted() const;


    /**
     * @brief Shows a scene on the lamps: its colors replace those of the lamps it covers, and sync is disabled
     *
     * Built-in scenes are spread over the active lamps, from left to right.
     *
     * @param name Scene name
     * @param builtin Whether the scene is a built-in one, or a saved one
     * @return true The scene exists
     */
    bool applyScene(
      const std::string& name,
      bool builtin
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
     * @brief Saves the lamp settings of the current entertainment area in the profile
     *
     * @return true Saved
     * @return false No area is selected, or the file couldn't be written
     */
    bool saveProfile();


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
     * @brief Tells whether the setup registered everything needed: bridge address and credentials
     *
     * @return true The setup can be finished
     */
    bool initialSetupComplete() const;


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
    /**
     * @brief Identifies the bridge for the certificate checks, learning its id if needed
     *
     */
    void _identifyBridge();


    /**
     * @brief Registers the bridge's identity for the certificate checks of the requests to it
     *
     */
    void _registerBridge();


    Runtime& m_runtime;
    Config& m_config;

    std::mutex m_setupBackendMutex;
    Network::Http::Server::SetupBackend* m_setupBackend{nullptr};
  };
}