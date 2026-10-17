#include <Halomux/Network/Http/Server/HostCheck.hpp>

#include <algorithm>
#include <cctype>
#include <string>

#include <arpa/inet.h>


namespace Halomux::Network::Http::Server
{
  namespace
  {
    bool isPort(
      std::string_view port
    )
    {
      return !port.empty() && port.size() <= 5 && std::all_of(port.begin(), port.end(), [](char c){return std::isdigit(static_cast<unsigned char>(c));});
    }
  }


  bool isAllowedHost(
    std::string_view host
  )
  {
    std::string_view name;
    std::string_view port;
    bool bracketed = false;

    if(host.starts_with('[')){
      // IPv6 address: [address] or [address]:port
      const size_t end = host.find(']');
      if(end == std::string_view::npos){
        return false;
      }

      name = host.substr(1, end - 1);
      const std::string_view rest = host.substr(end + 1);
      if(!rest.empty()){
        if(!rest.starts_with(':') || !isPort(rest.substr(1))){
          return false;
        }
      }
      bracketed = true;
    }
    else{
      const size_t colon = host.rfind(':');
      name = host.substr(0, colon);
      if(colon != std::string_view::npos){
        port = host.substr(colon + 1);
        if(!isPort(port)){
          return false;
        }
      }
    }

    if(name.empty()){
      return false;
    }

    const std::string nameString(name);
    unsigned char address[16];

    if(bracketed){
      return inet_pton(AF_INET6, nameString.c_str(), address) == 1;
    }

    if(inet_pton(AF_INET, nameString.c_str(), address) == 1){
      return true;
    }

    std::string lowercase = nameString;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return lowercase == "localhost";
  }
}
