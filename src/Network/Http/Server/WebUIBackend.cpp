#include <Huenicorn/Network/Http/Server/WebUIBackend.hpp>

#include <filesystem>

#include <Huenicorn/Core/CoreService.hpp>
#include <Huenicorn/Serialization/Config.hpp>
#include <Huenicorn/Serialization/Json.hpp>
#include <Huenicorn/Serialization/EntertainmentConfiguration.hpp>
#include <Huenicorn/Stream/Streamer.hpp>


namespace Huenicorn::Network::Http::Server
{
  using namespace Serialization;

  WebUIBackend::WebUIBackend(
    Huenicorn::Core::CoreService* coreService
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
      const auto& entertainmentConfigurations = m_coreService->entertainmentConfigurations();
      std::string currentEntertainmentConfigurationId = m_coreService->currentEntertainmentConfigurationId().value();

      Json jsonResponse = {
        {"entertainmentConfigurations", entertainmentConfigurations},
        {"currentEntertainmentConfigurationId", currentEntertainmentConfigurationId}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/channel/:channelId", [this](const Request& req, Response& res){
      uint8_t channelId = static_cast<uint8_t>(std::stoi(req.pathParams.at("channelId")));

      std::string response = Json(m_coreService->channels().at(channelId)).dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/channels", [this](const Request& /*req*/, Response& res){
      std::string response = Json(m_coreService->channels()).dump();
      res.contentType = "application/json";
      res.body = response;
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

      Json jsonDisplayInfo{
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


    m_httpServer.addRoute(HttpMethod::Get, "/api/lightMode", [this](const Request& /*req*/, Response& res){
      Json jsonLightMode = {
        {"syncEnabled", m_coreService->syncEnabled()},
        {"staticColors", m_coreService->staticColors()},
        {"scenes", m_coreService->scenes()}
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
      const std::string& data = req.body;
      std::string entertainmentConfigurationId = Json::parse(data);

      bool succeeded = m_coreService->setEntertainmentConfiguration(entertainmentConfigurationId);

      Json jsonResponse = {
        {"succeeded", succeeded},
        {"entertainmentConfigurationId", entertainmentConfigurationId},
        {"channels", Json(m_coreService->channels())}
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/setChannelUV/:channelId", [this](const Request& req, Response& res){
      uint8_t channelId = static_cast<uint8_t>(std::stoi(req.pathParams.at("channelId")));

      const std::string& data = req.body;
      Json jsonUV = Json::parse(data);

      float x = jsonUV.at("x");
      float y = jsonUV.at("y");
      Imaging::UVCorner uvCorner = static_cast<Imaging::UVCorner>(jsonUV.at("type").get<int>());

      auto optClampedUVs = m_coreService->setChannelUV(channelId, {x, y}, uvCorner);

      if(!optClampedUVs.has_value()){
        res.status = 404;
        res.contentType = "application/json";
        res.body = Json{{"succeeded", false}, {"error", "invalid channel id"}}.dump();
        return;
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


    m_httpServer.addRoute(HttpMethod::Put, "/api/setChannelGammaFactor/:channelId", [this](const Request& req, Response& res){
      uint8_t channelId = static_cast<uint8_t>(std::stoi(req.pathParams.at("channelId")));
      const std::string& data = req.body;
      Json jsonGammaFactorData = Json::parse(data);
      float gammaFactor = jsonGammaFactorData.at("gammaFactor");

      if(!m_coreService->setChannelGammaFactor(channelId, gammaFactor)){
        std::string response = Json{
          {"succeeded", false},
          {"error", "invalid channel id"}
        }.dump();

        res.contentType = "application/json";
        res.body = response;
        return;
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


    // DELETE METHODS
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
      uint8_t channelId = static_cast<uint8_t>(std::stoi(req.pathParams.at("channelId")));

      const std::string& data = req.body;
      Json jsonChannelData = Json::parse(data);
      bool active = jsonChannelData.at("active");

      if(!m_coreService->setChannelActivity(channelId, active)){
        std::string response = Json{
          {"succeeded", false},
          {"error", "invalid channel id"}
        }.dump();

        res.contentType = "application/json";
        res.body = response;
        return;
      }

      Json jsonResponse = Json{
        {"succeeded", true},
        {"channels", Json(m_coreService->channels())},
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


    m_httpServer.addRoute(HttpMethod::Post, "/api/saveProfile", [this](const Request& /*req*/, Response& res){
      m_coreService->saveProfile();

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
