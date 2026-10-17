#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace Halomux::Hue::Api
{
  /**
   * @brief Hue bridge found in the local network
   *
   */
  struct DiscoveredBridge
  {
    std::string id;
    std::string address;
  };


  /**
   * @brief Finds the Hue bridges of the local network through mDNS (service _hue._tcp)
   *
   * Works without internet access, unlike the discovery service of Philips.
   *
   * @param timeout How long to wait for answers
   * @return std::vector<DiscoveredBridge> Bridges that answered
   */
  std::vector<DiscoveredBridge> discoverBridges(
    std::chrono::milliseconds timeout
  );


  /**
   * @brief Reads an mDNS answer, received from the network: malformed input is rejected
   *
   * @param data Message
   * @param size Message size
   * @return std::optional<std::string> Bridge id from the TXT record, an empty string if the answer
   *         is about the Hue service without one, nothing if it isn't about it or is malformed
   */
  std::optional<std::string> parseDiscoveryAnswer(
    const uint8_t* data,
    size_t size
  );
}
