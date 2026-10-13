#include <csignal>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <pthread.h>

#include <Huenicorn/App/CommandLine.hpp>
#include <Huenicorn/Core/Runtime.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Platform/Selector.hpp>
#include <Huenicorn/Version.hpp>


/**
 * @brief Wrapper around threaded application
 * 
 */
class Application
{
public:
  bool start()
  {
    try{
      std::lock_guard lock(m_coreMutex);
      m_core = std::make_unique<Huenicorn::Core::Runtime>(Huenicorn::Version, Huenicorn::Platform::adapter.getConfigFilePath());
    }
    catch(const std::exception& e){
      Huenicorn::Core::Logger::error(e.what());
      return false;
    }

    bool succeeded = true;
    m_applicationThread.emplace([&](){
      try{
        m_core->start();
      }
      catch(const std::exception& e){
        Huenicorn::Core::Logger::error(e.what());
        succeeded = false;
      }
    });

    m_applicationThread.value().join();
    m_applicationThread.reset();

    std::lock_guard lock(m_coreMutex);
    m_core.reset();
    Huenicorn::Platform::adapter.releaseGrabber();

    return succeeded;
  }


  void stop()
  {
    std::lock_guard lock(m_coreMutex);
    if(!m_core){
      return;
    }

    m_core->stop();
  }


private:
  std::mutex m_coreMutex;
  std::unique_ptr<Huenicorn::Core::Runtime> m_core;
  std::optional<std::thread> m_applicationThread;
};


Application app;


int main(
  int argc,
  char* argv[]
)
{
  Huenicorn::App::CommandLine::Params params;
  Huenicorn::App::CommandLine::getOpt(argc, argv, params);
  if(params.shouldStop){
    return 0;
  }

  Huenicorn::Core::Logger::log("Starting Huenicorn version ", Huenicorn::Version, " for ", Huenicorn::Platform::adapter.getPlatformName());

  // Termination signals are handled synchronously by a dedicated thread, so stopping
  // the application doesn't run (non async-signal-safe) code inside a signal handler.
  // The mask must be set before spawning any other thread so they all inherit it.
  // SIGUSR1 is only used to release the signal thread once the application is done.
  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  sigaddset(&signals, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &signals, nullptr);

  std::thread signalThread([&signals](){
    int signal = 0;
    while(sigwait(&signals, &signal) == 0){
      if(signal == SIGUSR1){
        return;
      }

      Huenicorn::Core::Logger::log("Closing application");
      app.stop();
    }
  });

  bool succeeded = app.start();

  pthread_kill(signalThread.native_handle(), SIGUSR1);
  signalThread.join();

  if(!succeeded){
    Huenicorn::Core::Logger::error("Huenicorn terminated because of an error");
    return 1;
  }

  Huenicorn::Core::Logger::log("Huenicorn terminated properly");

  return 0;
}
