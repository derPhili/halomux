#include <Halomux/Network/Http/Server/SetupBackend.hpp>

#include <Halomux/Core/CoreService.hpp>
#include <Halomux/Network/Http/Server/RouteTools.hpp>
#include <Halomux/Hue/Api/ApiTools.hpp>
#include <Halomux/Serialization/Credentials.hpp>


namespace Halomux::Network::Http::Server
{
  SetupBackend::SetupBackend(
    Halomux::Core::CoreService* coreService
  ):
  m_coreService(coreService)
  {
    // WEB PAGES

    m_httpServer.addRoute(HttpMethod::Get, "/", [](const Request& /*req*/, Response& res){
      std::filesystem::path pageName = "setup.html";
      Utils::getWebFile(res, pageName);
    });


    m_httpServer.addRoute(HttpMethod::Get, "/:pageName", [](const Request& req, Response& res){
      std::filesystem::path pageName = req.pathParams.at("pageName");

      if(pageName == "index.html"){
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


    m_httpServer.addRoute(HttpMethod::Get, "/api/autodetectBridge", [](const Request& /*req*/, Response& res){
      Serialization::Json jsonResponse = Hue::Api::ApiTools::autodetectedBridge();
      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/language", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Serialization::Json{{"language", m_coreService->language()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/entertainmentAreas", [this](const Request& /*req*/, Response& res){
      res.contentType = "application/json";
      res.body = Serialization::Json{{"areas", m_coreService->bridgeEntertainmentAreas()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Get, "/api/configFilePath", [this](const Request& /*req*/, Response& res){
      Serialization::Json jsonResponse = {{"configFilePath", m_coreService->configFilePath()}};
      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    // POST METHODS

    m_httpServer.addRoute(HttpMethod::Post, "/api/finishSetup", [this](const Request& /*req*/, Response& res){
      // Starting without a paired bridge would fail
      if(!m_coreService->initialSetupComplete()){
        throw HttpError(409, "the setup isn't complete yet: pair with the bridge first");
      }

      Serialization::Json jsonResponse = {
        {"version", m_coreService->version()},
      };

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
      m_setupCompleted = true;
      m_httpServer.stop();
    });


    m_httpServer.addRoute(HttpMethod::Post, "/api/abort", [this](const Request& /*req*/, Response& res){
      std::string response = "{}";
      res.contentType = "application/json";
      res.body = response;

      m_setupCompleted = false;
      m_httpServer.stop();
    });


    // PUT METHODS

    m_httpServer.addRoute(HttpMethod::Put, "/api/setLanguage", [this](const Request& req, Response& res){
      if(!m_coreService->setLanguage(readBody(req, [](const Serialization::Json& body){return body.get<std::string>();}))){
        res.status = 400;
      }

      res.contentType = "application/json";
      res.body = Serialization::Json{{"language", m_coreService->language()}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/acceptNotice", [this](const Request& /*req*/, Response& res){
      m_coreService->acceptNotice();

      res.contentType = "application/json";
      res.body = Serialization::Json{{"succeeded", true}}.dump();
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/validateBridgeAddress", [this](const Request& req, Response& res){
      const std::string bridgeAddress = readBody(req, [](const Serialization::Json& body){return body.at("bridgeAddress").get<std::string>();});

      Serialization::Json jsonResponse = {{"succeeded", m_coreService->validateBridgeAddress(bridgeAddress)}};

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/validateCredentials", [this](const Request& req, Response& res){
      const auto credentials = readBody(req, [](const Serialization::Json& body){return body.get<Hue::Auth::Credentials>();});

      Serialization::Json jsonResponse = {{"succeeded", m_coreService->validateCredentials(credentials)}};

      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });


    m_httpServer.addRoute(HttpMethod::Put, "/api/registerNewUser", [this](const Request& /*req*/, Response& res){
      Serialization::Json jsonResponse = m_coreService->registerNewUser();
      std::string response = jsonResponse.dump();
      res.contentType = "application/json";
      res.body = response;
    });
  }
}
