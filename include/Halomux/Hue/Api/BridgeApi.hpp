#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <Halomux/Hue/Api/Device.hpp>
#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>


namespace Halomux::Hue::Api
{
  /**
   * @brief What the area session asks the bridge
   *
   */
  class BridgeApi
  {
  public:
    virtual ~BridgeApi() = default;


    /**
     * @brief Lists the entertainment areas
     *
     * @return std::optional<EntertainmentConfigurations> Areas, nothing if the bridge is unreachable
     */
    virtual std::optional<EntertainmentConfigurations> areas() = 0;


    /**
     * @brief Lists the lamps of each channel of an area
     *
     * @param areaId Entertainment area
     * @return std::optional<ChannelMembers> Lamps per channel, nothing if the bridge is unreachable or the area unknown
     */
    virtual std::optional<ChannelMembers> channelMembers(
      const std::string& areaId
    ) = 0;


    /**
     * @brief Starts or stops streaming to an area
     *
     * @param area Entertainment area
     * @param active Whether to start
     * @return true The bridge accepted
     */
    virtual bool setStreaming(
      const EntertainmentConfigurationEntry& area,
      bool active
    ) = 0;


    /**
     * @brief Tells whether an area streams
     *
     * @param areaId Entertainment area
     * @return std::optional<bool> Whether its status is active, nothing if unknown (bridge unreachable, area deleted)
     */
    virtual std::optional<bool> streamingActive(
      const std::string& areaId
    ) = 0;
  };


  /**
   * @brief The bridge, through its CLIP v2 API
   *
   */
  class ClipBridgeApi : public BridgeApi
  {
  public:
    ClipBridgeApi(
      const Auth::Credentials& credentials,
      const std::string& bridgeAddress
    );

    std::optional<EntertainmentConfigurations> areas() override;

    std::optional<ChannelMembers> channelMembers(
      const std::string& areaId
    ) override;

    bool setStreaming(
      const EntertainmentConfigurationEntry& area,
      bool active
    ) override;

    std::optional<bool> streamingActive(
      const std::string& areaId
    ) override;

  private:
    const Auth::Credentials m_credentials;
    const std::string m_bridgeAddress;
  };
}
