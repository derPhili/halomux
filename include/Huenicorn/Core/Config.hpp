#pragma once

#include <filesystem>
#include <mutex>
#include <optional>

#include <Huenicorn/Imaging/Interpolation.hpp>
#include <Huenicorn/Hue/Auth/Credentials.hpp>


namespace Huenicorn::Core
{
  /**
   * @brief Main configuration parameters to be stored and loaded in a persistent way
   * 
   */
  class Config
  {
  public:
    struct Data
    {
      std::optional<unsigned> restServerPort;
      std::optional<std::string> boundBackendIP;

      std::optional<std::string> bridgeAddress;
      std::optional<Hue::Auth::Credentials> credentials;
      std::optional<std::string> profileName;

      std::optional<unsigned> refreshRate{0};
      std::optional<unsigned> subsampleWidth{0};
      std::optional<Imaging::Interpolation::Type> interpolation{Imaging::Interpolation::Type::Area};
      std::optional<float> transitionSmoothing{0.f};
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
     * @brief Returns the registered subsample interpolation type
     * 
     * @return Type of current subsample interpolation
    */
    Imaging::Interpolation::Type interpolation() const;


    /**
     * @brief Returns the registered transition smoothing factor
     *
     * @return float Transition smoothing factor, in [0, 1)
    */
    float transitionSmoothing() const;


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
     * @brief Registers the interpolation type for subsample
     * 
     * @param interpolation Type of interpolation
    */
    void setInterpolation(
      Imaging::Interpolation::Type interpolation
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
    // interpolation, smoothing...) which are written by the web UI threads
    // while the streaming loop reads them. Values returned by reference are
    // only written during initial setup, before any concurrent access.
    mutable std::mutex m_mutex;

    template<typename T>
    friend struct Huenicorn::Serialization::JsonSerializer;
  };
}
