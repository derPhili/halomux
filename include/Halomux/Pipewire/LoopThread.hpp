#pragma once

#include <atomic>
#include <functional>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop


namespace Halomux::Pipewire
{
  /**
   * @brief A Pipewire main loop running in a thread of its own, safe to stop from any thread
   *
   * pw_main_loop_quit() has two traps when called from another thread: before the loop runs, its
   * effect is lost when pw_main_loop_run() starts (the thread never ends), and the loop thread
   * may destroy the loop, or release Pipewire, while the quitting call still runs (use after
   * free). This class quits through an event that stays pending until the loop runs, and
   * destroys the loop only once the quitting thread is done with it.
   */
  class LoopThread
  {
  public:
    // Runs in the loop thread before the loop: returns whether to run it
    using Setup = std::function<bool(pw_main_loop* loop)>;
    // Runs in the loop thread after the loop, whether it ran or not, before it's destroyed
    using Teardown = std::function<void(pw_main_loop* loop)>;


    /**
     * @brief Initializes Pipewire, creates the loop and starts its thread
     *
     * @param setup Runs in the loop thread before the loop
     * @param teardown Runs in the loop thread after the loop
     */
    LoopThread(
      Setup setup,
      Teardown teardown
    );


    /**
     * @brief Quits the loop and waits for its thread
     *
     */
    ~LoopThread();


    LoopThread(const LoopThread&) = delete;
    LoopThread& operator=(const LoopThread&) = delete;


    /**
     * @brief Returns the loop, valid as long as this object lives
     *
     * @return pw_main_loop* Loop, for pw_loop_invoke() from other threads
     */
    pw_main_loop* loop() const;


    /**
     * @brief Asks the loop to quit, from any thread, even before it runs
     *
     */
    void quit();


  private:
    static void _onQuit(
      void* userData,
      uint64_t count
    );


    pw_main_loop* m_loop{nullptr};
    spa_source* m_quitEvent{nullptr};
    std::atomic<bool> m_quitRequested{false};
    // Set once the quitting call returned: the loop thread waits for it before destroying the loop
    std::atomic<bool> m_quitReturned{false};
    std::thread m_thread;
  };
}
