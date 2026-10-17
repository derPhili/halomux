#pragma once

#include <string>
#include <vector>


namespace Halomux::Hue::Api
{
  /**
   * @brief Device data structure
   * 
   */
  struct Device
  {
    std::string id;
    std::string name;
  };

  using Devices = std::vector<Device>;
}
