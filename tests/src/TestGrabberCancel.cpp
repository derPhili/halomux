#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>
#include <Halomux/Pipewire/LoopThread.hpp>

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

  // The running grabber's Pipewire loop, ticking every 10 ms
  spa_source* timer = nullptr;
  auto running = std::make_unique<Pipewire::LoopThread>(
    [&timer](pw_main_loop* loop){
      timer = pw_loop_add_timer(pw_main_loop_get_loop(loop), onTimer, nullptr);
      timespec interval{0, 10'000'000};
      pw_loop_update_timer(pw_main_loop_get_loop(loop), timer, &interval, &interval, false);
      return true;
    },
    [&timer](pw_main_loop* loop){pw_loop_destroy_source(pw_main_loop_get_loop(loop), timer);}
  );

  Core::Config config(std::filesystem::temp_directory_path() / "halomux-test-cancel");
  bool cancelled = false;
  bool unavailable = false;
  try{
    Grabber::PipewireGrabber grabber(&config, true);
  }
  catch(const Grabber::GrabberCancelled&){
    cancelled = true;
  }
  catch(const Grabber::GrabberUnavailable&){
    // No portal is a failure worth starting again, unlike a selection the user cancelled
    unavailable = true;
  }

  const int before = ticks;
  std::this_thread::sleep_for(300ms);
  const int after = ticks;

  running.reset();

  if(!unavailable){
    Core::Logger::error(cancelled ? "An unreachable portal was taken for a cancelled selection" : "The screen selection didn't fail");
    return 1;
  }

  if(after - before < 5){
    Core::Logger::error("The running Pipewire loop stopped after a failed screen selection (", after - before, " ticks in 300 ms)");
    return 1;
  }

  Core::Logger::log("Grabber cancel: OK");
  return 0;
}
