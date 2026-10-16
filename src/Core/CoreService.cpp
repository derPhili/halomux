#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <optional>

#include <glm/common.hpp>

#include <Huenicorn/Version.hpp>
#include <Huenicorn/Core/CoreService.hpp>
#include <Huenicorn/Core/Logger.hpp>

#include <Huenicorn/Hue/Api/ApiTools.hpp>
#include <Huenicorn/Hue/Api/BridgeAddress.hpp>
#include <Huenicorn/Hue/Api/BridgeTls.hpp>
#include <Huenicorn/Serialization/Json.hpp>
#include <Huenicorn/Serialization/EntertainmentConfiguration.hpp>
#include <Huenicorn/Serialization/JsonSerializer.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>
#include <Huenicorn/Core/Notice.hpp>
#include <Huenicorn/Core/PreviewSequence.hpp>
#include <Huenicorn/Core/Scenes.hpp>
#include <Huenicorn/Core/Runtime.hpp>
#include <Huenicorn/Network/Http/Server/SetupBackend.hpp>


namespace Huenicorn::Core
{
  CoreService::CoreService(Runtime& runtime, Config& config):
  m_runtime(runtime),
  m_config(config)
  {

  }


  const std::string& CoreService::version() const
  {
    return Huenicorn::Version;
  }


  const std::filesystem::path CoreService::configFilePath() const
  {
    return m_config.configFilePath();
  }


  bool CoreService::validateBridgeAddress(
    const std::string& bridgeAddress
  )
  {
    // Trailing slashes are already stripped by the sanitization
    std::string sanitizedAddress = Hue::Api::sanitizeBridgeAddress(bridgeAddress);

    if(sanitizedAddress.empty()){
      return false;
    }

    // Not identified yet: only the CA of its certificate can be checked
    auto response = Network::Http::Client::sendRequest(Hue::Api::HttpProtocol + sanitizedAddress + "/api/0/config", "GET", "", {}, Hue::Api::bridgeTls(sanitizedAddress));
    if(!response.has_value()){
      return false;
    }

    try{
      auto responseData = response->asJson();
      if(!responseData.contains("name") || !responseData.contains("bridgeid")){
        return false;
      }

      m_config.setBridgeAddress(sanitizedAddress);
      m_config.setBridgeIdentity(responseData.at("bridgeid"), response->tlsVerified());
      _registerBridge();

      return true;
    }
    catch(const std::exception& e){
      Logger::error(e.what());
      return false;
    }
  }


  bool CoreService::validateCredentials(
    const Hue::Auth::Credentials& credentials
  )
  {
    const std::string& bridgeAddress = m_config.bridgeAddress().value();
    const Network::Http::Client::Headers headers = {{"hue-application-key", credentials.username()}};
    auto response = Network::Http::Client::sendRequest(Hue::Api::HttpProtocol + bridgeAddress + "/clip/v2/resource/bridge", "GET", "", headers, Hue::Api::bridgeTls(bridgeAddress));
    if(!response.has_value()){
      return false;
    }

    try{
      // An unknown application key gets errors and no data
      auto jsonResponse = response.value().asJson();
      if(!jsonResponse.contains("data") || jsonResponse.at("data").empty()){
        return false;
      }
    }
    catch(const std::exception& e){
      Logger::error(e.what());
      return false;
    }

    m_config.setCredentials(credentials);
    Logger::log("Successfully registered credentials");
    return true;
  }


  Serialization::Json CoreService::registerNewUser()
  {
    auto response = Hue::Api::ApiTools::registerNewUser(m_config.bridgeAddress().value());

    if(!response.has_value()){
      return {{"succeeded", false}, {"error", "unreachable bridge"}};
    }

    auto jsonResponse = response.value().asJson();

    if(jsonResponse.at(0).contains("error")){
      return {{"succeeded", false}, {"error", jsonResponse.at(0).at("error").at("description")}};
    }

    auto credentials = jsonResponse.at(0).at("success").get<Hue::Auth::Credentials>();
    m_config.setCredentials(credentials);

    return {{"succeeded", true}, {"username", credentials.username()}, {"clientkey", credentials.clientkey()}};
  }



