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


  void Config::setCredentials(
    const Hue::Auth::Credentials& credentials
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().credentials.emplace(credentials);
    _save(lock);
  }


  void Config::setProfileName(
    const std::string& profileName
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().profileName.emplace(profileName);
    _save(lock);
  }


  void Config::setSubsampleWidth(
    unsigned subsampleWidth
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().subsampleWidth = std::clamp(subsampleWidth, 1u, MaxSubsampleWidth);
    _save(lock);
  }


  void Config::setRefreshRate(
    unsigned refreshRate
  )
  {
    std::unique_lock lock(m_mutex);
    if(refreshRate < 1){
      refreshRate = 1;
    }

    m_configData.value().refreshRate = refreshRate;
    _save(lock);
  }


  void Config::setTransitionSmoothing(
    float transitionSmoothing
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().transitionSmoothing = std::clamp(transitionSmoothing, 0.f, 0.97f);
    _save(lock);
  }


  void Config::setAudioReactive(
    bool audioReactive
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().audioReactive = audioReactive;
    _save(lock);
  }


  void Config::setHighlightBoost(
    float highlightBoost
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().highlightBoost = std::clamp(highlightBoost, 0.f, 1.0f);
    _save(lock);
  }


  void Config::setBrightness(
    float brightness
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().brightness = std::clamp(brightness, 0.f, 1.0f);
    _save(lock);
  }


  void Config::setSyncEnabled(
    bool syncEnabled
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().syncEnabled = syncEnabled;
    _save(lock);
  }


  void Config::setStaticColors(
    const StaticColors& staticColors
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().staticColors = staticColors;
    _save(lock);
  }


  void Config::saveScene(
    const std::string& name,
    const StaticColors& staticColors
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().scenes.value()[name] = staticColors;
    _save(lock);
  }


  void Config::setLanguage(
    const std::string& language
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().language = language;
    _save(lock);
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


  bool Config::_isSavedBridge(
    const Data& data,
    const std::string& bridgeId
  )
  {
    const auto lowercase = [](std::string id){
      std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){return std::tolower(c);});
      return id;
    };

    return data.bridgeId.has_value() && lowercase(*data.bridgeId) == lowercase(bridgeId);
  }


  bool Config::bridgeVerifiedWith(
    const std::string& bridgeId,
    bool certificateVerified
  ) const
  {
    std::lock_guard lock(m_mutex);
    const auto& data = m_configData.value();

    // Verified once, a bridge must always be: but that binds this bridge, not another one
    return certificateVerified || (_isSavedBridge(data, bridgeId) && data.bridgeCertificateVerified.value_or(false));
  }


  void Config::setBridge(
    const std::string& bridgeAddress,
    const std::string& bridgeId,
    bool certificateVerified
  )
  {
    const bool verified = bridgeVerifiedWith(bridgeId, certificateVerified);

    std::unique_lock lock(m_mutex);
    auto& data = m_configData.value();

    if(!_isSavedBridge(data, bridgeId) || data.bridgeAddress != bridgeAddress){
      data.credentials.reset();
    }

    data.bridgeAddress = bridgeAddress;
    data.bridgeId = bridgeId;
    data.bridgeCertificateVerified = verified;
    _save(lock);
  }


  void Config::setBridgeIdentity(
    const std::string& bridgeId,
    bool certificateVerified
  )
  {
    const bool verified = bridgeVerifiedWith(bridgeId, certificateVerified);

    std::unique_lock lock(m_mutex);
    auto& data = m_configData.value();
    data.bridgeId = bridgeId;
    data.bridgeCertificateVerified = verified;
    _save(lock);
  }


  void Config::setNoticeAccepted(
    const NoticeAcceptance& acceptance
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData.value().noticeAccepted = acceptance;
    _save(lock);
  }


  bool Config::deleteScene(
    const std::string& name
  )
  {
    std::unique_lock lock(m_mutex);
    if(m_configData.value().scenes.value().erase(name) == 0){
      return false;
    }

    _save(lock);
    return true;
  }


  void Config::setRestoreToken(
    const std::string& restoreToken
  )
  {
    std::unique_lock lock(m_mutex);
    m_configData->restoreToken = restoreToken;
    _save(lock);
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
      std::unique_lock lock(m_mutex);
      _save(lock);
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

    // 0 picks one automatically; a negative width reads as a huge one
    if(configData.subsampleWidth.has_value() && *configData.subsampleWidth > MaxSubsampleWidth){
      configData.subsampleWidth = MaxSubsampleWidth;
      requireSave = true;
    }

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
      std::unique_lock lock(m_mutex);
      _save(lock);
    }

    return true;
  }


  void Config::_save(
    std::unique_lock<std::mutex>& lock
  )
  {
    const std::string content = Json(m_configData.value()).dump(2) + "\n";
    const uint64_t generation = ++m_changeGeneration;

    // Written without the lock: the streaming loop reads settings while the disk syncs
    lock.unlock();

    // In the order of the changes: a slower write of an older one never replaces a newer one
    std::lock_guard writeLock(m_writeMutex);
    if(generation < m_writtenGeneration){
      return;
    }
    m_writtenGeneration = generation;

    // Holds the bridge credentials: written readable by its owner only, and never left truncated
    writeFileAtomically(m_configFilePath, content);
  }
}
