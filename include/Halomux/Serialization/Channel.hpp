#pragma once

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Hue/Api/Channel.hpp>
#include <Halomux/Serialization/Device.hpp>
#include <Halomux/Serialization/UV.hpp>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Hue::Api::Channel>
  {
    // Serialization
    static void to_json(
      Json& jsonChannel,
      const Hue::Api::Channel& channel
    )
    {
      jsonChannel = {
        {"active", channel.state == Hue::Api::Channel::State::Active},
        {"uvs", Json(channel.uvs)},
        {"gammaFactor", channel.gammaFactor},
        {"devices", Json(channel.devices)}
      };

      if(channel.monitor.has_value()){
        const auto& geometry = channel.monitor->geometry;
        jsonChannel["monitor"] = {
          {"x", geometry.x},
          {"y", geometry.y},
          {"width", geometry.width},
          {"height", geometry.height},
          {"name", channel.monitor->name}
        };
      }
    }
  };


  /**
   * @brief Reads the monitor a channel of the profile follows
   *
   * @param jsonChannel Channel of the profile
   * @return std::optional<Grabber::MonitorRef> Monitor, nothing if missing or malformed
   */
  inline std::optional<Grabber::MonitorRef> monitorRefFromJson(
    const Json& jsonChannel
  )
  {
    if(!jsonChannel.contains("monitor") || !jsonChannel.at("monitor").is_object()){
      return std::nullopt;
    }

    const Json& jsonMonitor = jsonChannel.at("monitor");
    for(const char* key : {"x", "y", "width", "height"}){
      if(!jsonMonitor.contains(key) || !jsonMonitor.at(key).is_number_integer()){
        return std::nullopt;
      }
    }

    Grabber::MonitorRef ref{
      .geometry = {
        jsonMonitor.at("x").get<int>(),
        jsonMonitor.at("y").get<int>(),
        jsonMonitor.at("width").get<int>(),
        jsonMonitor.at("height").get<int>()
      },
      .name = ""
    };

    if(jsonMonitor.contains("name") && jsonMonitor.at("name").is_string()){
      ref.name = jsonMonitor.at("name").get<std::string>();
    }

    return ref;
  }


  template<>
  struct JsonSerializer<Hue::Api::Channels>
  {
    static void to_json(
      Json& jsonChannels,
      const Hue::Api::Channels& channels
    )
    {
      jsonChannels = Json::array();
      for(const auto& channel : channels){
        auto& it = jsonChannels.emplace_back(Json(channel.second));
        it["channelId"] = channel.first;
      }
    }
  };
}
