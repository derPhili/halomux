#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <string>

#include <glm/common.hpp>

#include <Halomux/Version.hpp>
#include <Halomux/Core/CoreService.hpp>

#include <Halomux/Core/AtomicFile.hpp>
#include <Halomux/Core/Logger.hpp>

#include <Halomux/Hue/Api/ApiTools.hpp>
#include <Halomux/Hue/Api/BridgeAddress.hpp>
#include <Halomux/Hue/Api/BridgeTls.hpp>
#include <Halomux/Hue/Api/ClipParsing.hpp>
#include <Halomux/Serialization/Json.hpp>
#include <Halomux/Serialization/EntertainmentConfiguration.hpp>
#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Serialization/Credentials.hpp>
#include <Halomux/Core/Notice.hpp>
#include <Halomux/Core/PreviewSequence.hpp>
#include <Halomux/Core/Profile.hpp>
#include <Halomux/Core/Scenes.hpp>
#include <Halomux/Core/Runtime.hpp>
#include <Halomux/Network/Http/Server/SetupBackend.hpp>


namespace Halomux::Core
{
  CoreService::CoreService(Runtime& runtime, Config& config):
  m_runtime(runtime),
  m_config(config)
  {

  }


  const std::string& CoreService::version() const
  {
    return Halomux::Version;
  }


  const std::filesystem::path CoreService::configFilePath() const
  {
    return m_config.configFilePath();
  }


  std::optional<bool> CoreService::validateBridgeAddress(
    const std::string& bridgeAddress
  )
  {
    // Until another bridge passes, none is to be paired with
    m_setupBridgeTrust.invalidate();

    // Trailing slashes are already stripped by the sanitization
    std::string sanitizedAddress = Hue::Api::sanitizeBridgeAddress(bridgeAddress);

    if(sanitizedAddress.empty()){
      return std::nullopt;
    }

    // Not identified yet: only the CA of its certificate can be checked
    auto response = Network::Http::Client::sendRequest(Hue::Api::HttpProtocol + sanitizedAddress + "/api/0/config", "GET", "", {}, Hue::Api::bridgeTls(sanitizedAddress));
    if(!response.has_value()){
      return std::nullopt;
    }

    try{
      auto responseData = response->asJson();
      // Its id names its certificate: without a valid one, it isn't a bridge to trust
      if(!responseData.contains("name") || !responseData.contains("bridgeid") || !responseData.at("bridgeid").is_string() || !Hue::Api::isBridgeId(responseData.at("bridgeid").get<std::string>())){
        return std::nullopt;
      }

      m_config.setBridgeAddress(sanitizedAddress);
      m_config.setBridgeIdentity(responseData.at("bridgeid"), response->tlsVerified());
      _registerBridge();

      // Verified once, the bridge is checked as verified from then on
      const bool verified = m_config.bridgeCertificateVerified();
      m_setupBridgeTrust.validated(sanitizedAddress, verified);
      return verified;
    }
    catch(const std::exception& e){
      Logger::error(e.what());
      return std::nullopt;
    }
  }


  bool CoreService::acceptUnverifiedBridge()
  {
    if(!m_setupBridgeTrust.acceptUnverified()){
      return false;
    }

    Logger::warn("Pairing with a bridge whose certificate isn't signed by the Hue CA, as confirmed by the user");
    return true;
  }


  bool CoreService::validateCredentials(
    const Hue::Auth::Credentials& credentials
  )
  {
    // The setup validates the address first, and pairs only with a bridge it trusts
    if(!m_config.bridgeAddress().has_value() || !m_setupBridgeTrust.trusted(*m_config.bridgeAddress())){
      return false;
    }

    // The bridge only checks the username: a mistyped clientkey would fail every streaming connection
    if(!credentials.hasValidClientkey()){
      Logger::error("The clientkey isn't 32 hexadecimal digits");
      return false;
    }

    const std::string bridgeAddress = m_config.bridgeAddress().value();
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
    // The setup validates the address first
    if(!m_config.bridgeAddress().has_value()){
      return {{"succeeded", false}, {"error", "no bridge address yet"}};
    }

    // Any device of the network could pass itself off as the bridge and keep the credentials
    if(!m_setupBridgeTrust.trusted(*m_config.bridgeAddress())){
      return {{"succeeded", false}, {"error", "the bridge isn't confirmed"}};
    }

    auto response = Hue::Api::ApiTools::registerNewUser(m_config.bridgeAddress().value());

    if(!response.has_value()){
      return {{"succeeded", false}, {"error", "unreachable bridge"}};
    }

    Hue::Api::ClipParsing::Pairing pairing;
    try{
      pairing = Hue::Api::ClipParsing::pairing(response.value().asJson());
    }
    catch(const std::exception&){
      return {{"succeeded", false}, {"error", "unexpected answer of the bridge"}};
    }

    if(!pairing.credentials.has_value()){
      return {{"succeeded", false}, {"error", pairing.error}};
    }

    auto credentials = pairing.credentials->get<Hue::Auth::Credentials>();
    m_config.setCredentials(credentials);

    // The credentials stay here: the page doesn't need them, and any local process may ask
    return {{"succeeded", true}};
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
      for(const auto& [id, area] : Hue::Api::ApiTools::loadEntertainmentConfigurations(credentials->username(), bridgeAddress.value()).value_or(Hue::Api::EntertainmentConfigurations{})){
        jsonAreas.push_back({{"name", area.name}, {"lamps", area.devices.size()}});
      }
    }
    catch(const std::exception& e){
      Logger::error("Could not list the entertainment areas: ", e.what());
    }

