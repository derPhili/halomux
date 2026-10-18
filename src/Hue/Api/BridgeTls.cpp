#include <Halomux/Hue/Api/BridgeTls.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>


namespace Halomux::Hue::Api
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


  bool isBridgeId(
    std::string_view bridgeId
  )
  {
    return bridgeId.size() == 16 && std::all_of(bridgeId.begin(), bridgeId.end(), [](unsigned char c){return std::isxdigit(c) != 0;});
  }


  bool registerBridge(
    const std::string& bridgeAddress,
    const std::string& bridgeId,
    bool requireVerification
  )
  {
    // Anything else could empty the expected name, which would turn off the name check
    if(!isBridgeId(bridgeId)){
      return false;
    }

    // The certificate name is the lowercase bridge id
    std::string name = bridgeId;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){return std::tolower(c);});

    std::lock_guard lock(registryMutex);
    registry[bridgeAddress] = {name, requireVerification};
    return true;
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