  Serialization::Json CoreService::bridgeEntertainmentAreas() const
  {
    Serialization::Json jsonAreas = Serialization::Json::array();

    const auto& bridgeAddress = m_config.bridgeAddress();
    const auto& credentials = m_config.credentials();
    if(!bridgeAddress.has_value() || !credentials.has_value()){
      return jsonAreas;
    }

    try{
      for(const auto& [id, area] : Hue::Api::ApiTools::loadEntertainmentConfigurations(credentials->username(), bridgeAddress.value())){
        jsonAreas.push_back({{"name", area.name}, {"lamps", area.devices.size()}});
      }
    }
    catch(const std::exception& e){
      Logger::error("Could not list the entertainment areas: ", e.what());
    }

    return jsonAreas;
  }


  const Hue::Api::EntertainmentConfigurations& CoreService::entertainmentConfigurations() const
  {
    return m_runtime.m_selector->entertainmentConfigurations();
  }


  std::optional<std::string> CoreService::currentEntertainmentConfigurationId() const
  {
    std::lock_guard lock(m_runtime.m_selectorMutex);
    return m_runtime.m_selector->currentEntertainmentConfigurationId();
  }


  bool CoreService::setChannelActivity(
    uint8_t channelId,
    bool active
  )
  {
    std::lock_guard lock(m_runtime.m_channelsMutex);
    auto& channels = m_runtime.m_channels;
    if(channels.find(channelId) == channels.end()){
      return false;
    }

    auto& channel = channels.at(channelId);

    // A lamp set up for the first time follows the monitor most other lamps follow
    if(active && !channel.monitor.has_value() && !m_runtime.m_monitors.empty()){
      std::vector<size_t> monitorsOfActiveLamps;
      for(const auto& [otherId, other] : channels){
        if(other.state == Hue::Api::Channel::State::Active && other.monitorIndex.has_value()){
          monitorsOfActiveLamps.push_back(*other.monitorIndex);
        }
      }

      const size_t monitor = Grabber::defaultMonitorForNewLamp(monitorsOfActiveLamps, m_runtime.m_monitors.size());
      channel.monitor = Grabber::monitorRef(m_runtime.m_monitors[monitor]);
      channel.monitorIndex = monitor;
    }

    channel.setActive(active);

    return true;
  }


  Hue::Api::Channels CoreService::channels() const
  {
    std::lock_guard lock(m_runtime.m_channelsMutex);
    return m_runtime.m_channels;
  }


  Hue::Api::ChannelStreams CoreService::currentColors() const
  {
    std::lock_guard lock(m_runtime.m_currentColorsMutex);
    return m_runtime.m_currentColorsSnapshot;
  }


  const Hue::Api::EntertainmentConfiguration& CoreService::currentEntertainmentConfiguration() const
  {
    std::lock_guard lock(m_runtime.m_selectorMutex);
    return m_runtime.m_selector->currentEntertainmentConfiguration();
  }



  glm::ivec2 CoreService::displayResolution() const
  {
    return m_runtime.grabber()->displayResolution();
  }


  std::vector<glm::ivec2> CoreService::subsampleResolutionCandidates() const
  {
    return m_runtime.grabber()->subsampleResolutionCandidates();
  }


  unsigned CoreService::subsampleWidth() const
  {
    return m_config.subsampleWidth();
  }


  unsigned CoreService::refreshRate() const
  {
    return m_config.refreshRate();
  }


  unsigned CoreService::maxRefreshRate() const
  {
    return std::min(m_runtime.grabber()->displayRefreshRate(), Stream::MaxStreamingRate);
  }


  Grabber::IGrabber::ProcessingInfo CoreService::processingInfo() const
  {
    return m_runtime.grabber()->processingInfo();
  }


  unsigned CoreService::displayRefreshRate() const
  {
    return m_runtime.grabber()->displayRefreshRate();
  }


  float CoreService::transitionSmoothing() const
  {
    // Config stores a raw [0, 1) blend factor; the API/UI works in percent.
    return m_config.transitionSmoothing() * 100.f;
  }


  float CoreService::brightness() const
  {
    return m_config.brightness() * 100.f;
  }


