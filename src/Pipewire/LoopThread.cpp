#include <Halomux/Pipewire/LoopThread.hpp>


namespace Halomux::Pipewire
{
  LoopThread::LoopThread(
    Setup setup,
    Teardown teardown
  )
  {
    // Created here: valid for the other threads as soon as the constructor returns
    pw_init(nullptr, nullptr);
    m_loop = pw_main_loop_new(nullptr);
    m_quitEvent = pw_loop_add_event(pw_main_loop_get_loop(m_loop), _onQuit, this);

    m_thread = std::thread([this, setup = std::move(setup), teardown = std::move(teardown)](){
      if(setup(m_loop)){
        pw_main_loop_run(m_loop);
      }

      teardown(m_loop);

      // The quitting thread may still be in pw_loop_signal_event()
      m_quitReturned.wait(false);

      pw_loop_destroy_source(pw_main_loop_get_loop(m_loop), m_quitEvent);
      pw_main_loop_destroy(m_loop);
      pw_deinit();
    });
  }


  LoopThread::~LoopThread()
  {
    quit();
    m_thread.join();
  }


  pw_main_loop* LoopThread::loop() const
  {
    return m_loop;
  }


  void LoopThread::quit()
  {
    if(m_quitRequested.exchange(true)){
      return;
    }

    // An event stays pending until the loop runs, unlike pw_main_loop_quit() from another thread
    pw_loop_signal_event(pw_main_loop_get_loop(m_loop), m_quitEvent);

    m_quitReturned = true;
    m_quitReturned.notify_one();
  }


  void LoopThread::_onQuit(
    void* userData,
    uint64_t /*count*/
  )
  {
    // In the loop thread
    pw_main_loop_quit(static_cast<LoopThread*>(userData)->m_loop);
  }
}
