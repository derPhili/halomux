#pragma once

#include <string>


namespace Huenicorn::Hue::Api
{
  constexpr auto HttpProtocol = "https://";

  std::string sanitizeBridgeAddress(
    const std::string& rawAddress
  );
}
