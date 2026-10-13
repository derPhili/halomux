#include <Huenicorn/Hue/Api/EntertainmentConfigurationSelector.hpp>

#include <Huenicorn/Hue/Api/ApiTools.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Hue::Api
{
  EntertainmentConfigurationSelector::EntertainmentConfigurationSelector(
    const Hue::Auth::Credentials& credentials,
    const std::string& bridgeAddress
  ):
  m_credentials(credentials),
  m_bridgeAddress(bridgeAddress)
  {
    m_entertainmentConfigurations = ApiTools::loadEntertainmentConfigurations(m_credentials.username(), m_bridgeAddress);
  }


  std::optional<std::string> EntertainmentConfigurationSelector::currentEntertainmentConfigurationId() const
  {
    if(!validSelection()){
      return std::nullopt;
    }

    return {m_currentEntertainmentConfiguration->first};
  }


  const EntertainmentConfiguration& EntertainmentConfigurationSelector::currentEntertainmentConfiguration() const
  {
    return m_currentEntertainmentConfiguration->second;
  }


  const EntertainmentConfigurations& EntertainmentConfigurationSelector::entertainmentConfigurations() const
  {
    return m_entertainmentConfigurations;
  }


  bool EntertainmentConfigurationSelector::validSelection() const
  {
    return m_currentEntertainmentConfiguration != m_entertainmentConfigurations.end();
  }


  bool EntertainmentConfigurationSelector::selectEntertainmentConfiguration(
    const std::string& entertainmentConfigurationId
  )
  {
    if(m_entertainmentConfigurations.size() == 0){
      Core::Logger::error("No entertainment configuration could be found yet. Please register one through the official Philips Hue application in order to power it with Huenicorn");
      return false;
    }

    auto selection = m_entertainmentConfigurations.begin();

    if(entertainmentConfigurationId.empty()){
      Core::Logger::log("Fallback selection ", selection->first);
    }
    else{
      selection = m_entertainmentConfigurations.find(entertainmentConfigurationId);

      if(selection == m_entertainmentConfigurations.end()){
        // Keep the current selection untouched
        Core::Logger::error("Invalid selection : ", entertainmentConfigurationId);
        return false;
      }
    }

    disableStreaming();
    m_currentEntertainmentConfiguration = selection;

    // Disable previous configuration
    if(ApiTools::streamingActive(*m_currentEntertainmentConfiguration, m_credentials.username(), m_bridgeAddress)){
      disableStreaming();
    }

    ApiTools::setStreamingState(*m_currentEntertainmentConfiguration, m_credentials.username(), m_bridgeAddress, true);

    return true;
  }


  void EntertainmentConfigurationSelector::disableStreaming() const
  {
    if(m_currentEntertainmentConfiguration == m_entertainmentConfigurations.end()){
      return;
    }

    ApiTools::setStreamingState(*m_currentEntertainmentConfiguration, m_credentials.username(), m_bridgeAddress, false);
  }
}
