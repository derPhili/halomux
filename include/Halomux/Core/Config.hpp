#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>

#include <Halomux/Hue/Auth/Credentials.hpp>
#include <Halomux/Imaging/Color.hpp>


namespace Halomux::Core
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

    // Acceptance of the notice about AI assistance and the missing warranty (see Notice.hpp)
    struct NoticeAcceptance
    {
      unsigned version{0};
      // Time of the acceptance, UTC ISO 8601
      std::string at;
    };

    // Wider than any monitor: a subsample width can't exceed the image it's taken from
    static constexpr unsigned MaxSubsampleWidth = 16384;

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
      std::optional<NoticeAcceptance> noticeAccepted;
      // Id of the bridge, the name its certificate carries
      std::optional<std::string> bridgeId;
      // Whether the bridge's certificate passed the check once: then it always has to
      std::optional<bool> bridgeCertificateVerified{false};
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
     * @return std::string Bound ip address
     */
    std::string boundBackendIP() const;


    /**
     * @brief Returns the registered address of the Hue bridge
     * 
     * @return std::optional<std::string> Registered address of the Hue bridge
     */
    std::optional<std::string> bridgeAddress() const;


    /**
     * @brief Returns registered credentials
     * 
     * @return std::optional<Credentials> Registered credentials
     */
    std::optional<Hue::Auth::Credentials> credentials() const;


    /**
     * @brief Returns the registered profile name
     * 
     * @return std::optional<std::string> Registered profile name
     */
    std::optional<std::string> profileName() const;


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
    std::optional<std::string> restoreToken() const;


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
     * @param subsampleWidth Subsample width, clamped to [1, MaxSubsampleWidth]
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
     * @brief Returns the last acceptance of the notice
     *
     * @return std::optional<NoticeAcceptance> Acceptance, empty if the notice was never accepted
    */
    std::optional<NoticeAcceptance> noticeAccepted() const;


    /**
     * @brief Returns the id of the bridge
     *
     * @return std::optional<std::string> Bridge id, empty if not known yet
    */
    std::optional<std::string> bridgeId() const;


    /**
     * @brief Returns whether the bridge's certificate passed the check once
     *
     * @return true Its certificate always has to pass the check
    */
    bool bridgeCertificateVerified() const;


    /**
     * @brief Registers that the user accepted a version of the notice
     *
     * @param acceptance Version of the notice and time of the acceptance
    */
    void setNoticeAccepted(
      const NoticeAcceptance& acceptance
    );


    /**
     * @brief Registers the identity of the bridge
     *
     * @param bridgeId Id of the bridge, the name its certificate carries
     * @param certificateVerified Whether its certificate passed the check. Once true, it stays true
    */
    void setBridgeIdentity(
      const std::string& bridgeId,
      bool certificateVerified
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
    friend struct Halomux::Serialization::JsonSerializer;
  };
}
