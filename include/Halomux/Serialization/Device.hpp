#pragma once

#include <vector>

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Hue/Api/Device.hpp>


namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Halomux::Hue::Api::Device>
  {
    // Deserialization
    static void from_json(
      const Json& jsonDevice,
      Hue::Api::Device& device
    )
    {
      jsonDevice.at("name").get_to(device.name);
    }

    // Serialization
    static void to_json(
      Json& jsonDevice,
      const Hue::Api::Device& device
    )
    {
      jsonDevice = {
        {"id", device.id},
        {"name", device.name},
      };
    }
  };
}
