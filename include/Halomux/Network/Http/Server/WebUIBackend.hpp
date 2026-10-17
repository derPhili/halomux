#pragma once

#include <future>

#include <Halomux/Network/Http/Server/HttpServer.hpp>
#include <Halomux/Network/Http/Server/WebrootUtils.hpp>


namespace Halomux::Core
{
  class CoreService;
}

namespace Halomux::Network::Http::Server
{
  class WebUIBackend
  {
  public:
    WebUIBackend(
      Halomux::Core::CoreService* coreService
    );

    ~WebUIBackend(){}

    bool start(
      uint32_t port,
      const std::string& boundBackendIP,
      std::promise<bool>&& readyPromise
    )
    {
      if(!m_httpServer.bind(boundBackendIP, port)){
        readyPromise.set_value(false);
        return false;
      }

      readyPromise.set_value(true);
      return m_httpServer.listen();
    }


    bool stop()
    {
      return m_httpServer.stop();
    }

  private:
    Halomux::Core::CoreService* m_coreService;
    HttpServer m_httpServer;
  };
}
