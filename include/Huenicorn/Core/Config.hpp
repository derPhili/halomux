#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>

#include <Huenicorn/Hue/Auth/Credentials.hpp>
#include <Huenicorn/Imaging/Color.hpp>


namespace Huenicorn::Core
{
  /**
   * @brief Main configuration parameters to be stored and loaded in a persistent way
   * 
   */
  class Config
  {
  public:
    // Color of each channel while the lamps don't follow the screen
    using StaticColors = std::map<uint8_t, Imaging::Color>;

    // Static colors saved by the user, by name
    using Scenes = std::map<std::string, StaticColors>;

    struct Data
    {
      std::optional<unsigned> restServerPort;
      std::optional<std::string> boundBackendIP;

      std::optional<std::string> bridgeAddress;
      std::optional<Hue::Auth::Credentials> credentials;
      std::optional<std::string> profileName;

      std::optional<unsigned> refreshRate{0};
      std::optional<unsigned> subsampleWidth{0};
      std::optional<float> transitionSmoothing{0.f};
      std::optional<float> highlightBoost{0.f};
      std::optional<bool> audioReactive{false};
      std::optional<float> brightness{1.f};
      std::optional<bool> syncEnabled{true};
      std::optional<StaticColors> staticColors{StaticColors{}};
      std::optional<Scenes> scenes{Scenes{}};
      std::optional<std::string> language{"auto"};
      // When the user accepted the notice of the setup (AI assistance, no warranty), UTC ISO 8601
      std::optional<std::string> noticeAccepted;
      std::optional<std::string> restoreToken;
    };


    // Constructor
    /**
     * @brief Config constructor
     * 
     * @param configRoot Path to the configuration directory
     */
    Config(
      const std::filesystem::path& configRoot
    );


    // Getters
    /**
     * @brief Returns path to config file
     * 
     * @return const std::filesystem::path& config file path
     */
    const std::filesystem::path& configFilePath() const;


    /**
     * @brief Returns whether the required fields were registered
     * 
     * @return true Required fileds were all provided
     * @return false Some required fields were not provided
     */
    bool initialSetupOk() const;


    /**
     * @brief Returns the registered REST port for web UI
     * 
     * @return int Registered REST port
     */
    unsigned restServerPort() const;


    /**
     * @brief Returns the registered bound ip address for the backend
     * 
     * @return const std::string& Bound ip address
     */
    const std::string& boundBackendIP() const;


    /**
     * @brief Returns the registered address of the Hue bridge
     * 
     * @return const std::optional<std::string>& Registered address of the Hue bridge
     */
    const std::optional<std::string>& bridgeAddress() const;


    /**
     * @brief Returns registered credentials
     * 
     * @return const std::optional<Credentials>& Registered credentials
     */
    const std::optional<Hue::Auth::Credentials>& credentials() const;


    /**
     * @brief Returns the registered profile name
     * 
     * @return const std::optional<std::string>& Registered profile name
     */
    const std::optional<std::string>& profileName() const;


    /**
     * @brief Returns the Registered refresh rate
     * 
     * @return unsigned Registered refresh rate
     */
    unsigned refreshRate() const;


    /**
     * @brief Returns the registered subsample with
     * 
     * @return unsigned Registered subsample width
     */
    unsigned subsampleWidth() const;


    /**
     * @brief Returns the registered transition smoothing factor
     *
     * @return float Transition smoothing factor, in [0, 1)
    */
    float transitionSmoothing() const;


    /**
     * @brief Returns the strength of the brightness flash on sudden brightening
     *
     * @return float Value in [0, 1.0]
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
     * @return float Value in [0, 1.0]
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
     * @return StaticColors Color by channel ID, channels without one are left to the caller
    */
    StaticColors staticColors() const;


    /**
     * @brief Returns the scenes saved by the user
     *
     * @return Scenes Static colors by scene name
    */
    Scenes scenes() const;


    /**
     * @brief Returns the language of the user interfaces
     *
     * @return std::string "auto" (system language), "en" or "de"
    */
    std::string language() const;


