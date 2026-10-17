#pragma once

#include <array>
#include <atomic>
#include <optional>
#include <thread>

#include <Halomux/Pipewire/LoopThread.hpp>
#include <Halomux/Audio/TransientDetector.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#pragma GCC diagnostic pop


namespace Halomux::Audio
{
  /**
   * @brief Detects transients (sudden loudness jumps) in the sound played by the PC
   *
   * Captures the monitor of the default audio output through Pipewire, in its own thread, and
   * runs a TransientDetector per stereo channel.
   */
  class AudioAnalyzer
  {
  public:
    // Trigger in [0, 1] per channel: left, right
    using Triggers = std::array<float, 2>;


    /**
     * @brief Starts capturing and analyzing the default audio output
     *
     */
    AudioAnalyzer();


    /**
     * @brief Stops the capture
     *
     */
    ~AudioAnalyzer();

    AudioAnalyzer(const AudioAnalyzer&) = delete;
    AudioAnalyzer& operator=(const AudioAnalyzer&) = delete;


    /**
     * @brief Returns the strongest trigger per channel since the previous call, and resets them
     *
     * @return Triggers Left and right triggers in [0, 1]
     */
    Triggers takeTriggers();


  private:
    // Methods, in the loop thread
    bool _connect(
      pw_main_loop* loop
    );

    void _disconnect();

    static void _onStreamParamChanged(
      void* userData,
      uint32_t id,
      const spa_pod* param
    );

    static void _onStreamProcess(
      void* userData
    );


    // Attributes
    pw_stream* m_stream{nullptr};

    // Pipewire thread only
    uint32_t m_rate{48000};
    uint32_t m_channels{2};
    std::array<TransientDetector, 2> m_detectors;

    // Shared with the consumer
    std::array<std::atomic<float>, 2> m_triggers{};

    // Declared last: stopped before the state its callbacks use goes away
    std::optional<Pipewire::LoopThread> m_loopThread;
  };
}
