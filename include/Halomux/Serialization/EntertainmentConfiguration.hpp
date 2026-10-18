#pragma once

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>

#include <Halomux/Serialization/Channel.hpp>
#include <Halomux/Serialization/Device.hpp>

namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Hue::Api::EntertainmentConfiguration>
  {
    // Serialization
    static void to_json(
      Json& jsonEntConf,
      const Hue::Api::EntertainmentConfiguration& entConf
    )
    {
      jsonEntConf = {
        {"name", entConf.name},
        //{"devices", entConf.devices},
        //{"channels", entConf.channels},
      };
    }
  };


  template<>
  struct JsonSerializer<Hue::Api::EntertainmentConfigurations>
  {
    static void to_json(
      Json& jsonEntConfs,
      const Hue::Api::EntertainmentConfigurations& entConfs
    )
    {
      jsonEntConfs = Json::array();
      for(const auto& entertainmentConfiguration : entConfs){
        auto& it = jsonEntConfs.emplace_back(entertainmentConfiguration.second);
        it["entertainmentConfigurationId"] = entertainmentConfiguration.first;
      }
    }
  };
}