    /**
     * @brief Returns the registered restoration token for screen selection portal
     * 
     * @return Optional restore token string
    */
    const std::optional<std::string>& restoreToken() const;


    // Setters
    /**
     * @brief Registers Hue bridge address
     * 
     * @param bridgeAddress Address of the Hue bridge
     */
    void setBridgeAddress(
      const std::string& bridgeAddress
    );


    /**
     * @brief Registers credentials
     * 
     * @param credentials User credentials for the Hue bridge
     */
    void setCredentials(
      const Hue::Auth::Credentials& credentials
    );


    /**
     * @brief Registers current profile name
     * 
     * @param profileName Profile name to use
     */
    void setProfileName(
      const std::string& profileName
    );


    /**
     * @brief registers the subsample width for image processing
     * 
     * @param subsampleWidth Subsample width
     */
    void setSubsampleWidth(
      unsigned subsampleWidth
    );


    /**
     * @brief Registers the refresh rate for color streaming
     * 
     * @param refreshRate Refresh rate
     */
    void setRefreshRate(
      unsigned refreshRate
    );


    /**
     * @brief Registers the transition smoothing factor
     *
     * @param transitionSmoothing Smoothing factor, clamped to [0, 0.97]
    */
    void setTransitionSmoothing(
      float transitionSmoothing
    );


    /**
     * @brief Registers the strength of the brightness flash on sudden brightening
     *
     * @param highlightBoost Value, clamped to [0, 1.0]
    */
    void setHighlightBoost(
      float highlightBoost
    );


    /**
     * @brief Registers whether transients in the PC sound also trigger the highlight boost
     *
     * @param audioReactive Sound reactive
    */
    void setAudioReactive(
      bool audioReactive
    );


    /**
     * @brief Registers the global lamp brightness factor
     *
     * @param brightness Value, clamped to [0, 1.0]
    */
    void setBrightness(
      float brightness
    );


    /**
     * @brief Registers whether the lamps follow the screen and sound, or show the static colors
     *
     * @param syncEnabled Lamps follow the screen and sound
    */
    void setSyncEnabled(
      bool syncEnabled
    );


    /**
     * @brief Registers the colors shown while sync is disabled
     *
     * @param staticColors Color by channel ID, replacing the previous ones
    */
    void setStaticColors(
      const StaticColors& staticColors
    );


    /**
     * @brief Saves a scene, replacing the one with the same name
     *
     * @param name Scene name
     * @param staticColors Color by channel ID
    */
    void saveScene(
      const std::string& name,
      const StaticColors& staticColors
    );


    /**
     * @brief Deletes a saved scene
     *
     * @param name Scene name
     * @return true The scene existed
     * @return false No scene has that name
    */
    bool deleteScene(
      const std::string& name
    );


    /**
     * @brief Registers the language of the user interfaces
     *
     * @param language "auto" (system language), "en" or "de"
    */
    void setLanguage(
      const std::string& language
    );


    /**
     * @brief Registers that the user accepted the notice of the setup
     *
     * @param acceptedAt Time of the acceptance, UTC ISO 8601
    */
    void setNoticeAccepted(
      const std::string& acceptedAt
    );


    /**
     * @brief Registers the restore token for screen selection
     * 
     * @param std::string Token for xdg session restauration
    */
    void setRestoreToken(
      const std::string& restoreToken
    );


  private:
    // Private methods
    /**
     * @brief Loads the configuration data from config file
     * 
     * @return true Required fields were loaded
     * @return false Required fields are missing
     */
    bool _loadConfigFile();


    /**
     * @brief Writes the current state of the config. Called from all setters
     * 
     */
    void _save() const;


    // Attributes
    std::filesystem::path m_configFilePath;
    std::optional<Data> m_configData;

    // Guards the runtime-tunable values (refresh rate, subsample width,
    // smoothing...) which are written by the web UI threads
    // while the streaming loop reads them. Values returned by reference are
    // only written during initial setup, before any concurrent access.
    mutable std::mutex m_mutex;

    template<typename T>
    friend struct Huenicorn::Serialization::JsonSerializer;
  };
}
