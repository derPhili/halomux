#pragma once

#include <Halomux/Network/Http/Client/Client.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <Halomux/Hue/Api/Device.hpp>
#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>
#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Hue::Api
{
  using MembersIds = std::unordered_set<std::string>;
  using ChannelsMembersIds = std::unordered_map<uint8_t, MembersIds>;
  using EntertainmentConfigurationsChannels = std::unordered_map<std::string, ChannelsMembersIds>;


  /**
   * @brief Provides wrappers around the Hue bridge API
   * 
   */
  namespace ApiTools
  {
    /**
     * @brief Loads entertainment configurations from Hue bridge
     * 
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return EntertainmentConfigurations List of entertainment configurations
     */
    std::optional<EntertainmentConfigurations> loadEntertainmentConfigurations(
      const std::string& username,
      const std::string& bridgeAddress
    );

    /**
     * @brief Loads devices from all entertainment configurations
     * 
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return Devices List of entertainment devices
     */
    Devices loadDevices(
      const std::string& username,
      const std::string& bridgeAddress
    );

    /**
     * @brief Loads entertainment configurations channels
     * 
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return EntertainmentConfigurationsChannels Map of entertainment configurations channels
     */
    EntertainmentConfigurationsChannels loadEntertainmentConfigurationsChannels(
      const std::string& username,
      const std::string& bridgeAddress
    );

    /**
     * @brief Resolves members data from list of IDs
     * 
     * @param membersIds Members ids to match
     * @param devices Devices data
     * @return Devices List of matched data for each device
     */
    Devices matchDevices(
      const MembersIds& membersIds,
      const Devices& devices
    );

    /**
     * @brief Set the streaming state of the entertainment configuration on the Hue bridge
     * 
     * @param entertainmentConfigurationEntry Entertainment configuration to manage
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @param active True for active, false for inactive
     */
    bool setStreamingState(
      const EntertainmentConfigurationEntry& entertainmentConfigurationEntry,
      const std::string& username,
      const std::string& bridgeAddress,
      bool active
    );

    /**
     * @brief Returns the streaming state of the entertainment configuration entry
     * 
     * @param entertainmentConfigurationEntry 
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return true Streaming is active
     * @return false Streaming is inactive
     */
    std::optional<bool> streamingActive(
      const EntertainmentConfigurationEntry& entertainmentConfigurationEntry,
      const std::string& username,
      const std::string& bridgeAddress
    );

    /**
     * @brief Returns the resolved Hue bridge IP address
     * 
     * @return Serialization::Json Object containing Hue bridge address and request status
     */
    Serialization::Json autodetectedBridge();

    /**
     * @brief Requests the addition of a new user on the Hue bridge
     * 
     * @return Serialization::Json Newly created user's credentials
     */
    std::optional<Network::Http::Client::Response> registerNewUser(const std::string& bridgeAddress);
  }
}
