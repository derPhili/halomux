#include <Halomux/Hue/Api/BridgeAddress.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>


namespace Halomux::Hue::Api
{
  namespace
  {
    bool startsWithIgnoreCase(
      std::string_view value,
      std::string_view prefix
    )
    {
      if(value.size() < prefix.size()){
        return false;
      }

      return std::equal(
        prefix.begin(),
        prefix.end(),
        value.begin(),
        [](char a, char b){
          return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        }
      );
    }
  }


  std::string sanitizeBridgeAddress(
    const std::string& rawAddress
  )
  {
    std::string address = rawAddress;

    static constexpr std::string_view HttpsPrefix = "https://";
    static constexpr std::string_view HttpPrefix = "http://";

    if(startsWithIgnoreCase(address, HttpsPrefix)){
      address.erase(0, HttpsPrefix.size());
    }
    else if(startsWithIgnoreCase(address, HttpPrefix)){
      address.erase(0, HttpPrefix.size());
    }

    auto slash = address.find('/');
    if(slash != std::string::npos){
      address.erase(slash);
    }

    while(!address.empty() && address.back() == '/'){
      address.pop_back();
    }

    // The bridge answers on its standard ports only, and the streaming connection has a port of
    // its own: a port given is dropped. IPv6 addresses are bracketed, as URLs need them.
    if(address.starts_with('[')){
      const size_t close = address.find(']');
      if(close == std::string::npos){
        return {};
      }
      address.erase(close + 1);
    }
    else if(std::count(address.begin(), address.end(), ':') == 1){
      address.erase(address.find(':'));
    }
    else if(address.find(':') != std::string::npos){
      address = "[" + address + "]";
    }

    return address;
  }


  std::string bridgeHost(
    const std::string& bridgeAddress
  )
  {
    if(bridgeAddress.size() >= 2 && bridgeAddress.front() == '[' && bridgeAddress.back() == ']'){
      return bridgeAddress.substr(1, bridgeAddress.size() - 2);
    }

    return bridgeAddress;
  }
}

