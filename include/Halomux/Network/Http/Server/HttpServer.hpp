#pragma once

#include <memory>
#include <mutex>

#include <Halomux/Network/Http/Server/HttpDataStructs.hpp>


namespace Halomux::Network::Http::Server
{
  class Impl;


  class HttpServer
  {
  public:
    HttpServer();
    ~HttpServer();

    bool bind(
      const std::string& boundAddress,
      unsigned port
    );

    bool listen();

    bool stop();

    void addRoute(
      HttpMethod method,
      const std::string& path,
      Handler handler
    );

  private:
    // Guards the implementation and the stop request: stop() comes from another thread, possibly
    // before bind(), such as SIGTERM during startup
    std::mutex m_mutex;
    bool m_stopRequested{false};
    std::unique_ptr<Impl> m_httpServerImpl;
    std::vector<Route> m_routes;
  };
}
