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

  // Least share of a primary a wall is taken to reflect: a wall reflecting none would dim the
  // others to nothing
  constexpr float MinWallReflection = 0.05f;


  // sRGB, as colors are given, to linear light and back
  float toLinear(
    float value
  )
  {
    return value > 0.04045f ? std::pow((value + 0.055f) / 1.055f, 2.4f) : value / 12.92f;
  }


  float toSrgb(
    float value
  )
  {
    return value > 0.0031308f ? 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f : value * 12.92f;
  }
}


namespace Halomux::Core::ChannelColor
{
  Imaging::Color compensateWall(
    const Imaging::Color& color,
    const Imaging::Color& wall
  )
  {
    const glm::vec3 wallSrgb = wall.toNormalized();
    const glm::vec3 reflection{
      std::max(toLinear(wallSrgb.r), MinWallReflection),
      std::max(toLinear(wallSrgb.g), MinWallReflection),
      std::max(toLinear(wallSrgb.b), MinWallReflection)
    };

    // The primary the wall reflects least is kept, the others dimmed down to it
    const float least = std::min({reflection.r, reflection.g, reflection.b});
    const glm::vec3 colorSrgb = color.toNormalized();

    const auto channel = [&](int i){
      const float compensated = toSrgb(toLinear(colorSrgb[i]) * least / reflection[i]);
      return static_cast<Imaging::Color::ChannelDepth>(std::lround(std::clamp(compensated, 0.f, 1.f) * Imaging::Color::Max));
    };

    return Imaging::Color{channel(0), channel(1), channel(2)};
  }


  glm::vec3 step(
    Hue::Api::Channel& channel,
    const Imaging::Color& color,
    const Settings& settings,
    float soundTrigger
  )
  {
    // Its own wall first. The highlight boost still follows the color measured
    glm::vec3 xybColor = compensateWall(color, channel.wallColor.value_or(settings.wallColor)).toXYB();

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