  bool CoreService::syncEnabled() const
  {
    return m_config.syncEnabled();
  }


  Config::StaticColors CoreService::staticColors() const
  {
    return m_config.staticColors();
  }


  Config::Scenes CoreService::scenes() const
  {
    return m_config.scenes();
  }


  std::string CoreService::language() const
  {
    return m_config.language();
  }


  float CoreService::highlightBoost() const
  {
    return m_config.highlightBoost() * 100.f;
  }


  float CoreService::startPreview()
  {
    m_runtime.m_previewStart = std::chrono::steady_clock::now().time_since_epoch().count();
    return PreviewSequence::Duration;
  }


  void CoreService::stopPreview()
  {
    m_runtime.m_previewStart = 0;
  }


  float CoreService::previewRemaining() const
  {
    const int64_t previewStart = m_runtime.m_previewStart;
    if(previewStart == 0){
      return 0.f;
    }

    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch() - std::chrono::steady_clock::duration(previewStart);
    return std::max(0.f, PreviewSequence::Duration - std::chrono::duration<float>(elapsed).count());
  }


  bool CoreService::audioReactive() const
  {
    return m_config.audioReactive();
  }


  void CoreService::setAudioReactive(
    bool audioReactive
  )
  {
    m_config.setAudioReactive(audioReactive);
  }


  bool CoreService::setEntertainmentConfiguration(
    const std::string& entertainmentConfigurationId
  )
  {
    std::lock_guard lock(m_runtime.m_selectorMutex);
    return m_runtime._enableEntertainmentConfiguration(entertainmentConfigurationId);
  }


  std::optional<Imaging::UVs> CoreService::setChannelUV(
    uint8_t channelId,
    Imaging::UV&& uv,
    Imaging::UVCorner uvCorner
  )
  {
    std::lock_guard lock(m_runtime.m_channelsMutex);
    auto channelIt = m_runtime.m_channels.find(channelId);
    if(channelIt == m_runtime.m_channels.end()){
      return std::nullopt;
    }

    return channelIt->second.setUV(std::move(uv), uvCorner);
  }


  std::optional<Imaging::UVs> CoreService::setChannelMonitor(
    uint8_t channelId,
    size_t monitor,
    const Imaging::UVs& uvs
  )
  {
    std::lock_guard lock(m_runtime.m_channelsMutex);
    auto channelIt = m_runtime.m_channels.find(channelId);
    if(channelIt == m_runtime.m_channels.end() || monitor >= m_runtime.m_monitors.size()){
      return std::nullopt;
    }

    const auto clamp = [](const Imaging::UV& uv){
      return Imaging::UV{std::clamp(uv.x, 0.f, 1.f), std::clamp(uv.y, 0.f, 1.f)};
    };

    const Imaging::UV a = clamp(uvs.min);
    const Imaging::UV b = clamp(uvs.max);

    auto& channel = channelIt->second;
    channel.uvs = {glm::min(a, b), glm::max(a, b)};
    channel.monitor = Grabber::monitorRef(m_runtime.m_monitors[monitor]);
    channel.monitorIndex = monitor;

    return channel.uvs;
  }


  Grabber::Monitors CoreService::monitors() const
  {
    return m_runtime.grabber()->monitors();
  }


  bool CoreService::reselectMonitors()
  {
    return m_runtime._reselectMonitors();
  }


  MonitorSelection CoreService::monitorSelection() const
  {
    return m_runtime.m_monitorSelection;
  }


  bool CoreService::setChannelGammaFactor(
    uint8_t channelId,
    float gammaExponent
  )
  {
    if(!std::isfinite(gammaExponent)){
      return false;
    }

    std::lock_guard lock(m_runtime.m_channelsMutex);
    auto& channels = m_runtime.m_channels;

    if(channels.find(channelId) == channels.end()){
      return false;
    }

    channels.at(channelId).gammaFactor = gammaExponent;
    return true;
  }


  void CoreService::setSubsampleWidth(
    unsigned subsampleWidth
  )
  {
    m_config.setSubsampleWidth(subsampleWidth);
  }


