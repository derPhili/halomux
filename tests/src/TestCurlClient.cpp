#include <string>
#include <thread>

#include <unistd.h>

#include <httplib.h>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Client/Client.hpp>

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
 * @brief Checks the limits of HTTP requests, whose answers come from the network
 *
 */
int main()
{
  httplib::Server server;
  // A bridge's whole resource list is about 140 KB
  const std::string usual(200 * 1024, 'x');
  const std::string huge(Network::Http::Client::MaxResponseSize + 1024 * 1024, 'x');
  server.Get("/usual", [&](const httplib::Request&, httplib::Response& res){res.set_content(usual, "text/plain");});
  server.Get("/huge", [&](const httplib::Request&, httplib::Response& res){res.set_content(huge, "text/plain");});

  const int port = 19000 + static_cast<int>(getpid() % 1000);
  if(!server.bind_to_port("127.0.0.1", port)){
    Core::Logger::error("Could not bind the test server");
    return 1;
  }
  std::thread listener([&server](){server.listen_after_bind();});
  server.wait_until_ready();

  const std::string base = "http://127.0.0.1:" + std::to_string(port);

  const auto ok = Network::Http::Client::sendRequest(base + "/usual", "GET");
  expect("Usual answer read", ok.has_value() && ok->asString() == usual);

  // A device posing as the bridge can't fill the memory
  expect("Oversized answer refused", !Network::Http::Client::sendRequest(base + "/huge", "GET").has_value());

  server.stop();
  listener.join();

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Curl client: OK");
  return 0;
}
