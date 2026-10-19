#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Api/BridgeTls.hpp>

using namespace Halomux;


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
 * @brief Checks which bridge ids are accepted for the certificate checks
 *
 */
int main()
{
  using namespace Hue::Api;

  expect("Bridge id", isBridgeId("aabbccddeeff0011"));
  expect("Uppercase bridge id", isBridgeId("AABBCCDDEEFF0011"));
  expect("Empty id refused", !isBridgeId(""));
  expect("Short id refused", !isBridgeId("aabbccddeeff001"));
  expect("Long id refused", !isBridgeId("aabbccddeeff00112"));
  expect("Non-hex id refused", !isBridgeId("aabbccddeeff001g"));
  expect("URL characters refused", !isBridgeId("aabbccdd:eff/011"));

  // An empty name would turn off the name check of the certificate
  expect("Invalid id not registered", !registerBridge("192.0.2.7", "", true));
  expect("Invalid id leaves the address unidentified", bridgeTls("192.0.2.7").expectedName.empty() && bridgeTls("192.0.2.7").allowUnverified);

  expect("Valid id registered", registerBridge("192.0.2.8", "AABBCCDDEEFF0011", true));
  const auto policy = bridgeTls("192.0.2.8");
  expect("Certificate name is the lowercase id", policy.expectedName == "aabbccddeeff0011" && !policy.allowUnverified);

  // A bridge whose certificate isn't signed by the CA: its public key, read when it was confirmed
  expect("Unverified bridge registered", registerBridge("192.0.2.9", "aabbccddeeff0011", false, "sha256//key"));
  const auto unverified = bridgeTls("192.0.2.9");
  expect("Unverified bridge pinned to its key", unverified.allowUnverified && unverified.pinnedPublicKey == "sha256//key");
  expect("Verified bridge checked by its CA, not a key", registerBridge("192.0.2.10", "aabbccddeeff0011", true, "sha256//key") && bridgeTls("192.0.2.10").pinnedPublicKey.empty());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Bridge TLS: OK");
  return 0;
}
