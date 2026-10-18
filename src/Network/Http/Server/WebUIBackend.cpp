#include <Halomux/Network/Http/Server/WebUIBackend.hpp>

#include <filesystem>

#include <Halomux/Core/CoreService.hpp>
#include <Halomux/Network/Http/Server/RouteTools.hpp>
#include <Halomux/Core/Scenes.hpp>
#include <Halomux/Serialization/Channel.hpp>
#include <Halomux/Serialization/Config.hpp>
#include <Halomux/Serialization/Json.hpp>
#include <Halomux/Serialization/EntertainmentConfiguration.hpp>
#include <Halomux/Stream/Streamer.hpp>


namespace Halomux::Network::Http::Server
{
  using namespace Serialization;


  namespace
  {
    // Channel as the light manager sees it: the index of its monitor instead of the stored reference
    Json webChannel(
      const Hue::Api::Channel& channel
    )
    {
      Json jsonChannel = Json(channel);
      jsonChannel["monitor"] = channel.monitorIndex.has_value() ? Json(*channel.monitorIndex) : Json(nullptr);
      jsonChannel["monitorMissing"] = channel.state == Hue::Api::Channel::State::Active && !channel.monitorIndex.has_value();
      return jsonChannel;
    }


    Json webChannels(
      const Hue::Api::Channels& channels
    )
    {
      Json jsonChannels = Json::array();
      for(const auto& [channelId, channel] : channels){
        Json jsonChannel = webChannel(channel);
        jsonChannel["channelId"] = channelId;
        jsonChannels.push_back(std::move(jsonChannel));
      }

      return jsonChannels;
    }
  }


