#pragma once

#include <atomic>
#include <optional>
#include <thread>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Server/HttpServer.hpp>
#include <Halomux/Network/Http/Server/WebrootUtils.hpp>
#include <Halomux/Platform/Selector.hpp>


namespace Halomux::Core
{
  class CoreService;
}

namespace Halomux::Network::Http::Server
{
  class SetupBackend
  {
  public:
    SetupBackend(
      Halomux::Core::CoreService* coreService
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


    /**
     * @brief Serves the setup until it's finished or aborted
     *
     * @param port Port of the setup page
     * @param boundBackendIP Address it listens on
     * @return std::optional<bool> Whether the setup was completed, nothing if it couldn't be
     *         opened, such as on a port in use
     */
    std::optional<bool> execute(
      uint32_t port,
      const std::string& boundBackendIP
    )
    {
      if(!m_httpServer.bind(boundBackendIP, port)){
        return std::nullopt;
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
    Halomux::Core::CoreService* m_coreService;
    HttpServer m_httpServer;
    std::atomic<bool> m_setupCompleted{false};
  };
}
