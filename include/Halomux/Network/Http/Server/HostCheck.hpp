#pragma once

#include <string_view>


namespace Halomux::Network::Http::Server
{
  /**
   * @brief Tells whether the light manager answers a request for this Host header
   *
   * A page of another site can make its own domain name resolve to this machine (DNS rebinding)
   * and then read and drive the API as if it were the same origin. Only names that can't be
   * rebound are accepted: IP addresses, which also covers access from other devices when bound
   * to 0.0.0.0, and localhost.
   *
   * @param host Value of the Host header, with an optional port
   * @return true The host is an IP address or localhost
   * @return false Any other name, or a malformed value
   */
  bool isAllowedHost(
    std::string_view host
  );
}
