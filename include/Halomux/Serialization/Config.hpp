#pragma once

#include <limits>
#include <stdexcept>

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Serialization/Credentials.hpp>
#include <Halomux/Serialization/Numbers.hpp>
#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>


namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Halomux::Core::Config::StaticColors>
  {
    // Object of #rrggbb colors keyed by channel ID
    static void to_json(
      Json& jsonStaticColors,
      const Halomux::Core::Config::StaticColors& staticColors
    )
    {
      jsonStaticColors = Json::object();
      for(const auto& [channelId, color] : staticColors){
        jsonStaticColors[std::to_string(channelId)] = color.toHex();
      }
    }


    static void from_json(
      const Json& jsonStaticColors,
      Halomux::Core::Config::StaticColors& staticColors
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
  struct JsonSerializer<Halomux::Core::Config::Data>
  {
    static void to_json(
      Json& jsonConfigData,
      const Halomux::Core::Config::Data& configData
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

      if(configData.bridgePublicKey.has_value()){
        jsonConfigData["bridgePublicKey"] = *configData.bridgePublicKey;
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
      Halomux::Core::Config::Data& configData
    )
    {
      // Whole numbers in their range, not wrapped: others keep their default, such as a negative
      // number read as a huge unsigned one
      const auto readUnsigned = [&jsonConfigData](const char* key, std::optional<unsigned>& value, unsigned min, unsigned max){
        if(!jsonConfigData.contains(key)){
          return;
        }

        try{
          value = readInteger(jsonConfigData.at(key), min, max);
        }
        catch(const std::invalid_argument&){
          Halomux::Core::Logger::warn("Ignoring the invalid ", key, " of the configuration: ", jsonConfigData.at(key).dump());
        }
      };

      readUnsigned("restServerPort", configData.restServerPort, 1u, 65535u);

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

      readUnsigned("refreshRate", configData.refreshRate, 0u, std::numeric_limits<unsigned>::max());
      readUnsigned("subsampleWidth", configData.subsampleWidth, 0u, std::numeric_limits<unsigned>::max());

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
        configData.staticColors = jsonConfigData.at("staticColors").get<Halomux::Core::Config::StaticColors>();
      }

      if(jsonConfigData.contains("scenes")){
        configData.scenes = jsonConfigData.at("scenes").get<Halomux::Core::Config::Scenes>();
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

      if(jsonConfigData.contains("bridgePublicKey")){
        configData.bridgePublicKey = jsonConfigData.at("bridgePublicKey");
      }

      if(jsonConfigData.contains("noticeAccepted")){
        const Json& jsonAcceptance = jsonConfigData.at("noticeAccepted");
        configData.noticeAccepted = Halomux::Core::Config::NoticeAcceptance{
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
