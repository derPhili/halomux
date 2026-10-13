#pragma once

#include <unordered_map>

#include <Huenicorn/Hue/Api/Channel.hpp>
#include <Huenicorn/Hue/Api/Device.hpp>


namespace Huenicorn::Hue::Api
{
  /**
   * @brief Wrapper around Hue Entertainment Configuration
   * 
   */
  struct EntertainmentConfiguration
  {
    std::string name;
    Devices devices;
    Channels channels;
  };


  // Type definitions
  using EntertainmentConfigurations = std::unordered_map<std::string, EntertainmentConfiguration>;
  using EntertainmentConfigurationsIterator = EntertainmentConfigurations::iterator;
  using EntertainmentConfigurationEntry = std::pair<std::string, EntertainmentConfiguration>;
}