  WebUIBackend::WebUIBackend(
    Halomux::Core::CoreService* coreService
  ):
  m_coreService(coreService)
  {
    // WEB PAGES

    m_httpServer.addRoute(HttpMethod::Get, "/", [](const Request& /*req*/, Response& res){
      std::filesystem::path pageName = "index.html";
      Utils::getWebFile(res, pageName);
    });

    m_httpServer.addRoute(HttpMethod::Get, "/:pageName", [](const Request& req, Response& res){
      std::filesystem::path pageName = req.pathParams.at("pageName");

      if(pageName == "setup.html"){
        pageName = "404.html";
      }

      Utils::getWebFile(res, pageName);
    });


    // GET METHODS

    m_httpServer.addRoute(HttpMethod::Get, "/api/version", [this](const Request& /*req*/, Response& res){
      Serialization::Json jsonResponse = {
        {"version", m_coreService->version()},
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/webUIStatus", [](const Request& /*req*/, Response& res){
      Json jsonResponse = {{
        "ready", true
      }};

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/entertainmentConfigurations", [this](const Request& /*req*/, Response& res){
      // Asked again, so that areas created in the Hue app since show up when the page loads
      const auto entertainmentConfigurations = m_coreService->refreshEntertainmentConfigurations();
      // None while the bridge is unreachable or has no area yet
      const auto currentEntertainmentConfigurationId = m_coreService->currentEntertainmentConfigurationId();

      Json jsonResponse = {
        {"entertainmentConfigurations", entertainmentConfigurations},
        {"currentEntertainmentConfigurationId", currentEntertainmentConfigurationId.has_value() ? Json(*currentEntertainmentConfigurationId) : Json(nullptr)}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/channel/:channelId", [this](const Request& req, Response& res){
      const uint8_t channelId = channelIdOf(req);

      const auto channels = m_coreService->channels();
      const auto channelIt = channels.find(channelId);
      if(channelIt == channels.end()){
        throw HttpError(404, "unknown channel");
      }

      res.contentType = "application/json";
      res.body = webChannel(channelIt->second).dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/channels", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = webChannels(m_coreService->channels()).dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/currentColors", [this](const Request& /*req*/, Response& res){
      Json jsonResponse = Json::array();
      for(const auto& channelStream : m_coreService->currentColors()){
        jsonResponse.push_back({
          {"channelId", channelStream.id},
          {"r", channelStream.r},
          {"g", channelStream.g},
          {"b", channelStream.b}
        });
      }

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/displayInfo", [this](const Request& /*req*/, Response& res){
      auto displayResolution = m_coreService->displayResolution();

      Json jsonSubsampleCandidates = Json::array();
      for(const auto& candidate : m_coreService->subsampleResolutionCandidates()){
        jsonSubsampleCandidates.push_back({
          {"x", candidate.x},
          {"y", candidate.y}
        });
      }

      Json jsonMonitors = Json::array();
      const auto monitors = m_coreService->monitors();
      for(size_t i = 0; i < monitors.size(); i++){
        const auto& monitor = monitors[i];
        jsonMonitors.push_back({
          {"index", i},
          {"name", monitor.name},
          {"description", monitor.description},
          {"x", monitor.geometry.x},
          {"y", monitor.geometry.y},
          {"width", monitor.geometry.width},
          {"height", monitor.geometry.height},
          {"pixelWidth", monitor.pixelWidth},
          {"pixelHeight", monitor.pixelHeight},
          {"gpu", monitor.gpu},
          {"active", monitor.active},
          {"lost", monitor.lost}
        });
      }

      std::string monitorSelection = "idle";
      switch(m_coreService->monitorSelection()){
        case Core::MonitorSelection::Pending: monitorSelection = "pending"; break;
        case Core::MonitorSelection::Cancelled: monitorSelection = "cancelled"; break;
        case Core::MonitorSelection::Idle: break;
      }

      Json jsonDisplayInfo{
        {"monitors", jsonMonitors},
        {"monitorSelection", monitorSelection},
        {"x", displayResolution.x},
        {"y", displayResolution.y},
        {"subsampleWidth", m_coreService->subsampleWidth()},
        {"subsampleResolutionCandidates", jsonSubsampleCandidates},
        {"selectedRefreshRate", m_coreService->refreshRate()},
        {"maxRefreshRate", m_coreService->maxRefreshRate()},
        {"displayRefreshRate", m_coreService->displayRefreshRate()},
        {"bridgeMaxRefreshRate", Stream::MaxStreamingRate},
        {"selectedTransitionSmoothing", m_coreService->transitionSmoothing()},
        {"selectedHighlightBoost", m_coreService->highlightBoost()},
        {"selectedAudioReactive", m_coreService->audioReactive()},
        {"selectedBrightness", m_coreService->brightness()}
      };

      std::string response = jsonDisplayInfo.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/notice", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Json{{"accepted", m_coreService->noticeAccepted()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/language", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Json{{"language", m_coreService->language()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/lightMode", [this](const Request& /*req*/, Response& res){
      Json jsonBuiltinScenes = Json::array();
      for(const auto& scene : Core::Scenes::Builtin){
        jsonBuiltinScenes.push_back({
          {"name", scene.name},
          {"left", scene.left.toHex()},
          {"right", scene.right.toHex()}
        });
      }

      Json jsonLightMode = {
        {"syncEnabled", m_coreService->syncEnabled()},
        {"staticColors", m_coreService->staticColors()},
        {"scenes", m_coreService->scenes()},
        {"builtinScenes", jsonBuiltinScenes},
        {"previewRemaining", m_coreService->previewRemaining()},
        {"language", m_coreService->language()}
      };

      res.contentType = "application/json";
      res.body = jsonLightMode.dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/processingInfo", [this](const Request& /*req*/, Response& res){
      auto processingInfo = m_coreService->processingInfo();

      Json jsonProcessingInfo = {
        {"backend", processingInfo.gpu ? "GPU" : "CPU"},
        {"details", processingInfo.details}
      };

      res.contentType = "application/json";
      res.body = jsonProcessingInfo.dump();
    });


    // PUT METHODS

    m_httpServer.addRoute(HttpMethod::Put, "/api/setEntertainmentConfiguration", [this](const Request& req, Response& res){
      const std::string entertainmentConfigurationId = Json::parse(req.body).get<std::string>();

      if(!m_coreService->setEntertainmentConfiguration(entertainmentConfigurationId)){
        throw HttpError(404, "unknown entertainment area, or its lamps couldn't be loaded");
      }

      Json jsonResponse = {
        {"succeeded", true},
        {"entertainmentConfigurationId", entertainmentConfigurationId},
        {"channels", webChannels(m_coreService->channels())}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setChannelUV/:channelId", [this](const Request& req, Response& res){
      const uint8_t channelId = channelIdOf(req);

      const std::string& data = req.body;
      Json jsonUV = Json::parse(data);

      float x = jsonUV.at("x");
      float y = jsonUV.at("y");
      const auto uvCorner = parseUVCorner(jsonUV.at("type").get<int>());
      if(!uvCorner.has_value()){
        throw HttpError(400, "unknown corner");
      }

      auto optClampedUVs = m_coreService->setChannelUV(channelId, {x, y}, *uvCorner);

      if(!optClampedUVs.has_value()){
        throw HttpError(404, "unknown channel");
      }

      const auto& clampedUVs = optClampedUVs.value();

      // TODO : Serialize from JsonSerializer
      Json jsonResponse = {
        {"uvA", {{"x", clampedUVs.min.x}, {"y", clampedUVs.min.y}}},
        {"uvB", {{"x", clampedUVs.max.x}, {"y", clampedUVs.max.y}}}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setChannelMonitor/:channelId", [this](const Request& req, Response& res){
      const uint8_t channelId = channelIdOf(req);
      const Json jsonData = Json::parse(req.body);
      const size_t monitor = jsonData.at("monitor").get<size_t>();
      const Imaging::UVs uvs = jsonData.at("uvs").get<Imaging::UVs>();

      if(!m_coreService->setChannelMonitor(channelId, monitor, uvs).has_value()){
        throw HttpError(404, "unknown channel or monitor");
      }

      res.contentType = "application/json";
      res.body = webChannel(m_coreService->channels().at(channelId)).dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setChannelGammaFactor/:channelId", [this](const Request& req, Response& res){
      const uint8_t channelId = channelIdOf(req);
      const std::string& data = req.body;
      Json jsonGammaFactorData = Json::parse(data);
      float gammaFactor = jsonGammaFactorData.at("gammaFactor");

      if(!m_coreService->setChannelGammaFactor(channelId, gammaFactor)){
        throw HttpError(404, "unknown channel");
      }

      Json jsonResponse = Json{
        {"succeeded", true},
        {"gammaFactor", gammaFactor}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setSubsampleWidth", [this](const Request& req, Response& res){
      const std::string& data = req.body;
      unsigned subsampleWidth = Json::parse(data).get<unsigned>();
      m_coreService->setSubsampleWidth(subsampleWidth);

      glm::ivec2 displayResolution = m_coreService->displayResolution();
      Json jsonDisplay{
        {"x", displayResolution.x},
        {"y", displayResolution.y},
        {"subsampleWidth", m_coreService->subsampleWidth()}
      };

      std::string response = jsonDisplay.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setRefreshRate", [this](const Request& req, Response& res){
      const std::string& data = req.body;
      unsigned refreshRate = Json::parse(data).get<unsigned>();
      m_coreService->setRefreshRate(refreshRate);

      Json jsonRefreshRate{
        {"refreshRate", m_coreService->refreshRate()}
      };

      std::string response = jsonRefreshRate.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setTransitionSmoothing", [this](const Request& req, Response& res){
      const std::string& data = req.body;
      float transitionSmoothing = Json::parse(data).get<float>();
      m_coreService->setTransitionSmoothing(transitionSmoothing);

      Json jsonTransitionSmoothing{
        {"transitionSmoothing", m_coreService->transitionSmoothing()}
      };

      std::string response = jsonTransitionSmoothing.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setBrightness", [this](const Request& req, Response& res){
      m_coreService->setBrightness(Json::parse(req.body).get<float>());

      res.contentType = "application/json";
      res.body = Json{{"brightness", m_coreService->brightness()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setHighlightBoost", [this](const Request& req, Response& res){
      m_coreService->setHighlightBoost(Json::parse(req.body).get<float>());

      res.contentType = "application/json";
      res.body = Json{{"highlightBoost", m_coreService->highlightBoost()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setAudioReactive", [this](const Request& req, Response& res){
      m_coreService->setAudioReactive(Json::parse(req.body).get<bool>());

      res.contentType = "application/json";
      res.body = Json{{"audioReactive", m_coreService->audioReactive()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setSyncEnabled", [this](const Request& req, Response& res){
      m_coreService->setSyncEnabled(Json::parse(req.body).get<bool>());

      res.contentType = "application/json";
      res.body = Json{{"syncEnabled", m_coreService->syncEnabled()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setStaticColors", [this](const Request& req, Response& res){
      m_coreService->setStaticColors(Json::parse(req.body).get<Core::Config::StaticColors>());

      res.contentType = "application/json";
      res.body = Json{{"staticColors", m_coreService->staticColors()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/saveScene", [this](const Request& req, Response& res){
      const Json jsonScene = Json::parse(req.body);
      const bool saved = m_coreService->saveScene(jsonScene.at("name"), jsonScene.at("colors").get<Core::Config::StaticColors>());

      if(!saved){
        res.status = 400;
      }

      res.contentType = "application/json";
      res.body = Json{{"succeeded", saved}, {"scenes", m_coreService->scenes()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/acceptNotice", [this](const Request& /*req*/, Response& res){
      m_coreService->acceptNotice();

      res.contentType = "application/json";
      res.body = Json{{"accepted", m_coreService->noticeAccepted()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setLanguage", [this](const Request& req, Response& res){
      if(!m_coreService->setLanguage(Json::parse(req.body).get<std::string>())){
        res.status = 400;
      }

      res.contentType = "application/json";
      res.body = Json{{"language", m_coreService->language()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/applyScene", [this](const Request& req, Response& res){
      const Json jsonScene = Json::parse(req.body);
      const bool applied = m_coreService->applyScene(jsonScene.at("name"), jsonScene.value("builtin", false));

      if(!applied){
        res.status = 404;
      }

      res.contentType = "application/json";
      res.body = Json{
        {"succeeded", applied},
        {"syncEnabled", m_coreService->syncEnabled()},
        {"staticColors", m_coreService->staticColors()}
      }.dump();
    });


    // DELETE METHODS
    m_httpServer.addRoute(HttpMethod::Delete, "/api/preview", [this](const Request& /*req*/, Response& res){
      m_coreService->stopPreview();

      res.contentType = "application/json";
      res.body = Json{{"remaining", m_coreService->previewRemaining()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Delete, "/api/scene", [this](const Request& req, Response& res){
      const bool deleted = m_coreService->deleteScene(Json::parse(req.body).at("name"));

      if(!deleted){
        res.status = 404;
      }

      res.contentType = "application/json";
      res.body = Json{{"succeeded", deleted}, {"scenes", m_coreService->scenes()}}.dump();
    });


    // POST METHODS
    m_httpServer.addRoute(HttpMethod::Post, "/api/setChannelActivity/:channelId", [this](const Request& req, Response& res){
      const uint8_t channelId = channelIdOf(req);

      const std::string& data = req.body;
      Json jsonChannelData = Json::parse(data);
      bool active = jsonChannelData.at("active");

      if(!m_coreService->setChannelActivity(channelId, active)){
        throw HttpError(404, "unknown channel");
      }

      Json jsonResponse = Json{
        {"succeeded", true},
        {"channels", webChannels(m_coreService->channels())},
      };

      if(active){
        jsonResponse["newActiveChannelId"] = channelId;
      }

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Post, "/api/preview", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Json{{"duration", m_coreService->startPreview()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Post, "/api/reselectMonitors", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Json{{"started", m_coreService->reselectMonitors()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Post, "/api/saveProfile", [this](const Request& /*req*/, Response& res){
      if(!m_coreService->saveProfile()){
        throw HttpError(500, "the profile couldn't be saved");
      }

      Json jsonResponse = {
        {"succeeded", true}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Post, "/api/stop", [this](const Request& /*req*/, Response& res){

      Json jsonResponse = {{
        "succeeded", true
      }};

      std::string response = jsonResponse.dump();

      res.contentType = "application/json";
      res.body = response;

      m_coreService->stop();
    });
  }
}
