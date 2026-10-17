#include <Halomux/Core/Profile.hpp>


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
}
