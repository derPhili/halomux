#include <limits>
#include <cmath>

#include <Halomux/Audio/TransientDetector.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition, float value)
  {
    if(!condition){
      Core::Logger::error(name, " (got ", value, ")");
      failures++;
    }
  }


  float decibelsToPower(float decibels)
  {
    return std::pow(10.f, decibels / 10.f);
  }
}


/**
 * @brief Checks the transient detection on synthetic buffer energies (10 ms buffers)
 *
 */
int main()
{
  constexpr float BufferDuration = 0.01f;
  const float base = decibelsToPower(-30.f); // Background sound at -30 dBFS

  {
    Audio::TransientDetector detector;
    float maxTrigger = 0.f;
    for(int i = 0; i < 100; i++){
      maxTrigger = std::max(maxTrigger, detector.process(base, BufferDuration));
    }
    expect("Steady sound never triggers", maxTrigger == 0.f, maxTrigger);

    float trigger = detector.process(base * decibelsToPower(20.f), BufferDuration);
    expect("+20 dB jump gives a full trigger", trigger == 1.f, trigger);

    // The average catches up with steady loud sound
    for(int i = 0; i < 100; i++){
      trigger = detector.process(base * decibelsToPower(20.f), BufferDuration);
    }
    expect("Loud steady sound stops triggering", trigger == 0.f, trigger);
  }

  {
    Audio::TransientDetector detector;
    for(int i = 0; i < 100; i++){
      detector.process(base, BufferDuration);
    }

    // Half way between the threshold and a full trigger
    const float jumpDb = (Audio::TransientDetector::TriggerThresholdDb + Audio::TransientDetector::FullTriggerDb) / 2.f;
    const float trigger = detector.process(base * decibelsToPower(jumpDb), BufferDuration);
    expect("Partial jump gives a partial trigger", std::abs(trigger - 0.5f) < 0.02f, trigger);

    const float small = detector.process(base * decibelsToPower(3.f), BufferDuration);
    expect("Jump below the threshold is ignored", small == 0.f, small);
  }

  {
    Audio::TransientDetector detector;
    for(int i = 0; i < 100; i++){
      detector.process(0.f, BufferDuration);
    }

    // From silence, but still below the noise floor
    const float quiet = detector.process(Audio::TransientDetector::NoiseFloor / 2.f, BufferDuration);
    expect("Sound under the noise floor never triggers", quiet == 0.f, quiet);

    const float loud = detector.process(base, BufferDuration);
    expect("Sound after silence triggers", loud == 1.f, loud);
  }

  {
    // A buffer with an invalid sample (NaN, infinity) is ignored: the detection carries on
    Audio::TransientDetector detector;
    for(int i = 0; i < 100; i++){
      detector.process(base, BufferDuration);
    }
    const float invalid = detector.process(std::numeric_limits<float>::quiet_NaN(), BufferDuration);
    detector.process(std::numeric_limits<float>::infinity(), BufferDuration);
    expect("Invalid buffer doesn't trigger", invalid == 0.f, invalid);
    const float steady = detector.process(base, BufferDuration);
    expect("Steady sound after an invalid buffer doesn't trigger", steady == 0.f, steady);

    const float loud = detector.process(base * 100.f, BufferDuration);
    expect("Detection carries on after an invalid buffer", loud == 1.f, loud);
  }

  if(failures > 0){
    Core::Logger::error(failures, " test(s) failed");
    return 1;
  }

  Core::Logger::log("All tests passed");
  return 0;
}
