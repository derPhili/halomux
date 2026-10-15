#include <Huenicorn/Hue/Api/BridgeTls.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>


namespace Huenicorn::Hue::Api
{
  namespace
  {
    struct RegisteredBridge
    {
      std::string bridgeId;
      bool requireVerification;
    };

    std::mutex registryMutex;
    std::map<std::string, RegisteredBridge> registry;
  }


  void registerBridge(
    const std::string& bridgeAddress,
    const std::string& bridgeId,
    bool requireVerification
  )
  {
    // The certificate name is the lowercase bridge id
    std::string name = bridgeId;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){return std::tolower(c);});

    std::lock_guard lock(registryMutex);
    registry[bridgeAddress] = {name, requireVerification};
  }


  Network::Http::Client::TlsPolicy bridgeTls(
    const std::string& bridgeAddress
  )
  {
    std::lock_guard lock(registryMutex);

    const auto bridgeIt = registry.find(bridgeAddress);
    if(bridgeIt == registry.end()){
      // Not identified yet (setup): only the CA can be checked
      return {.caPem = BridgeRootCertificate, .expectedName = {}, .allowUnverified = true};
    }

    return {
      .caPem = BridgeRootCertificate,
      .expectedName = bridgeIt->second.bridgeId,
      .allowUnverified = !bridgeIt->second.requireVerification
    };
  }
}
