#pragma once

#include <string>


namespace Halomux::Hue::Api
{
  constexpr auto HttpProtocol = "https://";

  std::string sanitizeBridgeAddress(
    const std::string& rawAddress
  );
}
