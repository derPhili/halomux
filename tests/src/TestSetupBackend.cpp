#include <httplib.h>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Server/SetupBackend.hpp>

using namespace Halomux;


/**
 * @brief The setup can't open on a port in use: told apart from a setup the user aborted, so the
 *        service fails and systemd starts it again
 */
int main()
{
  // Takes the port first, as another service would
  httplib::Server other;
  const int port = other.bind_to_any_port("127.0.0.1");
  if(port <= 0){
    Core::Logger::error("No free port for the test");
    return 1;
  }

  Network::Http::Server::SetupBackend setup(nullptr);
  const auto completed = setup.execute(static_cast<uint32_t>(port), "127.0.0.1");
  if(completed.has_value()){
    Core::Logger::error("A setup that couldn't open counts as aborted");
    return 1;
  }

  Core::Logger::log("Setup backend: OK");
  return 0;
}
