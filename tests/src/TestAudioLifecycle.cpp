#include <Halomux/Audio/AudioAnalyzer.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;


/**
 * @brief Starts and stops the sound analysis many times
 *
 * Stopping quits its Pipewire loop from another thread: the loop must not be destroyed (and
 * Pipewire not released) while the quitting call still runs. Run with AddressSanitizer.
 */
int main()
{
  for(int i = 0; i < 200; i++){
    Audio::AudioAnalyzer analyzer;
  }

  Core::Logger::log("Audio lifecycle: OK");
  return 0;
}
