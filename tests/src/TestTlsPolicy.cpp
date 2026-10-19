#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <unistd.h>

#include <httplib.h>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Client/Client.hpp>

using namespace Halomux;
using Network::Http::Client::TlsPolicy;


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


  std::string fixture(const std::string& name)
  {
    std::ifstream file(std::string(HALOMUX_TEST_FIXTURES) + "/tls/" + name);
    std::stringstream content;
    content << file.rdbuf();
    return content.str();
  }


  // An HTTPS server answering "bridge" with a certificate, in a thread of its own
  struct Server
  {
    httplib::SSLServer server;
    std::thread listener;
    int port{0};

    explicit Server(const std::string& certificate)
    : server((std::string(HALOMUX_TEST_FIXTURES) + "/tls/" + certificate + ".pem").c_str(), (std::string(HALOMUX_TEST_FIXTURES) + "/tls/" + certificate + ".key").c_str())
    {
      server.Get("/api/0/config", [](const httplib::Request&, httplib::Response& res){res.set_content("bridge", "text/plain");});
      port = server.bind_to_any_port("127.0.0.1");
      listener = std::thread([this](){server.listen_after_bind();});
      server.wait_until_ready();
    }

    ~Server()
    {
      server.stop();
      listener.join();
    }

    std::string url() const
    {
      return "https://127.0.0.1:" + std::to_string(port) + "/api/0/config";
    }
  };
}


/**
 * @brief The certificate checks of the requests to the bridge, against a CA of the test's own:
 *        the CA, the name the certificate carries, and the fallback to unverified answers
 */
int main()
{
  const std::string ca = fixture("ca.pem");
  if(ca.empty()){
    Core::Logger::error("Test CA missing");
    return 1;
  }

  // Signed by the CA, named after the bridge's id; a device with a certificate of its own
  Server bridge("bridge");
  Server impostor("impostor");

  const auto request = [](const Server& server, const TlsPolicy& policy){
    return Network::Http::Client::sendRequest(server.url(), "GET", "", {}, policy);
  };

  // The bridge as identified: its CA and its name
  const auto verified = request(bridge, {.caPem = ca, .expectedName = "001788fffe0a0b0c", .allowUnverified = false});
  expect("Bridge verified", verified.has_value() && verified->tlsVerified() && verified->asString() == "bridge");

  // Not identified yet, during the setup: its CA only
  const auto caOnly = request(bridge, {.caPem = ca, .expectedName = "", .allowUnverified = true});
  expect("Bridge verified by its CA alone", caOnly.has_value() && caOnly->tlsVerified());

  // Another bridge's certificate at this address: refused once the bridge was verified
  expect("Another name refused", !request(bridge, {.caPem = ca, .expectedName = "001788fffe0a0b0d", .allowUnverified = false}).has_value());

  // A certificate of its own: refused for a verified bridge, answered as unverified otherwise
  expect("Own certificate refused for a verified bridge", !request(impostor, {.caPem = ca, .expectedName = "001788fffe0a0b0c", .allowUnverified = false}).has_value());
  const auto unverified = request(impostor, {.caPem = ca, .expectedName = "001788fffe0a0b0c", .allowUnverified = true});
  expect("Own certificate answered as unverified", unverified.has_value() && !unverified->tlsVerified());
  const auto setupUnverified = request(impostor, {.caPem = ca, .expectedName = "", .allowUnverified = true});
  expect("Own certificate unverified during the setup", setupUnverified.has_value() && !setupUnverified->tlsVerified());

  // Only the given CA: the system's certificates don't count
  expect("Without the CA refused", !request(bridge, {.caPem = fixture("impostor.pem"), .expectedName = "001788fffe0a0b0c", .allowUnverified = false}).has_value());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("TLS policy: OK");
  return 0;
}
