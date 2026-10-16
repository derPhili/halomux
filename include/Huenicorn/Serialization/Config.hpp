#pragma once

#include <Huenicorn/Serialization/JsonSerializer.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>
#include <Huenicorn/Core/Config.hpp>


namespace Huenicorn::Serialization
{
  template<>
  struct JsonSerializer<Huenicorn::Core::Config::StaticColors>
  {
    // Object of #rrggbb colors keyed by channel ID
    static void to_json(
      Json& jsonStaticColors,
      const Huenicorn::Core::Config::StaticColors& staticColors
    )
    {
      jsonStaticColors = Json::object();
      for(const auto& [channelId, color] : staticColors){
        jsonStaticColors[std::to_string(channelId)] = color.toHex();
      }
    }


    static void from_json(
      const Json& jsonStaticColors,
      Huenicorn::Core::Config::StaticColors& staticColors
    )
    {
      staticColors.clear();
      for(const auto& [key, jsonColor] : jsonStaticColors.items()){
        const int channelId = std::stoi(key);
        const auto color = Imaging::Color::fromHex(jsonColor.get<std::string>());
        if(channelId < 0 || channelId > 255 || !color.has_value()){
          throw std::invalid_argument("Invalid static color " + key + ": " + jsonColor.dump());
        }

        staticColors.emplace(static_cast<uint8_t>(channelId), color.value());
      }
    }
  };


  template<>
  struct JsonSerializer<Huenicorn::Core::Config::Data>
  {
    static void to_json(
      Json& jsonConfigData,
      const Huenicorn::Core::Config::Data& configData
    )
    {
      if(configData.subsampleWidth.has_value()){
        jsonConfigData["subsampleWidth"] = *configData.subsampleWidth;
      }

      if(configData.refreshRate.has_value()){
        jsonConfigData["refreshRate"] = *configData.refreshRate;
      }

      if(configData.restServerPort.has_value()){
        jsonConfigData["restServerPort"] = *configData.restServerPort;
      }

      if(configData.transitionSmoothing.has_value()){
        jsonConfigData["transitionSmoothing"] = *configData.transitionSmoothing;
      }

      if(configData.highlightBoost.has_value()){
        jsonConfigData["highlightBoost"] = *configData.highlightBoost;
      }

      if(configData.audioReactive.has_value()){
        jsonConfigData["audioReactive"] = *configData.audioReactive;
      }

      if(configData.brightness.has_value()){
        jsonConfigData["brightness"] = *configData.brightness;
      }

      if(configData.syncEnabled.has_value()){
        jsonConfigData["syncEnabled"] = *configData.syncEnabled;
      }

      if(configData.staticColors.has_value()){
        jsonConfigData["staticColors"] = *configData.staticColors;
      }

      if(configData.scenes.has_value()){
        jsonConfigData["scenes"] = *configData.scenes;
      }

      if(configData.language.has_value()){
        jsonConfigData["language"] = *configData.language;
      }

      if(configData.bridgeId.has_value()){
        jsonConfigData["bridgeId"] = *configData.bridgeId;
      }

      if(configData.bridgeCertificateVerified.has_value()){
        jsonConfigData["bridgeCertificateVerified"] = *configData.bridgeCertificateVerified;
      }

      if(configData.noticeAccepted.has_value()){
        jsonConfigData["noticeAccepted"] = {
          {"version", configData.noticeAccepted->version},
          {"at", configData.noticeAccepted->at}
        };
      }

      if(configData.boundBackendIP.has_value()){
        jsonConfigData["boundBackendIP"] = *configData.boundBackendIP;
      }

      if(configData.bridgeAddress.has_value()){
        jsonConfigData["bridgeAddress"] = *configData.bridgeAddress;
      }

      if(configData.credentials.has_value()){
        jsonConfigData["credentials"] = *configData.credentials;
      }

      if(configData.profileName.has_value()){
        jsonConfigData["profileName"] = *configData.profileName;
      }

      if(configData.restoreToken.has_value()){
        jsonConfigData["restoreToken"] = *configData.restoreToken;
      }
    }


    static void from_json(
      const Json& jsonConfigData,
      Huenicorn::Core::Config::Data& configData
    )
    {
      if(jsonConfigData.contains("restServerPort")){
        configData.restServerPort = jsonConfigData.at("restServerPort");
      }

      if(jsonConfigData.contains("boundBackendIP")){
        configData.boundBackendIP = jsonConfigData.at("boundBackendIP");
      }

      if(jsonConfigData.contains("bridgeAddress")){
        configData.bridgeAddress.emplace(jsonConfigData.at("bridgeAddress"));
      }

      if(jsonConfigData.contains("credentials")){
        configData.credentials.emplace(jsonConfigData.at("credentials").get<Hue::Auth::Credentials>());
      }

      if(jsonConfigData.contains("profileName")){
        configData.profileName.emplace(jsonConfigData.at("profileName"));
      }

      if(jsonConfigData.contains("refreshRate")){
        configData.refreshRate = jsonConfigData.at("refreshRate");
      }

      if(jsonConfigData.contains("subsampleWidth")){
        configData.subsampleWidth = jsonConfigData.at("subsampleWidth");
      }

      if(jsonConfigData.contains("transitionSmoothing")){
        configData.transitionSmoothing = jsonConfigData.at("transitionSmoothing");
      }

      if(jsonConfigData.contains("highlightBoost")){
        configData.highlightBoost = jsonConfigData.at("highlightBoost");
      }

      if(jsonConfigData.contains("audioReactive")){
        configData.audioReactive = jsonConfigData.at("audioReactive");
      }

      if(jsonConfigData.contains("brightness")){
        configData.brightness = jsonConfigData.at("brightness");
      }

      if(jsonConfigData.contains("syncEnabled")){
        configData.syncEnabled = jsonConfigData.at("syncEnabled");
      }

      if(jsonConfigData.contains("staticColors")){
        configData.staticColors = jsonConfigData.at("staticColors").get<Huenicorn::Core::Config::StaticColors>();
      }

      if(jsonConfigData.contains("scenes")){
        configData.scenes = jsonConfigData.at("scenes").get<Huenicorn::Core::Config::Scenes>();
      }

      if(jsonConfigData.contains("language")){
        configData.language = jsonConfigData.at("language");
      }

      if(jsonConfigData.contains("bridgeId")){
        configData.bridgeId = jsonConfigData.at("bridgeId");
      }

      if(jsonConfigData.contains("bridgeCertificateVerified")){
        configData.bridgeCertificateVerified = jsonConfigData.at("bridgeCertificateVerified");
      }

      if(jsonConfigData.contains("noticeAccepted")){
        const Json& jsonAcceptance = jsonConfigData.at("noticeAccepted");
        configData.noticeAccepted = Huenicorn::Core::Config::NoticeAcceptance{
          .version = jsonAcceptance.value("version", 0u),
          .at = jsonAcceptance.value("at", std::string{})
        };
      }

      if(jsonConfigData.contains("restoreToken")){
        configData.restoreToken = jsonConfigData.at("restoreToken");
      }
    }
  };
}