  void CoreService::setRefreshRate(
    unsigned refreshRate
  )
  {
    refreshRate = std::min(refreshRate, maxRefreshRate());

    m_config.setRefreshRate(refreshRate);
    refreshRate = m_config.refreshRate();

    if(m_runtime.m_loopRegulator){
      m_runtime.m_loopRegulator->setTickInterval(Timing::fromHertz(refreshRate));
    }

    m_runtime.grabber()->setFrameRateLimit(refreshRate);
  }


  void CoreService::setTransitionSmoothing(
    float transitionSmoothing
  )
  {
    m_config.setTransitionSmoothing(transitionSmoothing / 100.f);
  }


  void CoreService::setBrightness(
    float brightness
  )
  {
    if(std::isfinite(brightness)){
      m_config.setBrightness(brightness / 100.f);
    }
  }


  void CoreService::setSyncEnabled(
    bool syncEnabled
  )
  {
    m_config.setSyncEnabled(syncEnabled);
  }


  void CoreService::setStaticColors(
    const Config::StaticColors& staticColors
  )
  {
    m_config.setStaticColors(staticColors);
  }


  bool CoreService::saveScene(
    const std::string& name,
    const Config::StaticColors& staticColors
  )
  {
    static constexpr size_t MaxNameLength = 40;

    const auto first = name.find_first_not_of(" \t");
    if(first == std::string::npos){
      return false;
    }

    const std::string trimmedName = name.substr(first, name.find_last_not_of(" \t") - first + 1);
    if(trimmedName.size() > MaxNameLength){
      return false;
    }

    m_config.saveScene(trimmedName, staticColors);
    return true;
  }


  bool CoreService::setLanguage(
    const std::string& language
  )
  {
    if(language != "auto" && language != "en" && language != "de"){
      return false;
    }

    m_config.setLanguage(language);
    return true;
  }


  void CoreService::acceptNotice()
  {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    m_config.setNoticeAccepted({.version = NoticeVersion, .at = std::format("{:%FT%TZ}", now)});
  }


  bool CoreService::noticeAccepted() const
  {
    const auto acceptance = m_config.noticeAccepted();
    return acceptance.has_value() && acceptance->version >= NoticeVersion;
  }


  bool CoreService::deleteScene(
    const std::string& name
  )
  {
    return m_config.deleteScene(name);
  }


  bool CoreService::applyScene(
    const std::string& name,
    bool builtin
  )
  {
    Config::StaticColors sceneColors;

    if(builtin){
      const Scenes::BuiltinScene* scene = Scenes::findBuiltin(name);
      if(!scene){
        return false;
      }

      // Colors are spread from left to right over the whole desktop
      std::vector<std::pair<uint8_t, float>> lampCenters;
      {
        std::lock_guard lock(m_runtime.m_channelsMutex);
        for(const auto& [channelId, channel] : m_runtime.m_channels){
          if(channel.state == Hue::Api::Channel::State::Active){
            lampCenters.emplace_back(channelId, m_runtime._desktopCenterX(channel));
          }
        }
      }

      sceneColors = Scenes::spread(*scene, std::move(lampCenters));
    }
    else{
      const Config::Scenes savedScenes = m_config.scenes();
      const auto sceneIt = savedScenes.find(name);
      if(sceneIt == savedScenes.end()){
        return false;
      }

      sceneColors = sceneIt->second;
    }

    // Lamps the scene doesn't cover keep their color
    Config::StaticColors staticColors = m_config.staticColors();
    for(const auto& [channelId, color] : sceneColors){
      staticColors[channelId] = color;
    }

    m_config.setStaticColors(staticColors);
    m_config.setSyncEnabled(false);

    return true;
  }


  void CoreService::setHighlightBoost(
    float highlightBoost
  )
  {
    if(std::isfinite(highlightBoost)){
      m_config.setHighlightBoost(highlightBoost / 100.f);
    }
  }


