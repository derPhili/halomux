#pragma once

#include <optional>
#include <string>

#include <Halomux/Hue/Auth/Credentials.hpp>
#include <Halomux/Hue/Api/EntertainmentConfiguration.hpp>


namespace Halomux::Hue::Api
{
  /**
   * @brief Selector tool for Entertainment Configurations
   * 
   */
  class EntertainmentConfigurationSelector
  {
  public:
    // Constructor
    /**
     * @brief EntertainmentConfigurationSelector constructor
     * 
     * @param credentials Hue bridge credentials
     * @param bridgeAddress Hue bridge address
     */
    EntertainmentConfigurationSelector(
      const Hue::Auth::Credentials& credentials,
      const std::string& bridgeAddress
    );


    // Getters
    /**
     * @brief Returns the currently selected entertainment configuration ID
     * 
     * @return std::optional<std::string> current entertainment configuration ID
     */
    std::optional<std::string> currentEntertainmentConfigurationId() const;


    /**
     * @brief Returns the currently selected entertainment configuration
     * 
     * @return const EntertainmentConfiguration& current entertainment configuration
     */
    const EntertainmentConfiguration& currentEntertainmentConfiguration() const;


    /**
     * @brief Returns a list of available entertainment configurations
     * 
     * @return const EntertainmentConfigurations& entertainment configurations
     */
    const EntertainmentConfigurations& entertainmentConfigurations() const;


    /**
     * @brief Returns whether the current entertainment configuration is valid or not
     * 
     * @return true Current entertainment configuration is valid
     * @return false Current entertainment configuration is not valid
     */
    bool validSelection() const;


    // Methods
    /**
     * @brief Selects an entertainment configuration
     * 
     * @param entertainmentConfigurationId ID of the entertainment configuration to select
     * @return true Entertainment configuration was selected successfully
     * @return false Entertainment configuration could not be selected
     */
    bool selectEntertainmentConfiguration(
      const std::string& entertainmentConfigurationId
    );


    /**
     * @brief Disables streaming on entertainment configuration if already active
     * 
     */
    void disableStreaming() const;


  private:
    // Attributes
    const Hue::Auth::Credentials m_credentials;
    const std::string m_bridgeAddress;

    EntertainmentConfigurations m_entertainmentConfigurations;
    EntertainmentConfigurationsIterator m_currentEntertainmentConfiguration{m_entertainmentConfigurations.end()};
  };
}
