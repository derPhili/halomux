#include <Halomux/Hue/Api/BridgeApi.hpp>

#include <Halomux/Hue/Api/ApiTools.hpp>


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
    try{
      return ApiTools::loadEntertainmentConfigurations(m_credentials.username(), m_bridgeAddress);
    }
    catch(const std::exception&){
      // A malformed answer: as unknown as no answer
      return std::nullopt;
    }
  }


  std::optional<ChannelMembers> ClipBridgeApi::channelMembers(
    const std::string& areaId
  )
  {
    try{
      const auto areasChannels = ApiTools::loadEntertainmentConfigurationsChannels(m_credentials.username(), m_bridgeAddress);
      const auto areaIt = areasChannels.find(areaId);
      if(areaIt == areasChannels.end()){
        return std::nullopt;
      }

      const auto devices = ApiTools::loadDevices(m_credentials.username(), m_bridgeAddress);

      ChannelMembers members;
      for(const auto& [channelId, membersIds] : areaIt->second){
        members.emplace(channelId, ApiTools::matchDevices(membersIds, devices));
      }

      return members;
    }
    catch(const std::exception&){
      return std::nullopt;
    }
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