  void CoreService::saveProfile()
  {
    std::lock_guard selectorLock(m_runtime.m_selectorMutex);
    std::lock_guard channelsLock(m_runtime.m_channelsMutex);

    if(!m_runtime.m_selector->validSelection()){
      Logger::error("There is currently no valid entertainment configuration selected.");
      return;
    }

    Serialization::Json profile;
    if(m_runtime.m_selector->validSelection()){
      profile = Serialization::Json{
        {"entertainmentConfigurationId", m_runtime.m_selector->currentEntertainmentConfigurationId().value()},
        {"channels", m_runtime.m_channels}
      };
    }

    if(!m_config.profileName().has_value()){
      m_config.setProfileName("profile");
    }

    // Write then rename, so an interrupted write never leaves a truncated profile behind
    std::filesystem::path pp = profilePath();
    std::filesystem::path tmpPath = pp;
    tmpPath += ".tmp";

    {
      std::ofstream profileFile(tmpPath, std::ofstream::out);
      profileFile << profile.dump(2) << "\n";
      if(!profileFile){
        Logger::error("Failed to write profile ", std::quoted(tmpPath.string()));
        return;
      }
    }

    std::filesystem::rename(tmpPath, pp);
  }


  void CoreService::stop()
  {
    m_runtime.stop();
  }


  void CoreService::_identifyBridge()
  {
    // Bridges set up before the certificate checks, or whose certificate never passed them, are
    // identified again at each start: once their certificate passes the check, it always has to
    if(!m_config.bridgeId().has_value() || !m_config.bridgeCertificateVerified()){
      const std::string& bridgeAddress = m_config.bridgeAddress().value();
      auto response = Network::Http::Client::sendRequest(Hue::Api::HttpProtocol + bridgeAddress + "/api/0/config", "GET", "", {}, Hue::Api::bridgeTls(bridgeAddress));

      try{
        if(response.has_value() && response->asJson().contains("bridgeid")){
          m_config.setBridgeIdentity(response->asJson().at("bridgeid"), response->tlsVerified());
        }
      }
      catch(const std::exception& e){
        Logger::error("Could not identify the bridge: ", e.what());
      }
    }

    _registerBridge();
  }


  void CoreService::_registerBridge()
  {
    const auto bridgeId = m_config.bridgeId();
    if(!bridgeId.has_value()){
      return;
    }

    const bool verified = m_config.bridgeCertificateVerified();
    Hue::Api::registerBridge(m_config.bridgeAddress().value(), bridgeId.value(), verified);

    if(!verified){
      Logger::warn("The certificate of the bridge isn't signed by the Hue CA: its identity can't be verified, so connections to it aren't protected against impersonation");
    }
  }


  bool CoreService::ensureInitialSetup()
  {
    if(m_config.initialSetupOk()){
      _identifyBridge();
      return true;
    }

    Logger::log("Starting setup backend");
    unsigned port = m_config.restServerPort();
    const std::string& boundBackendIP = m_config.boundBackendIP();

    Network::Http::Server::SetupBackend sb(this);
    {
      std::lock_guard lock(m_setupBackendMutex);
      if(!m_runtime.m_keepLooping){
        return false;
      }

      m_setupBackend = &sb;
    }

    bool succeeded = sb.execute(port, boundBackendIP);

    {
      std::lock_guard lock(m_setupBackendMutex);
      m_setupBackend = nullptr;
    }

    m_runtime.m_openedSetup = true;

    if(!succeeded){
      Logger::log("Initial setup was aborted");
      return false;
    }

    Logger::log("Finished setup");

    return true;
  }


  void CoreService::abortInitialSetup()
  {
    std::lock_guard lock(m_setupBackendMutex);
    if(m_setupBackend){
      m_setupBackend->abort();
    }
  }


  std::filesystem::path CoreService::profilePath() const
  {
    if(!m_config.profileName().has_value()){
      return {};
    }

    return m_runtime.m_configRoot / std::filesystem::path(m_config.profileName().value()).replace_extension("json");
  }


  std::optional<Serialization::Json> CoreService::getProfile()
  {
    std::filesystem::path pp = profilePath();

    if(!pp.empty() && std::filesystem::exists(pp) && std::filesystem::is_regular_file(pp)){
      try{
        std::ifstream profileFile(pp);
        return Serialization::Json::parse(profileFile);
      }
      catch(const std::exception& e){
        Logger::error("Ignoring invalid profile ", std::quoted(pp.string()), ": ", e.what());
      }
    }

    return std::nullopt;
  }
}
