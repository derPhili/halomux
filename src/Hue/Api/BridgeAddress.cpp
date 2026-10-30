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

      // After the brackets, only a port
      const std::string_view rest(address.data() + close + 1, address.size() - close - 1);
      if(!rest.empty() && (rest.front() != ':' || !std::all_of(rest.begin() + 1, rest.end(), [](unsigned char c){return std::isdigit(c);}))){
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

    // Only what a host is made of: anything else would change what the URL means, such as a
    // user name before an @ or a query after a ?
    const auto all = [](std::string_view text, auto allowed){return !text.empty() && std::all_of(text.begin(), text.end(), allowed);};
    if(address.starts_with('[')){
      const std::string_view inner(address.data() + 1, address.size() - 2);
      const size_t zone = inner.find('%');
      const std::string_view ip = inner.substr(0, zone);
      // A zone written plain or as in URLs (%25)
      std::string_view zoneId = zone == std::string_view::npos ? std::string_view{} : inner.substr(zone + 1);
      if(zoneId.starts_with("25")){
        zoneId.remove_prefix(2);
      }
      const bool ipValid = all(ip, [](unsigned char c){return std::isxdigit(c) || c == ':' || c == '.';});
      const bool zoneValid = zone == std::string_view::npos || all(zoneId, [](unsigned char c){return std::isalnum(c) || c == '.' || c == '_' || c == '-';});
      if(!ipValid || !zoneValid){
        return {};
      }
    }
    else if(!all(address, [](unsigned char c){return std::isalnum(c) || c == '.' || c == '-';})){
      return {};
    }

    return address;
  }


  std::string bridgeHost(
    const std::string& bridgeAddress
  )
  {
    if(bridgeAddress.size() >= 2 && bridgeAddress.front() == '[' && bridgeAddress.back() == ']'){
      std::string host = bridgeAddress.substr(1, bridgeAddress.size() - 2);

      // A zone written as in URLs (RFC 6874): name resolution takes it plain
      if(const size_t zone = host.find("%25"); zone != std::string::npos){
        host.erase(zone + 1, 2);
      }

      return host;
    }

    return bridgeAddress;
  }
}

