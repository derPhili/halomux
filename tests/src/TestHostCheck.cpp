#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Server/HostCheck.hpp>

using namespace Halomux;
using Network::Http::Server::isAllowedHost;


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


int main()
{
  // The light manager itself, locally or from another device when bound to 0.0.0.0
  expect("Loopback", isAllowedHost("127.0.0.1:8215"));
  expect("Loopback without port", isAllowedHost("127.0.0.1"));
  expect("localhost", isAllowedHost("localhost:8215"));
  expect("localhost in capitals", isAllowedHost("LocalHost:8215"));
  expect("IPv6 loopback", isAllowedHost("[::1]:8215"));
  expect("IPv6 address", isAllowedHost("[fe80::1]:8215"));
  expect("LAN address", isAllowedHost("192.168.0.42:8215"));

  // Names a page could rebind to the local address
  expect("Foreign domain rejected", !isAllowedHost("evil.example:8215"));
  expect("Domain looking like localhost rejected", !isAllowedHost("localhost.evil.example:8215"));
  expect("Domain resolving to an address rejected", !isAllowedHost("127.0.0.1.nip.io:8215"));
  expect("Missing host rejected", !isAllowedHost(""));
  expect("Only a port rejected", !isAllowedHost(":8215"));
  expect("Unclosed IPv6 bracket rejected", !isAllowedHost("[::1:8215"));
  expect("Garbage after an address rejected", !isAllowedHost("127.0.0.1x:8215"));
  expect("Non numeric port rejected", !isAllowedHost("127.0.0.1:abc"));

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Host check: OK");
  return 0;
}
