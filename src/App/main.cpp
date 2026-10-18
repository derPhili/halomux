#include <csignal>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <pthread.h>

#include <Halomux/App/CommandLine.hpp>
#include <Halomux/Core/Runtime.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Platform/Selector.hpp>
#include <Halomux/Version.hpp>


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
      m_core = std::make_unique<Halomux::Core::Runtime>(Halomux::Version, Halomux::Platform::adapter.getConfigFilePath());
    }
    catch(const std::exception& e){
      Halomux::Core::Logger::error(e.what());
      return false;
    }

    bool succeeded = true;
    m_applicationThread.emplace([&](){
      try{
        succeeded = m_core->start();
      }
      catch(const std::exception& e){
        Halomux::Core::Logger::error(e.what());
        succeeded = false;
      }
    });

    m_applicationThread.value().join();
    m_applicationThread.reset();

    std::lock_guard lock(m_coreMutex);
    // Destroys the grabber too, before the configuration it reads
    m_core.reset();

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
  std::unique_ptr<Halomux::Core::Runtime> m_core;
  std::optional<std::thread> m_applicationThread;
};


Application app;


int main(
  int argc,
  char* argv[]
)
{
  Halomux::App::CommandLine::Params params;
  Halomux::App::CommandLine::getOpt(argc, argv, params);
  if(params.shouldStop){
    return 0;
  }

  Halomux::Core::Logger::log("Starting ", Halomux::AppName, " ", Halomux::Version, " for ", Halomux::Platform::adapter.getPlatformName());

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

      Halomux::Core::Logger::log("Closing application");
      app.stop();
    }
  });

  bool succeeded = app.start();

  pthread_kill(signalThread.native_handle(), SIGUSR1);
  signalThread.join();

  if(!succeeded){
    Halomux::Core::Logger::error(Halomux::AppName, " terminated because of an error");
    return 1;
  }

  Halomux::Core::Logger::log(Halomux::AppName, " terminated properly");

  return 0;
}
