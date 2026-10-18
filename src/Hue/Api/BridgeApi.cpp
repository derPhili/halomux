#include <Halomux/Hue/Api/BridgeApi.hpp>

#include <Halomux/Hue/Api/ApiTools.hpp>
#include <Halomux/Hue/Api/ClipParsing.hpp>


namespace Halomux::Hue::Api
{
  ClipBridgeApi::ClipBridgeApi(
    const Auth::Credentials& credentials,
    const std::string& bridgeAddress
  ):
  m_credentials(credentials),
  m_bridgeAddress(bridgeAddress)
  {}


  std::optional<EntertainmentConfigurations> ClipBridgeApi::areas()
  {
    return ApiTools::loadEntertainmentConfigurations(m_credentials.username(), m_bridgeAddress);
  }


  std::optional<ChannelMembers> ClipBridgeApi::channelMembers(
    const std::string& areaId
  )
  {
    const auto resources = ApiTools::loadResources(m_credentials.username(), m_bridgeAddress);
    if(!resources){
      return std::nullopt;
    }

    return ClipParsing::channelMembers(*resources, areaId);
  }


  bool ClipBridgeApi::setStreaming(
    const EntertainmentConfigurationEntry& area,
    bool active
  )
  {
    return ApiTools::setStreamingState(area, m_credentials.username(), m_bridgeAddress, active);
  }


  std::optional<bool> ClipBridgeApi::streamingActive(
    const std::string& areaId
  )
  {
    return ApiTools::streamingActive({areaId, {}}, m_credentials.username(), m_bridgeAddress);
  }
}
