#include <cmath>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Hue/Api/Channel.hpp>

using namespace Huenicorn;


namespace
{
  int failures = 0;

  void expectNear(const char* name, float value, float expected)
  {
    if(std::abs(value - expected) > 1e-4f){
      Core::Logger::error(name, ": ", value, " instead of ", expected);
      failures++;
    }
  }
}


/**
 * @brief Checks the highlight boost envelope of a channel
 *
 */
int main()
{
  const float decay = std::exp(-0.02f / 0.25f); // 50 Hz, 250 ms time constant

  {
    Hue::Api::Channel channel(true, {}, 0.f);
    expectNear("First frame never triggers", channel.updateHighlightBoost(0.9f, 1.f, decay), 0.f);
    expectNear("Small rise is ignored", channel.updateHighlightBoost(0.94f, 1.f, decay), 0.f);
    expectNear("Darkening never triggers", channel.updateHighlightBoost(0.1f, 1.f, decay), 0.f);
  }

  {
    Hue::Api::Channel channel(true, {}, 0.f);
    channel.updateHighlightBoost(0.1f, 0.6f, decay);

    // Full jump: boost = strength, instantly
    expectNear("Full jump", channel.updateHighlightBoost(0.5f, 0.6f, decay), 0.6f);

    // Steady content: exponential release
    expectNear("Release after one tick", channel.updateHighlightBoost(0.5f, 0.6f, decay), 0.6f * decay);
    float boost = 0.f;
    for(int i = 0; i < 49; i++){
      boost = channel.updateHighlightBoost(0.5f, 0.6f, decay);
    }
    expectNear("Release after one second", boost, 0.6f * std::pow(decay, 50.f));
  }

  {
    Hue::Api::Channel channel(true, {}, 0.f);
    channel.updateHighlightBoost(0.2f, 1.f, decay);

    // Half way between threshold and full jump
    const float rise = (Hue::Api::Channel::BoostThreshold + Hue::Api::Channel::BoostFullJump) / 2.f;
    expectNear("Partial jump", channel.updateHighlightBoost(0.2f + rise, 1.f, decay), 0.5f);
  }

  {
    Hue::Api::Channel channel(true, {}, 0.f);
    channel.updateHighlightBoost(0.5f, 0.8f, decay);

    // Sound trigger on steady picture, and the stronger trigger wins
    expectNear("External trigger", channel.updateHighlightBoost(0.5f, 0.8f, decay, 0.5f), 0.4f);
    expectNear("Visual trigger stronger than external", channel.updateHighlightBoost(0.9f, 0.8f, decay, 0.25f), 0.8f);
  }

  {
    Hue::Api::Channel channel(true, {}, 0.f);
    channel.updateHighlightBoost(0.1f, 0.f, decay);
    expectNear("Disabled boost", channel.updateHighlightBoost(0.9f, 0.f, decay), 0.f);
  }

  if(failures > 0){
    Core::Logger::error(failures, " test(s) failed");
    return 1;
  }

  Core::Logger::log("All tests passed");
  return 0;
}
