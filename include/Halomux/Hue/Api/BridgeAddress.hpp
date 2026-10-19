#pragma once

#include <string>


namespace Halomux::Hue::Api
{
  constexpr auto HttpProtocol = "https://";

  /**
   * @brief Reads a bridge address as entered, for the HTTPS URLs of the bridge
   *
   * Drops a scheme, a path and a port; IPv6 addresses are bracketed.
   *
   * @param rawAddress Address as entered
   * @return std::string Host of the URLs, empty if it can't be read
   */
  std::string sanitizeBridgeAddress(
    const std::string& rawAddress
  );


  /**
   * @brief Returns the host of a bridge address for connections without a URL, such as the
   *        streaming connection
   *
   * @param bridgeAddress Sanitized address (see sanitizeBridgeAddress())
   * @return std::string Host, IPv6 addresses without their brackets, their zone plain
   */
  std::string bridgeHost(
    const std::string& bridgeAddress
  );
}
