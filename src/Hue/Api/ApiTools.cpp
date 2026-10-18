#include <Halomux/Hue/Api/ApiTools.hpp>

#include <Halomux/Version.hpp>
#include <Halomux/Hue/Api/BridgeAddress.hpp>
#include <Halomux/Hue/Api/BridgeDiscovery.hpp>
#include <Halomux/Hue/Api/BridgeTls.hpp>
#include <Halomux/Hue/Api/ClipParsing.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>
#include <Halomux/Network/Http/Client/Client.hpp>
#include <Halomux/Platform/Selector.hpp>
#include <Halomux/Serialization/Credentials.hpp>


namespace Halomux::Hue::Api
{
  namespace ApiTools
  {
    std::optional<Serialization::Json> loadResources(
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      const auto response = Network::Http::Client::sendRequest(HttpProtocol + bridgeAddress + "/clip/v2/resource", "GET", "", headers, bridgeTls(bridgeAddress));
      if(!response.has_value()){
        return std::nullopt;
      }

      try{
        return response->asJson();
      }
      catch(const std::exception&){
        return std::nullopt;
      }
    }


    std::optional<EntertainmentConfigurations> loadEntertainmentConfigurations(
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      const auto resources = loadResources(username, bridgeAddress);
      if(!resources){
        return std::nullopt;
      }

      return ClipParsing::areas(*resources);
    }


    bool setStreamingState(
      const EntertainmentConfigurationEntry& entertainmentConfigurationEntry,
      const std::string& username,
      const std::string& bridgeAddress,
      bool active
    )
    {
      Serialization::Json jsonBody = {
        {"action", active ? "start" : "stop"},
        {"metadata", {{"name", entertainmentConfigurationEntry.second.name}}}
      };

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};

      std::string url = HttpProtocol + bridgeAddress + "/clip/v2/resource/entertainment_configuration/" + entertainmentConfigurationEntry.first;

      const auto response = Network::Http::Client::sendRequest(url, "PUT", jsonBody.dump(), headers, bridgeTls(bridgeAddress));
      if(!response.has_value()){
        return false;
      }

      try{
        return ClipParsing::succeeded(response->asJson());
      }
      catch(const std::exception&){
        return false;
      }
    }


    std::optional<bool> streamingActive(
      const EntertainmentConfigurationEntry& entertainmentConfigurationEntry,
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      using namespace Network::Http::Client;

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      std::string url = HttpProtocol + bridgeAddress + "/clip/v2/resource/entertainment_configuration/" + entertainmentConfigurationEntry.first;
      auto entConfResponse = Network::Http::Client::sendRequest(url, "GET", "", headers, bridgeTls(bridgeAddress));
      if(!entConfResponse.has_value()){
        return std::nullopt;
      }

      try{
        return ClipParsing::streamingActive(entConfResponse->asJson());
      }
      catch(const std::exception&){
        return std::nullopt;
      }
    }
  }


  Serialization::Json ApiTools::autodetectedBridge()
  {
    // In the local network first, as Philips recommends: no internet access needed
    const auto localBridges = discoverBridges(std::chrono::milliseconds(1500));
    if(!localBridges.empty()){
      Serialization::Json jsonBridges = Serialization::Json::array();
      for(const auto& bridge : localBridges){
        jsonBridges.push_back({{"id", bridge.id}, {"internalipaddress", bridge.address}});
      }

      return {{"succeeded", true}, {"bridges", jsonBridges}};
    }

    auto detectedBridgeResponse = Network::Http::Client::sendRequest("https://discovery.meethue.com/", "GET");

    if(!detectedBridgeResponse.has_value()){
      return {{"succeeded", false}, {"error", "Could not reach discovery service. Please check your internet connection."}};
    }

    auto bridges = detectedBridgeResponse.value().asJson();

    return {{"succeeded", true}, {"bridges", bridges}};
  }


  std::optional<Network::Http::Client::Response> ApiTools::registerNewUser(const std::string& bridgeAddress)
  {
    std::string sessionUsername = Platform::adapter.getUsername();
    std::string deviceType = std::string(Halomux::AppId) + "#" + sessionUsername;

    Serialization::Json request = {{"devicetype", deviceType}, {"generateclientkey", true}};
    return Network::Http::Client::sendRequest(HttpProtocol + bridgeAddress + "/api", "POST", request.dump(), {}, bridgeTls(bridgeAddress));
  }
}
