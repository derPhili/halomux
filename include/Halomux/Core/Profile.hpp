#pragma once

#include <string>

#include <Halomux/Hue/Api/BridgeApi.hpp>
#include <Halomux/Hue/Api/Channel.hpp>
#include <Halomux/Serialization/Json.hpp>


/**
 * @brief The profile: the lamp settings of each entertainment area
 *
 * Format: {"entertainmentConfigurationId": current area, "areas": {area id: {"channels": [...]}}}.
 * Profiles written before areas had their own settings hold a single "channels" array, which
 * belongs to their "entertainmentConfigurationId".
 */
namespace Halomux::Core::Profile
{
  /**
   * @brief Returns the saved lamps of an area
   *
   * @param profile Profile, possibly missing or hand edited
   * @param areaId Entertainment area
   * @return Serialization::Json Array of channels, empty if the area has none or the profile is malformed
   */
  Serialization::Json channelsForArea(
    const Serialization::Json& profile,
    const std::string& areaId
  );


  /**
   * @brief Returns the area selected when the profile was saved
   *
   * @param profile Profile
   * @return std::string Area id, empty if none
   */
  std::string currentArea(
    const Serialization::Json& profile
  );


  /**
   * @brief Returns the profile with the lamps of an area replaced, the other areas kept
   *
   * @param profile Current profile, possibly legacy or malformed
   * @param areaId Entertainment area, which becomes the current one
   * @param channels Its channels
   * @return Serialization::Json Profile to save
   */
  Serialization::Json withArea(
    const Serialization::Json& profile,
    const std::string& areaId,
    const Serialization::Json& channels
  );


  /**
   * @brief Builds the lamps of an area from their saved settings
   *
   * Every channel of the area becomes a lamp; a lamp without saved settings, or with malformed
   * ones (such as a hand-edited profile), is inactive with the default settings.
   *
   * @param members Lamps of each channel of the area, from the bridge
   * @param profileChannels Saved channels of the area (see channelsForArea())
   * @return Hue::Api::Channels Lamps of the area
   */
  Hue::Api::Channels channelsFromProfile(
    const Hue::Api::ChannelMembers& members,
    const Serialization::Json& profileChannels
  );
}
