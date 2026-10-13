#pragma once

#include <atomic>
#include <thread>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Network/Http/Server/HttpServer.hpp>
#include <Huenicorn/Network/Http/Server/WebrootUtils.hpp>
#include <Huenicorn/Platform/Selector.hpp>


namespace Huenicorn::Core
{
  class CoreService;
}

namespace Huenicorn::Network::Http::Server
{
  class SetupBackend
  {
  public:
    SetupBackend(
      Huenicorn::Core::CoreService* coreService
    );

    ~SetupBackend(){}


    /**
     * @brief Aborts the setup, unblocking execute()
     *
     */
    void abort()
    {
      m_setupCompleted = false;
      m_httpServer.stop();
    }


    bool execute(
      uint32_t port,
      const std::string& boundBackendIP
    )
    {
      if(!m_httpServer.bind(boundBackendIP, port)){
        return false;
      }

      std::thread spawnBrowserThread([port](){
        std::stringstream serviceUrlStream;
        serviceUrlStream << "http://127.0.0.1:" << port;
        std::string serviceURL = serviceUrlStream.str();
        Core::Logger::log("Setup WebUI is ready and available at ", serviceURL);

        Platform::adapter.openWebBrowser(serviceURL);
      });
      spawnBrowserThread.detach();

      m_httpServer.listen();

      return m_setupCompleted;
    }

  private:
    Huenicorn::Core::CoreService* m_coreService;
    HttpServer m_httpServer;
    std::atomic<bool> m_setupCompleted{false};
  };
}
