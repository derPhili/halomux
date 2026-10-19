#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Api/BridgeAddress.hpp>

using namespace Halomux;
using Hue::Api::bridgeHost;
using Hue::Api::sanitizeBridgeAddress;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }
}


/**
 * @brief Bridge addresses as users enter them, for HTTPS URLs and the streaming connection
 */
int main()
{
  // As entered: with a scheme, a path or trailing slashes
  expect("Address", sanitizeBridgeAddress("192.168.0.42") == "192.168.0.42");
  expect("Scheme and path dropped", sanitizeBridgeAddress("HTTPS://192.168.0.42/api/") == "192.168.0.42");
  expect("Name", sanitizeBridgeAddress("hue-bridge.local") == "hue-bridge.local");

  // The bridge answers on its standard ports only: the streaming connection has its own
  expect("Port dropped", sanitizeBridgeAddress("192.168.0.42:443") == "192.168.0.42");
  expect("Port of a name dropped", sanitizeBridgeAddress("http://hue-bridge.local:80/") == "hue-bridge.local");

  // IPv6: bracketed in URLs
  expect("Bracketed IPv6", sanitizeBridgeAddress("[fd00::42]") == "[fd00::42]");
  expect("Bare IPv6 bracketed", sanitizeBridgeAddress("fd00::42") == "[fd00::42]");
  expect("Port of an IPv6 dropped", sanitizeBridgeAddress("https://[fd00::42]:443/") == "[fd00::42]");
  expect("Unclosed bracket refused", sanitizeBridgeAddress("[fd00::42").empty());

  // The streaming connection takes the host without brackets
  expect("Host of an address", bridgeHost("192.168.0.42") == "192.168.0.42");
  expect("Host of an IPv6", bridgeHost("[fd00::42]") == "fd00::42");
  // A zone written as in URLs (RFC 6874), which name resolution doesn't take
  expect("Zone of a link local address", sanitizeBridgeAddress("https://[fe80::42%25eth0]/") == "[fe80::42%25eth0]");
  expect("Host with its zone", bridgeHost("[fe80::42%25eth0]") == "fe80::42%eth0");
  expect("Host with a plain zone", bridgeHost("[fe80::42%eth0]") == "fe80::42%eth0");

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Bridge address: OK");
  return 0;
}
