#include <Halomux/Core/Profile.hpp>

#include <algorithm>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Serialization/Channel.hpp>
#include <Halomux/Serialization/UV.hpp>


namespace Halomux::Core::Profile
{
  using Serialization::Json;

  namespace
  {
    constexpr const char* CurrentKey = "entertainmentConfigurationId";
    constexpr const char* AreasKey = "areas";
    constexpr const char* ChannelsKey = "channels";


    // Every area's channels, legacy profiles included
    Json areas(
      const Json& profile
    )
    {
      Json result = Json::object();
      if(!profile.is_object()){
        return result;
      }

      if(profile.contains(AreasKey) && profile.at(AreasKey).is_object()){
        for(const auto& [areaId, area] : profile.at(AreasKey).items()){
          if(area.is_object() && area.contains(ChannelsKey) && area.at(ChannelsKey).is_array()){
            result[areaId] = {{ChannelsKey, area.at(ChannelsKey)}};
          }
        }
      }

      // Legacy: the channels of the area selected when it was saved
      const std::string legacyArea = currentArea(profile);
      if(!legacyArea.empty() && !result.contains(legacyArea) && profile.contains(ChannelsKey) && profile.at(ChannelsKey).is_array()){
        result[legacyArea] = {{ChannelsKey, profile.at(ChannelsKey)}};
      }

      return result;
    }
  }


  Json channelsForArea(
    const Json& profile,
    const std::string& areaId
  )
  {
    const Json allAreas = areas(profile);
    if(!allAreas.contains(areaId)){
      return Json::array();
    }

    return allAreas.at(areaId).at(ChannelsKey);
  }


  std::string currentArea(
    const Json& profile
  )
  {
    if(!profile.is_object() || !profile.contains(CurrentKey) || !profile.at(CurrentKey).is_string()){
      return {};
    }

    return profile.at(CurrentKey).get<std::string>();
  }


  Json withArea(
    const Json& profile,
    const std::string& areaId,
    const Json& channels
  )
  {
    Json allAreas = areas(profile);
    allAreas[areaId] = {{ChannelsKey, channels}};

    return {
      {CurrentKey, areaId},
      {AreasKey, allAreas}
    };
  }


  Hue::Api::Channels channelsFromProfile(
    const Hue::Api::ChannelMembers& members,
    const Json& profileChannels
  )
  {
    Hue::Api::Channels channels;

    for(const auto& [channelId, devices] : members){
      Hue::Api::Channel channel{false, devices, 0.f};

      if(profileChannels.is_array()){
        for(const auto& saved : profileChannels){
          if(!saved.is_object() || !saved.contains("channelId") || !saved.at("channelId").is_number_integer() || saved.at("channelId").get<int>() != channelId){
            continue;
          }

          try{
            // The gamma slider's range, which a hand-edited profile may leave
            const float gammaFactor = static_cast<float>(std::clamp(saved.at("gammaFactor").get<double>(), -1.0, 1.0));
            Hue::Api::Channel restored{saved.at("active").get<bool>(), devices, gammaFactor, saved.at("uvs").get<Imaging::UVs>()};
            restored.monitor = Serialization::monitorRefFromJson(saved);
            channel = std::move(restored);
          }
          catch(const std::exception& e){
            Logger::warn("Ignoring the malformed settings of channel ", static_cast<int>(channelId), " in the profile: ", e.what());
          }
          break;
        }
      }

      channels.emplace(channelId, std::move(channel));
    }

    return channels;
  }
}
