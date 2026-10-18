#include <atomic>
#include <chrono>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Pipewire/LoopThread.hpp>

using namespace Halomux;
using namespace std::chrono_literals;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }


  int onInvoke(spa_loop* /*loop*/, bool /*async*/, uint32_t /*seq*/, const void* /*data*/, size_t /*size*/, void* userData)
  {
    static_cast<std::atomic<int>*>(userData)->fetch_add(1);
    return 0;
  }
}


/**
 * @brief Starts and stops Pipewire loop threads in every order. Run with AddressSanitizer:
 *        a loop destroyed while being quit is a use after free, a quit lost before the loop
 *        runs hangs (ctest's timeout).
 */
int main()
{
  // Stopped before the loop runs: the quit must not be lost
  for(int i = 0; i < 300; i++){
    Pipewire::LoopThread thread([](pw_main_loop*){return true;}, [](pw_main_loop*){});
  }

  // Stopped while it runs
  for(int i = 0; i < 300; i++){
    std::atomic<bool> running{false};
    Pipewire::LoopThread thread([&running](pw_main_loop*){running = true; return true;}, [](pw_main_loop*){});
    while(!running){
      std::this_thread::yield();
    }
    std::this_thread::sleep_for(100us);
  }

  // Setup failed in the thread: the loop doesn't run, and stopping still works
  {
    std::atomic<int> tornDown{0};
    for(int i = 0; i < 100; i++){
      Pipewire::LoopThread thread([](pw_main_loop*){return false;}, [&tornDown](pw_main_loop*){tornDown++;});
    }
    expect("Torn down after a failed setup", tornDown == 100);
  }

  // Calls from other threads reach the loop
  {
    std::atomic<int> calls{0};
    {
      Pipewire::LoopThread thread([](pw_main_loop*){return true;}, [](pw_main_loop*){});
      for(int i = 0; i < 10; i++){
        pw_loop_invoke(pw_main_loop_get_loop(thread.loop()), onInvoke, 0, nullptr, 0, true, &calls);
      }
    }
    expect("Invocations from another thread run", calls == 10);
  }

  // Quitting twice is harmless
  {
    Pipewire::LoopThread thread([](pw_main_loop*){return true;}, [](pw_main_loop*){});
    thread.quit();
    thread.quit();
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Loop thread: OK");
  return 0;
}
