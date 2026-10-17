#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>

using namespace Halomux;
using namespace std::chrono_literals;


namespace
{
  std::atomic<int> ticks{0};

  void onTimer(void* /*data*/, uint64_t /*expirations*/)
  {
    ticks++;
  }
}


/**
 * @brief A screen selection that fails, like one the user cancels, must leave the running
 *        capture's Pipewire loop working
 */
int main()
{
  // No portal to reach: the selection fails right away
  setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/halomux-test-bus", 1);

  // The running grabber's Pipewire thread
  pw_main_loop* loop = nullptr;
  std::atomic<bool> loopReady{false};
  std::thread running([&](){
    pw_init(nullptr, nullptr);
    loop = pw_main_loop_new(nullptr);
    spa_source* timer = pw_loop_add_timer(pw_main_loop_get_loop(loop), onTimer, nullptr);
    timespec interval{0, 10'000'000};
    pw_loop_update_timer(pw_main_loop_get_loop(loop), timer, &interval, &interval, false);
    loopReady = true;
    pw_main_loop_run(loop);
    pw_loop_destroy_source(pw_main_loop_get_loop(loop), timer);
    pw_main_loop_destroy(loop);
    pw_deinit();
  });

  while(!loopReady){
    std::this_thread::sleep_for(1ms);
  }

  Core::Config config(std::filesystem::temp_directory_path() / "halomux-test-cancel");
  bool cancelled = false;
  try{
    Grabber::PipewireGrabber grabber(&config, true);
  }
  catch(const Grabber::GrabberCancelled&){
    cancelled = true;
  }

  const int before = ticks;
  std::this_thread::sleep_for(300ms);
  const int after = ticks;

  pw_main_loop_quit(loop);
  running.join();

  if(!cancelled){
    Core::Logger::error("The screen selection didn't fail");
    return 1;
  }

  if(after - before < 5){
    Core::Logger::error("The running Pipewire loop stopped after a failed screen selection (", after - before, " ticks in 300 ms)");
    return 1;
  }

  Core::Logger::log("Grabber cancel: OK");
  return 0;
}
