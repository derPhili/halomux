#include <Huenicorn/Hue/Api/ApiTools.hpp>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Version.hpp>
#include <Huenicorn/Hue/Api/BridgeAddress.hpp>
#include <Huenicorn/Hue/Api/BridgeTls.hpp>
#include <Huenicorn/Hue/Auth/Credentials.hpp>
#include <Huenicorn/Network/Http/Client/Client.hpp>
#include <Huenicorn/Platform/Selector.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>
#include <Huenicorn/Serialization/Device.hpp>
#include <Huenicorn/Serialization/EntertainmentConfiguration.hpp>


namespace Huenicorn::Hue::Api
{
  namespace ApiTools
  {
    EntertainmentConfigurations loadEntertainmentConfigurations(
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      // I don't always abbreviate variable names
      // but when I do, it's because I don't have a 32:9 monitor
      EntertainmentConfigurations entConfs;

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      std::string entConfUrl = HttpProtocol + bridgeAddress + "/clip/v2/resource/entertainment_configuration";
      auto entConfResponse = Network::Http::Client::sendRequest(entConfUrl, "GET", "", headers, bridgeTls(bridgeAddress));

      if(entConfResponse.has_value()){
        // Listing entertainment configurations

        auto jsonEntConfs = entConfResponse.value().asJson();

        for(const auto& jsonEntConf : jsonEntConfs.at("data")){
          EntertainmentConfiguration entConf = jsonEntConf.get<EntertainmentConfiguration>();

          for(auto& device : entConf.devices){
            std::string lightUrl = HttpProtocol + bridgeAddress + "/clip/v2/resource/light/" + device.id;

            auto lightResponse = Network::Http::Client::sendRequest(lightUrl, "GET", "", headers, bridgeTls(bridgeAddress));
            if(!lightResponse.has_value()){
              Core::Logger::warn("Could not load the metadata of light ", device.id);
              continue;
            }

            auto jsonLightData = lightResponse.value().asJson();
            auto deviceId = device.id;
            device = jsonLightData.at("data").at(0).at("metadata").get<Device>();
            device.id = deviceId;
          }

          const auto& jsonChannels = jsonEntConf.at("channels");
          for (const auto& jsonChannel : jsonChannels) {
            uint8_t channelId = jsonChannel.at("channel_id").get<uint8_t>();
            entConf.channels.insert({channelId, Channel{false, {}, 0.f}});
          }

          entConfs.insert({jsonEntConf.at("id").get<std::string>(), entConf});
        }
      }

      return entConfs;
    }


    Devices loadDevices(
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      using namespace Network::Http::Client;

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      std::string resourceUrl = HttpProtocol + bridgeAddress + "/clip/v2/resource";
      auto resourceResponse = Network::Http::Client::sendRequest(resourceUrl, "GET", "", headers, bridgeTls(bridgeAddress));

      Devices devices;

      if(resourceResponse.has_value()){
        auto jsonResource = resourceResponse.value().asJson();
        for(const auto& jsonData : jsonResource.at("data")){
          if(jsonData.at("type") != "device"){
            continue;
          }

          for(const auto& service : jsonData.at("services")){
            if(service.at("rtype") == "entertainment"){
              auto device = jsonData.at("metadata").get<Device>();
              device.id = service.at("rid");
              devices.push_back(device);
            }
          }
        }
      }

      return devices;
    }


    EntertainmentConfigurationsChannels loadEntertainmentConfigurationsChannels(
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      using namespace Network::Http::Client;

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      std::string resourceUrl = HttpProtocol + bridgeAddress + "/clip/v2/resource/entertainment_configuration";

      auto entertainmentConfigurationsResponse = Network::Http::Client::sendRequest(resourceUrl, "GET", "", headers, bridgeTls(bridgeAddress));

      EntertainmentConfigurationsChannels entConfsChannels;

      if(entertainmentConfigurationsResponse.has_value()){
        auto jsonEntertainmentConfigurations = entertainmentConfigurationsResponse.value().asJson();
        for(const auto& entConf : jsonEntertainmentConfigurations.at("data")){
          std::string configurationId = entConf.at("id");
          for(const auto& jsonChannel : entConf.at("channels")){
            uint8_t channelId = jsonChannel.at("channel_id");
            for(const auto& jsonMember : jsonChannel.at("members")){
              std::string jsonMemberId = jsonMember.at("service").at("rid");
              entConfsChannels[configurationId][channelId].insert(jsonMemberId);
            }
          }
        }
      }

      return entConfsChannels;
    }


    Devices matchDevices(
      const MembersIds& membersIds,
      const Devices& devices
    )
    {
      Devices matchedDevices;
      std::copy_if(
        devices.begin(),
        devices.end(),
        std::back_inserter(matchedDevices),
        [&](const Device& d){
          return membersIds.count(d.id) > 0;
        }
      );

      return matchedDevices;
    }


    void setStreamingState(
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

      Network::Http::Client::sendRequest(url, "PUT", jsonBody.dump(), headers, bridgeTls(bridgeAddress));
    }


    bool streamingActive(
      const EntertainmentConfigurationEntry& entertainmentConfigurationEntry,
      const std::string& username,
      const std::string& bridgeAddress
    )
    {
      using namespace Network::Http::Client;

      std::string status;

      Network::Http::Client::Headers headers = {{"hue-application-key", username}};
      std::string url = HttpProtocol + bridgeAddress + "/clip/v2/resource/entertainment_configuration/" + entertainmentConfigurationEntry.first;
      auto entConfResponse = Network::Http::Client::sendRequest(url, "GET", "", headers, bridgeTls(bridgeAddress));
      if(entConfResponse.has_value()){
        status = entConfResponse.value().asJson().at("data").front().at("status");
      }

      return status == "active";
    }
  }


  Serialization::Json ApiTools::autodetectedBridge()
  {
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
    std::string deviceType = std::string(Huenicorn::AppId) + "#" + sessionUsername;

    Serialization::Json request = {{"devicetype", deviceType}, {"generateclientkey", true}};
    return Network::Http::Client::sendRequest(HttpProtocol + bridgeAddress + "/api", "POST", request.dump(), {}, bridgeTls(bridgeAddress));
  }
}
