#pragma once

#include <Huenicorn/Serialization/JsonSerializer.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>
#include <Huenicorn/Core/Config.hpp>


namespace Huenicorn::Serialization
{
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

      if(configData.interpolation.has_value()){
        jsonConfigData["interpolation"] = *configData.interpolation;
      }

      if(configData.transitionSmoothing.has_value()){
        jsonConfigData["transitionSmoothing"] = *configData.transitionSmoothing;
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

      if(jsonConfigData.contains("interpolation")){
        configData.interpolation = jsonConfigData.at("interpolation");
      }

      if(jsonConfigData.contains("transitionSmoothing")){
        configData.transitionSmoothing = jsonConfigData.at("transitionSmoothing");
      }

      if(jsonConfigData.contains("restoreToken")){
        configData.restoreToken = jsonConfigData.at("restoreToken");
      }
    }
  };
}
