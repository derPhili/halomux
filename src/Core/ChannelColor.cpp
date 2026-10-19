#include <Halomux/Core/ChannelColor.hpp>

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>
#include <glm/exponential.hpp>


namespace
{
  // Time constant of the fade between static colors [s]
  constexpr float StaticFadeTimeConstant = 0.35f;

  // Time constant of the highlight boost's release [s]
  constexpr float BoostTimeConstant = 0.25f;
}


namespace Halomux::Core::ChannelColor
{
  glm::vec3 step(
    Hue::Api::Channel& channel,
    const Imaging::Color& color,
    const Settings& settings,
    float soundTrigger
  )
  {
    glm::vec3 xybColor = color.toXYB();

    // Ease toward the newly measured color instead of snapping to it, so
    // fast-changing content doesn't produce an abrupt strobe-like cut.
    // transitionSmoothing == 0 snaps to it.
    float smoothing = settings.transitionSmoothing;
    if(!settings.syncing && !settings.previewing){
      // Fade softly between static colors, whatever the smoothing tuned for the screen
      smoothing = std::max(smoothing, std::exp(-settings.tickDuration / StaticFadeTimeConstant));
    }
    if(channel.hasPreviousXyb && smoothing > 0.f){
      xybColor = glm::mix(channel.previousXyb, xybColor, 1.f - smoothing);
    }
    channel.previousXyb = xybColor;
    channel.hasPreviousXyb = true;

    xybColor.z = glm::pow(xybColor.z, channel.gammaExponent());

    // Global dimmer, applied last so it scales what the lamp would otherwise show
    xybColor.z *= settings.brightness;

    // Flash on sudden brightening, after smoothing so it isn't eased away: the boost takes
    // the lamp part of the way to full brightness and fades with a fixed time constant
    const float decay = std::exp(-settings.tickDuration / BoostTimeConstant);

    // Static colors don't flash when switching scenes
    const float boostStrength = settings.syncing || settings.previewing ? settings.highlightBoost : 0.f;
    const float boost = channel.updateHighlightBoost(color.brightness(), boostStrength, decay, soundTrigger);
    xybColor.z += boost * (1.f - xybColor.z);

    return xybColor;
  }


  void shutDown(
    Hue::Api::Channel& channel,
    Hue::Api::ChannelStream& stream
  )
  {
    stream.r = 0;
    stream.g = 0;
    stream.b = 0;
    channel.hasPreviousXyb = false;
    channel.hasPreviousMeasuredBrightness = false;
    channel.highlightBoost = 0.f;
    channel.acknowledgeShutdown();
  }
}
