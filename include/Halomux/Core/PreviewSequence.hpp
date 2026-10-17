#pragma once

#include <array>
#include <cmath>

#include <Halomux/Imaging/Color.hpp>


namespace Halomux::Core::PreviewSequence
{
  /**
   * Synthetic content shown on the lamps instead of the screen, to try out the settings.
   * It goes through the same processing as captured frames (smoothing, brightness, highlight
   * boost, gamma):
   *
   *  [ 0,  4[ Hard cuts between saturated colors, every second    -> transition smoothing
   *  [ 4,  7.5[ Dark scene with white flashes: left, right, both  -> highlight boost, per lamp
   *  [ 7.5, 10.5[ Slow hue rotation, offset between the lamps     -> how smoothly lamps follow
   *  [10.5, 12[ Black, then a cut to white                        -> brightness
   */

  // Total duration [s]
  inline constexpr float Duration = 12.f;

  // Duration of a flash [s]
  inline constexpr float FlashDuration = 0.12f;


  /**
   * @brief Converts a hue at full saturation and value to a color
   *
   * @param hue Hue [degrees]
   * @return Imaging::Color Color
   */
  inline Imaging::Color fromHue(float hue)
  {
    hue = std::fmod(std::fmod(hue, 360.f) + 360.f, 360.f);
    const float sector = hue / 60.f;
    const float rising = sector - std::floor(sector);

    auto channel = [](float value){
      return static_cast<Imaging::Color::ChannelDepth>(std::lround(value * 255.f));
    };

    switch(static_cast<int>(sector)){
      case 0: return {255, channel(rising), 0};
      case 1: return {channel(1.f - rising), 255, 0};
      case 2: return {0, 255, channel(rising)};
      case 3: return {0, channel(1.f - rising), 255};
      case 4: return {channel(rising), 0, 255};
      default: return {255, 0, channel(1.f - rising)};
    }
  }


  /**
   * @brief Returns the content of a lamp at a time of the sequence
   *
   * @param time Time since the start of the sequence [s]
   * @param pan Horizontal position of the lamp's region, 0 = left, 1 = right
   * @return Imaging::Color Color the lamp's region would have
   */
  inline Imaging::Color colorAt(float time, float pan)
  {
    if(time < 4.f){
      static const std::array<Imaging::Color, 4> cuts = {
        Imaging::Color{255, 40, 40},  // Red
        Imaging::Color{40, 80, 255},  // Blue
        Imaging::Color{40, 220, 90},  // Green
        Imaging::Color{255, 170, 40}  // Amber
      };

      return cuts[static_cast<size_t>(time) % cuts.size()];
    }

    if(time < 7.5f){
      const bool left = pan < 0.5f;
      const auto flashing = [time](float start){
        return time >= start && time < start + FlashDuration;
      };

      if((flashing(4.5f) && left) || (flashing(5.5f) && !left) || flashing(6.5f)){
        return {255, 255, 255};
      }

      return {25, 25, 30};
    }

    if(time < 10.5f){
      return fromHue((time - 7.5f) / 3.f * 360.f + pan * 90.f);
    }

    return time < 11.25f ? Imaging::Color{0, 0, 0} : Imaging::Color{255, 255, 255};
  }
}
