#pragma once

#include <optional>
#include <string>

#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>
#include <Halomux/Serialization/Json.hpp>


/**
 * @brief Reading the bridge's CLIP v2 answers, which may be empty, errors or malformed
 */
namespace Halomux::Hue::Api::ClipParsing
{
  /**
   * @brief Reads whether an entertainment area streams
   *
   * @param response Answer to GET /clip/v2/resource/entertainment_configuration/{id}
   * @return std::optional<bool> Whether its status is active, nothing if the answer doesn't tell (such as a deleted area)
   */
  std::optional<bool> streamingActive(
    const Serialization::Json& response
  );


  /**
   * @brief Reads whether a request changing a resource succeeded
   *
   * @param response Answer of the bridge
   * @return true The answer holds no error
   */
  bool succeeded(
    const Serialization::Json& response
  );


  /**
   * @brief Result of pairing with the bridge
   *
   */
  struct Pairing
  {
    // {"username", "clientkey"} once the link button was pressed
    std::optional<Serialization::Json> credentials;
    // Why it failed, such as "link button not pressed"
    std::string error;
  };


  /**
   * @brief Reads the answer to POST /api, the v1 pairing request
   *
   * @param response Answer of the bridge
   * @return Pairing Credentials, or the reason they're missing
   */
  Pairing pairing(
    const Serialization::Json& response
  );


  /**
   * @brief Reads the entertainment areas from the bridge's resource list (GET /clip/v2/resource)
   *
   * Malformed areas are left out, lamps whose light is missing from the list keep an empty name.
   *
   * @param response Answer of the bridge
   * @return std::optional<EntertainmentConfigurations> Areas by ID, with their lamps and inactive channels, nothing if the answer is an error
   */
  std::optional<EntertainmentConfigurations> areas(
    const Serialization::Json& response
  );


  /**
   * @brief Reads the lamps of each channel of an area from the bridge's resource list (GET /clip/v2/resource)
   *
   * @param response Answer of the bridge
   * @param areaId ID of the entertainment area
   * @return std::optional<ChannelMembers> Lamps by channel, named after their device, nothing if the answer is an error or the area isn't in it
   */
  std::optional<ChannelMembers> channelMembers(
    const Serialization::Json& response,
    const std::string& areaId
  );
}
