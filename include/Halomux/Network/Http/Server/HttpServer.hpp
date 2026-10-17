#pragma once

#include <memory>

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
    std::unique_ptr<Impl> m_httpServerImpl;
    std::vector<Route> m_routes;
  };
}
