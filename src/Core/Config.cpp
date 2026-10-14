#include <Huenicorn/Core/Config.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Serialization/Config.hpp>


namespace Huenicorn::Core
{
  using namespace Serialization;

  Config::Config(
    const std::filesystem::path& settingsRoot
  ):
  m_configFilePath(settingsRoot / "config.json")
  {
    _loadConfigFile();
  }


  const std::filesystem::path& Config::configFilePath() const
  {
    return m_configFilePath;
  }


  bool Config::initialSetupOk() const
  {
    try{
      const auto& data = m_configData.value();
      const auto& bridgeAddress = data.bridgeAddress;
      const auto& credentials = data.credentials;

      return bridgeAddress.has_value() && credentials.has_value();
    }
    catch(const std::exception& e){
      return false;
    }

    return true;
  }


  unsigned Config::restServerPort() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().restServerPort.value();
  }


  const std::string& Config::boundBackendIP() const
  {
    return m_configData.value().boundBackendIP.value();
  }


  unsigned Config::refreshRate() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().refreshRate.value();
  }


  unsigned Config::subsampleWidth() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().subsampleWidth.value();
  }


  float Config::transitionSmoothing() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().transitionSmoothing.value();
  }


  float Config::brightness() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().brightness.value();
  }


  const std::optional<std::string>& Config::restoreToken() const
  {
    return m_configData->restoreToken;
  }


  const std::optional<std::string>& Config::bridgeAddress() const
  {
    return m_configData.value().bridgeAddress;
  }


  const std::optional<std::string>& Config::profileName() const
  {
    return m_configData.value().profileName;
  }


  const std::optional<Hue::Auth::Credentials>& Config::credentials() const
  {
    return m_configData.value().credentials;
  }


  void Config::setBridgeAddress(
    const std::string& bridgeAddress
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().bridgeAddress.emplace(bridgeAddress);
    _save();
  }


  void Config::setCredentials(
    const Hue::Auth::Credentials& credentials
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().credentials.emplace(credentials);
    _save();
  }


  void Config::setProfileName(
    const std::string& profileName
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().profileName.emplace(profileName);
    _save();
  }


  void Config::setSubsampleWidth(
    unsigned subsampleWidth
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().subsampleWidth = std::max(subsampleWidth, 1u);
    _save();
  }


  void Config::setRefreshRate(
    unsigned refreshRate
  )
  {
    std::lock_guard lock(m_mutex);
    if(refreshRate < 1){
      refreshRate = 1;
    }

    m_configData.value().refreshRate = refreshRate;
    _save();
  }


  void Config::setTransitionSmoothing(
    float transitionSmoothing
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().transitionSmoothing = std::clamp(transitionSmoothing, 0.f, 0.97f);
    _save();
  }


  void Config::setBrightness(
    float brightness
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().brightness = std::clamp(brightness, 0.f, 1.0f);
    _save();
  }


  void Config::setRestoreToken(
    const std::string& restoreToken
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData->restoreToken = restoreToken;
    _save();
  }


  bool Config::_loadConfigFile()
  {
    const Data defaultConfigData = {
      .restServerPort = 8215,
      .boundBackendIP = "127.0.0.1",
      .bridgeAddress = std::nullopt,
      .credentials = std::nullopt,
      .profileName = std::nullopt,
      .refreshRate = 0,
      .subsampleWidth = 0,
      .transitionSmoothing = 0.f,
      .brightness = 1.f,
      .restoreToken = std::nullopt
    };

    Json jsonConfig = Json::object();
    if(!std::filesystem::exists(m_configFilePath)){
      m_configData.emplace(defaultConfigData);
      _save();
      return false;
    }

    try{
      jsonConfig = Json::parse(std::ifstream(m_configFilePath));
      m_configData = jsonConfig.get<Data>();
    }
    catch(const std::exception& e){
      // Don't overwrite the file with defaults: it holds the bridge credentials
      throw std::runtime_error("Invalid configuration file " + m_configFilePath.string() + " (" + e.what() + "). Please fix or remove it.");
    }

    bool requireSave = false;

    auto& configData = m_configData.value();

    if(!configData.restServerPort.has_value()){
      configData.restServerPort = defaultConfigData.restServerPort.value();
      requireSave = true;
    }

    if(!configData.boundBackendIP.has_value()){
      configData.boundBackendIP = defaultConfigData.boundBackendIP.value();
      requireSave = true;
    }

    if(
      !configData.bridgeAddress.has_value()
      ||
      !configData.credentials.has_value()
    ){
      Logger::warn("Incomplete config. Initial setup is required");
      return false;
    }

    if(requireSave){
      _save();
    }

    return true;
  }


  void Config::_save() const
  {
    if(!std::filesystem::exists(m_configFilePath)){
      std::filesystem::create_directories(m_configFilePath.parent_path());
    }

    // Write then rename, so an interrupted write never leaves a truncated config behind
    std::filesystem::path tmpPath = m_configFilePath;
    tmpPath += ".tmp";

    {
      std::ofstream configFile(tmpPath);
      configFile << Json(m_configData.value()).dump(2) << "\n";
      if(!configFile){
        Logger::error("Failed to write configuration file ", tmpPath.string());
        return;
      }
    }

    std::filesystem::rename(tmpPath, m_configFilePath);
  }
}