    return jsonAreas;
  }


  Hue::Api::EntertainmentConfigurations CoreService::entertainmentConfigurations() const
  {
    return m_runtime.m_areaSession ? m_runtime.m_areaSession->areas() : Hue::Api::EntertainmentConfigurations{};
  }


  Hue::Api::EntertainmentConfigurations CoreService::refreshEntertainmentConfigurations()
  {
    return m_runtime.m_areaSession ? m_runtime.m_areaSession->refreshAreas() : Hue::Api::EntertainmentConfigurations{};
  }


  std::optional<std::string> CoreService::currentEntertainmentConfigurationId() const
  {
    return m_runtime.m_areaSession ? m_runtime.m_areaSession->currentAreaId() : std::nullopt;
  }


  bool CoreService::setChannelActivity(
    uint8_t channelId,
    bool active
  )
  {
    return m_runtime.m_channelStore.setActivity(channelId, active);
  }


  Hue::Api::Channels CoreService::channels() const
  {
    return m_runtime.m_channelStore.snapshot();
  }


  Hue::Api::ChannelStreams CoreService::currentColors() const
  {
    std::lock_guard lock(m_runtime.m_currentColorsMutex);
    return m_runtime.m_currentColorsSnapshot;
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
    return m_runtime.m_areaSession && m_runtime.m_areaSession->select(entertainmentConfigurationId);
  }


  std::optional<Imaging::UVs> CoreService::setChannelUV(
    uint8_t channelId,
    Imaging::UV&& uv,
    Imaging::UVCorner uvCorner
  )
  {
    return m_runtime.m_channelStore.setUV(channelId, uv, uvCorner, m_config.subsampleWidth());
  }


  std::optional<Imaging::UVs> CoreService::setChannelMonitor(
    uint8_t channelId,
    size_t monitor,
    const Imaging::UVs& uvs
  )
  {
    return m_runtime.m_channelStore.setMonitor(channelId, monitor, uvs, m_config.subsampleWidth());
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
    return m_runtime.m_channelStore.setGammaFactor(channelId, gammaExponent);
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
      std::vector<std::pair<uint8_t, float>> lampCenters = m_runtime.m_channelStore.activeLampCenters();

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


  bool CoreService::saveProfile()
  {
    // Read, changed and written by one save at a time: concurrent saves would drop each other's
    // area, and an older snapshot written last would make its area the current one again
    std::lock_guard lock(m_profileMutex);

    // The lamps with the area they belong to, read together: an area switch can't mix them up
    const auto lamps = m_runtime.m_channelStore.areaSnapshot();
    if(!lamps.areaId.has_value()){
      Logger::error("There is currently no valid entertainment configuration selected.");
      return false;
    }

    // The other areas' lamp settings are kept
    const Serialization::Json profile = Profile::withArea(getProfile().value_or(Serialization::Json()), *lamps.areaId, Serialization::Json(lamps.channels));

    if(!m_config.profileName().has_value()){
      m_config.setProfileName("profile");
    }

    // Never left truncated, whatever happens while writing
    return writeFileAtomically(profilePath(), profile.dump(2) + "\n");
  }


  void CoreService::stop()
  {
    m_runtime.stop();
  }


  void CoreService::_identifyBridge()
  {
    // Bridges set up before the certificate checks, or whose certificate never passed them, are
    // identified again at each start: once their certificate passes the check, it always has to
    const auto savedId = m_config.bridgeId();
    if(!savedId.has_value() || !Hue::Api::isBridgeId(*savedId) || !m_config.bridgeCertificateVerified()){
      const std::string bridgeAddress = m_config.bridgeAddress().value();
      auto response = Network::Http::Client::sendRequest(Hue::Api::HttpProtocol + bridgeAddress + "/api/0/config", "GET", "", {}, Hue::Api::bridgeTls(bridgeAddress));

      try{
        if(response.has_value()){
          const auto answer = response->asJson();
          if(answer.contains("bridgeid") && answer.at("bridgeid").is_string() && Hue::Api::isBridgeId(answer.at("bridgeid").get<std::string>())){
            m_config.setBridgeIdentity(answer.at("bridgeid").get<std::string>(), response->tlsVerified());
          }
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
    if(!Hue::Api::registerBridge(m_config.bridgeAddress().value(), bridgeId.value(), verified)){
      Logger::error("The bridge answered no valid id: its certificate can't be checked against it");
      return;
    }

    if(!verified){
      Logger::warn("The certificate of the bridge isn't signed by the Hue CA: its identity can't be verified, so connections to it aren't protected against impersonation");
    }
  }


  bool CoreService::initialSetupComplete() const
  {
    return m_config.initialSetupOk();
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

    const std::optional<bool> completed = sb.execute(port, boundBackendIP);

    {
      std::lock_guard lock(m_setupBackendMutex);
      m_setupBackend = nullptr;
    }

    // Not the user's choice: the service fails, and is started again
    if(!completed.has_value()){
      throw std::runtime_error("Could not open the setup on " + boundBackendIP + ":" + std::to_string(port) + ", which may be in use");
    }

    m_runtime.m_openedSetup = true;

    if(!*completed){
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
