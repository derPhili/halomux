#include <Halomux/Network/Http/Server/HttpServer.hpp>

#include <Halomux/Network/Http/Server/Impl/HttpLibServerImpl.hpp>


namespace Halomux::Network::Http::Server
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
    std::lock_guard lock(m_mutex);
    m_httpServerImpl = std::make_unique<Impl>(m_routes);

    return m_httpServerImpl->bind(boundAddress, port);
  }


  bool HttpServer::listen()
  {
    Impl* impl = nullptr;
    {
      std::lock_guard lock(m_mutex);
      // Stopped meanwhile: the stop would otherwise be lost, as nothing listened yet
      if(m_stopRequested || !m_httpServerImpl){
        return false;
      }
      impl = m_httpServerImpl.get();
    }

    return impl->listen();
  }


  bool HttpServer::stop()
  {
    std::lock_guard lock(m_mutex);
    m_stopRequested = true;

    if(!m_httpServerImpl){
      return true;
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
