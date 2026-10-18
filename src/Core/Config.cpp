#include <Halomux/Core/Config.hpp>

#include <Halomux/Core/AtomicFile.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Serialization/Config.hpp>


namespace Halomux::Core
{
  using namespace Serialization;

  namespace
  {
    constexpr std::filesystem::perms OwnerOnly = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write;

    // Above, colors would barely move: at 1 they never would
    constexpr float MaxTransitionSmoothing = 0.97f;
  }

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
    std::lock_guard lock(m_mutex);
    const auto& data = m_configData.value();
    return data.bridgeAddress.has_value() && data.credentials.has_value();
  }


  unsigned Config::restServerPort() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().restServerPort.value();
  }


  std::string Config::boundBackendIP() const
  {
    // A copy: the setup's threads may change it meanwhile
    std::lock_guard lock(m_mutex);
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


  float Config::highlightBoost() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().highlightBoost.value();
  }


  bool Config::audioReactive() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().audioReactive.value();
  }


  float Config::brightness() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().brightness.value();
  }


  bool Config::syncEnabled() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().syncEnabled.value();
  }


  Config::StaticColors Config::staticColors() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().staticColors.value();
  }


  Config::Scenes Config::scenes() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().scenes.value();
  }


  std::string Config::language() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().language.value();
  }


  std::optional<std::string> Config::restoreToken() const
  {
    // A copy: the setup's threads may change it meanwhile
    std::lock_guard lock(m_mutex);
    return m_configData.value().restoreToken;
  }


  std::optional<std::string> Config::bridgeAddress() const
  {
    // A copy: the setup's threads may change it meanwhile
    std::lock_guard lock(m_mutex);
    return m_configData.value().bridgeAddress;
  }


  std::optional<std::string> Config::profileName() const
  {
    // A copy: the setup's threads may change it meanwhile
    std::lock_guard lock(m_mutex);
    return m_configData.value().profileName;
  }


  std::optional<Hue::Auth::Credentials> Config::credentials() const
  {
    // A copy: the setup's threads may change it meanwhile
    std::lock_guard lock(m_mutex);
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


  void Config::setAudioReactive(
    bool audioReactive
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().audioReactive = audioReactive;
    _save();
  }


  void Config::setHighlightBoost(
    float highlightBoost
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().highlightBoost = std::clamp(highlightBoost, 0.f, 1.0f);
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


  void Config::setSyncEnabled(
    bool syncEnabled
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().syncEnabled = syncEnabled;
    _save();
  }


  void Config::setStaticColors(
    const StaticColors& staticColors
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().staticColors = staticColors;
    _save();
  }


  void Config::saveScene(
    const std::string& name,
    const StaticColors& staticColors
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().scenes.value()[name] = staticColors;
    _save();
  }


  void Config::setLanguage(
    const std::string& language
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().language = language;
    _save();
  }


  std::optional<std::string> Config::bridgeId() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().bridgeId;
  }


  bool Config::bridgeCertificateVerified() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().bridgeCertificateVerified.value_or(false);
  }


  std::optional<Config::NoticeAcceptance> Config::noticeAccepted() const
  {
    std::lock_guard lock(m_mutex);
    return m_configData.value().noticeAccepted;
  }


  void Config::setBridgeIdentity(
    const std::string& bridgeId,
    bool certificateVerified
  )
  {
    std::lock_guard lock(m_mutex);
    auto& data = m_configData.value();

    // Verified once, a bridge must always be: but that binds this bridge, not another one
    const auto lowercase = [](std::string id){
      std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){return std::tolower(c);});
      return id;
    };
    const bool sameBridge = data.bridgeId.has_value() && lowercase(*data.bridgeId) == lowercase(bridgeId);

    data.bridgeId = bridgeId;
    data.bridgeCertificateVerified = (sameBridge && data.bridgeCertificateVerified.value_or(false)) || certificateVerified;
    _save();
  }


  void Config::setNoticeAccepted(
    const NoticeAcceptance& acceptance
  )
  {
    std::lock_guard lock(m_mutex);
    m_configData.value().noticeAccepted = acceptance;
    _save();
  }


  bool Config::deleteScene(
    const std::string& name
  )
  {
    std::lock_guard lock(m_mutex);
    if(m_configData.value().scenes.value().erase(name) == 0){
      return false;
    }

    _save();
    return true;
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
      .highlightBoost = 0.f,
      .audioReactive = false,
      .brightness = 1.f,
      .syncEnabled = true,
      .staticColors = StaticColors{},
      .scenes = Scenes{},
      .language = "auto",
      .noticeAccepted = std::nullopt,
      .bridgeId = std::nullopt,
      .bridgeCertificateVerified = false,
      .restoreToken = std::nullopt
    };

    Json jsonConfig = Json::object();
    if(!std::filesystem::exists(m_configFilePath)){
      m_configData.emplace(defaultConfigData);
      _save();
      return false;
    }

    // Files written by former versions were readable by every user
    std::error_code permissionsError;
    if((std::filesystem::status(m_configFilePath).permissions() & ~OwnerOnly) != std::filesystem::perms::none){
      std::filesystem::permissions(m_configFilePath, OwnerOnly, std::filesystem::perm_options::replace, permissionsError);
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

    // Hand-edited values get the limits of the setters: a smoothing of 1 would freeze the colors
    const auto limit = [&requireSave](std::optional<float>& value, float min, float max){
      if(value.has_value() && (!std::isfinite(*value) || *value < min || *value > max)){
        value = std::isfinite(*value) ? std::clamp(*value, min, max) : min;
        requireSave = true;
      }
    };
    limit(configData.transitionSmoothing, 0.f, MaxTransitionSmoothing);
    limit(configData.highlightBoost, 0.f, 1.f);
    limit(configData.brightness, 0.f, 1.f);

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
    // Holds the bridge credentials: written readable by its owner only, and never left truncated
    writeFileAtomically(m_configFilePath, Json(m_configData.value()).dump(2) + "\n");
  }
}
