#pragma once

#include <Halomux/Network/Http/Client/Client.hpp>
#include <string>

#include <Halomux/Hue/Api/Device.hpp>
#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>
#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Hue::Api
{
  /**
   * @brief Provides wrappers around the Hue bridge API
   * 
   */
  namespace ApiTools
  {
    /**
     * @brief Loads all the resources of the Hue bridge (GET /clip/v2/resource)
     *
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return std::optional<Serialization::Json> Answer of the bridge, nothing if it's unreachable or the answer isn't JSON
     */
    std::optional<Serialization::Json> loadResources(
      const std::string& username,
      const std::string& bridgeAddress
    );

    /**
     * @brief Loads entertainment configurations from Hue bridge
     *
     * @param username Username credential for the HTTPS request
     * @param bridgeAddress Address of the Hue bridge
     * @return std::optional<EntertainmentConfigurations> Entertainment configurations, nothing if the bridge is unreachable or answers an error
     */
    std::optional<EntertainmentConfigurations> loadEntertainmentConfigurations(
      const std::string& username,
      const std::string& bridgeAddress
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
