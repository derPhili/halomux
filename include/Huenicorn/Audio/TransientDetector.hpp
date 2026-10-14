#pragma once

#include <algorithm>
#include <cmath>


namespace Huenicorn::Audio
{
  /**
   * @brief Detects sudden loudness jumps in a stream of audio buffer energies
   *
   * Each buffer's mean power is compared with a short-term exponential average of the previous
   * ones: a jump of TriggerThresholdDb starts a trigger, FullTriggerDb gives a full one.
   * Steady sound, however loud, quickly stops triggering as the average catches up.
   */
  class TransientDetector
  {
  public:
    // Loudness jump [dB] over the short-term average starting a trigger, and giving a full one
    static constexpr float TriggerThresholdDb = 6.f;
    static constexpr float FullTriggerDb = 12.f;

    // Time constant of the short-term average [s]
    static constexpr float AverageTimeConstant = 0.5f;

    // Buffers quieter than this mean power (about -50 dBFS) never trigger
    static constexpr float NoiseFloor = 1e-5f;


    /**
     * @brief Processes the next buffer
     *
     * @param energy Mean power of the buffer (mean of the squared samples)
     * @param duration Duration of the buffer [s]
     * @return float Trigger in [0, 1]
     */
    float process(
      float energy,
      float duration
    )
    {
      if(!m_hasAverage){
        m_average = energy;
        m_hasAverage = true;
      }

      float trigger = 0.f;
      if(energy > NoiseFloor){
        const float jumpDb = 10.f * std::log10(energy / std::max(m_average, NoiseFloor));
        trigger = std::clamp((jumpDb - TriggerThresholdDb) / (FullTriggerDb - TriggerThresholdDb), 0.f, 1.f);
      }

      // Average independent of the buffer size
      const float keep = std::exp(-duration / AverageTimeConstant);
      m_average = m_average * keep + energy * (1.f - keep);

      return trigger;
    }


    /**
     * @brief Forgets the history, e.g. when the stream format changes
     *
     */
    void reset()
    {
      m_hasAverage = false;
    }


  private:
    float m_average{0.f};
    bool m_hasAverage{false};
  };
}
