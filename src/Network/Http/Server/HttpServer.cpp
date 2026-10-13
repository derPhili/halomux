#include <Huenicorn/Network/Http/Server/HttpServer.hpp>

#include <Huenicorn/Network/Http/Server/Impl/HttpLibServerImpl.hpp>


namespace Huenicorn::Network::Http::Server
{
  HttpServer::HttpServer()
  {
  }


  HttpServer::~HttpServer()
  {
  }


  bool HttpServer::bind(
    const std::string& boundAddress,
    unsigned port
  )
  {
    m_httpServerImpl = std::make_unique<Impl>(m_routes);

    return m_httpServerImpl->bind(boundAddress, port);
  }


  bool HttpServer::listen()
  {
    if(!m_httpServerImpl){
      return false;
    }

    return m_httpServerImpl->listen();
  }


  bool HttpServer::stop()
  {
    if(!m_httpServerImpl){
      return false;
    }

    return m_httpServerImpl->stop();
  }

  void HttpServer::addRoute(
    HttpMethod method,
    const std::string& path,
    Handler handler
  )
  {
    m_routes.push_back({method, path, handler});
  }
}
