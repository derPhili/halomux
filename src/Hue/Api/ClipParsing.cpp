#include <Halomux/Hue/Api/ClipParsing.hpp>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <unordered_set>

#include <Halomux/Core/Logger.hpp>


namespace
{
  using Halomux::Serialization::Json;


  // The resources of a successful answer, nothing for an error or a malformed answer
  const Json* resourceList(
    const Json& response
  )
  {
    if(!Halomux::Hue::Api::ClipParsing::succeeded(response) || !response.contains("data") || !response.at("data").is_array()){
      return nullptr;
    }

    return &response.at("data");
  }


  // Whether an entry of the list is a resource of that type, with an ID
  bool isResource(
    const Json& resource,
    std::string_view type
  )
  {
    return resource.is_object() && resource.contains("type") && resource.at("type") == type && resource.contains("id") && resource.at("id").is_string();
  }


  // The name in a resource's metadata, empty without one
  std::string nameOf(
    const Json& resource
  )
  {
    if(!resource.contains("metadata") || !resource.at("metadata").is_object()){
      return "";
    }

    const Json& metadata = resource.at("metadata");
    return metadata.contains("name") && metadata.at("name").is_string() ? metadata.at("name").get<std::string>() : "";
  }


  // The ID of a channel of an entertainment area, throws if it isn't one
  uint8_t channelIdOf(
    const Json& channel
  )
  {
    const int channelId = channel.at("channel_id").get<int>();
    if(channelId < 0 || channelId > 255){
      throw std::out_of_range("channel_id " + std::to_string(channelId));
    }

    return static_cast<uint8_t>(channelId);
  }
}


namespace Halomux::Hue::Api::ClipParsing
{
  using Serialization::Json;


  std::optional<bool> streamingActive(
    const Json& response
  )
  {
    if(!succeeded(response) || !response.contains("data") || !response.at("data").is_array() || response.at("data").empty()){
      return std::nullopt;
    }

    const Json& area = response.at("data").front();
    if(!area.is_object() || !area.contains("status") || !area.at("status").is_string()){
      return std::nullopt;
    }

    return area.at("status").get<std::string>() == "active";
  }


  bool succeeded(
    const Json& response
  )
  {
    if(!response.is_object()){
      return false;
    }

    if(!response.contains("errors")){
      return true;
    }

    const Json& errors = response.at("errors");
    return errors.is_array() && errors.empty();
  }


  Pairing pairing(
    const Json& response
  )
  {
    if(!response.is_array() || response.empty() || !response.at(0).is_object()){
      return {.credentials = std::nullopt, .error = "unexpected answer of the bridge"};
    }

    const Json& result = response.at(0);

    if(result.contains("success")){
      const Json& success = result.at("success");
      if(success.is_object() && success.contains("username") && success.at("username").is_string() && success.contains("clientkey") && success.at("clientkey").is_string()){
        return {.credentials = Json{{"username", success.at("username")}, {"clientkey", success.at("clientkey")}}, .error = ""};
      }

      return {.credentials = std::nullopt, .error = "the bridge sent no credentials"};
    }

    if(result.contains("error") && result.at("error").is_object() && result.at("error").contains("description") && result.at("error").at("description").is_string()){
      return {.credentials = std::nullopt, .error = result.at("error").at("description").get<std::string>()};
    }

    return {.credentials = std::nullopt, .error = "unexpected answer of the bridge"};
  }


  std::optional<EntertainmentConfigurations> areas(
    const Json& response
  )
  {
    const Json* resources = resourceList(response);
    if(!resources){
      return std::nullopt;
    }

    std::unordered_map<std::string, std::string> lightNames;
    for(const Json& resource : *resources){
      if(isResource(resource, "light")){
        lightNames.emplace(resource.at("id").get<std::string>(), nameOf(resource));
      }
    }

    EntertainmentConfigurations parsed;
    for(const Json& resource : *resources){
      if(!isResource(resource, "entertainment_configuration")){
        continue;
      }

      try{
        EntertainmentConfiguration area;
        area.name = resource.at("metadata").at("name").get<std::string>();

        for(const Json& lightService : resource.at("light_services")){
          const auto lightId = lightService.at("rid").get<std::string>();
          const auto nameIt = lightNames.find(lightId);
          area.devices.push_back({.id = lightId, .name = nameIt != lightNames.end() ? nameIt->second : ""});
        }

        for(const Json& channel : resource.at("channels")){
          area.channels.insert({channelIdOf(channel), Channel{false, {}, 0.f}});
        }

        parsed.insert({resource.at("id").get<std::string>(), std::move(area)});
      }
      catch(const std::exception& e){
        Core::Logger::warn("Ignoring a malformed entertainment area of the bridge: ", e.what());
      }
    }

    return parsed;
  }


  std::optional<ChannelMembers> channelMembers(
    const Json& response,
    const std::string& areaId
  )
  {
    const Json* resources = resourceList(response);
    if(!resources){
      return std::nullopt;
    }

    const auto areaIt = std::ranges::find_if(*resources, [&](const Json& resource){
      return isResource(resource, "entertainment_configuration") && resource.at("id") == areaId;
    });

    if(areaIt == resources->end()){
      return std::nullopt;
    }

    // Lamps by entertainment service, named after their device, in the order of the list
    Devices lamps;
    for(const Json& resource : *resources){
      if(!isResource(resource, "device") || !resource.contains("services") || !resource.at("services").is_array()){
        continue;
      }

      for(const Json& service : resource.at("services")){
        if(service.is_object() && service.value("rtype", "") == "entertainment" && service.contains("rid") && service.at("rid").is_string()){
          lamps.push_back({.id = service.at("rid").get<std::string>(), .name = nameOf(resource)});
        }
      }
    }

    try{
      ChannelMembers members;
      for(const Json& channel : areaIt->at("channels")){
        std::unordered_set<std::string> memberIds;
        for(const Json& member : channel.at("members")){
          // A member without service drives no lamp
          if(member.is_object() && member.contains("service") && member.at("service").is_object() && member.at("service").contains("rid") && member.at("service").at("rid").is_string()){
            memberIds.insert(member.at("service").at("rid").get<std::string>());
          }
        }

        Devices& channelLamps = members[channelIdOf(channel)];
        std::ranges::copy_if(lamps, std::back_inserter(channelLamps), [&](const Device& lamp){
          return memberIds.contains(lamp.id);
        });
      }

      return members;
    }
    catch(const std::exception& e){
      Core::Logger::warn("Malformed entertainment area of the bridge: ", e.what());
      return std::nullopt;
    }
  }
}
