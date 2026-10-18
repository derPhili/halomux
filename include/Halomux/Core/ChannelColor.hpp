#pragma once

#include <glm/vec3.hpp>

#include <Halomux/Hue/Api/Channel.hpp>
#include <Halomux/Imaging/Color.hpp>


namespace Halomux::Core::ChannelColor
{
  /**
   * @brief What shapes the colors of an update, the same for every lamp
   *
   */
  struct Settings
  {
    // Time between two updates [s]
    float tickDuration{0.02f};
    // Fraction of the previous color kept on each update, in [0, 1)
    float transitionSmoothing{0.f};
    // Global dimmer factor, in [0, 1]
    float brightness{1.f};
    // Boost reached by a full brightness jump, in [0, 1]
    float highlightBoost{0.f};
    // The lamps follow the screen, rather than showing the static colors
    bool syncing{true};
    // The preview sequence replaces the screen content
    bool previewing{false};
  };


  /**
   * @brief Returns the color to stream for a lamp, from the color it should show
   *
   * Eases toward the color, applies the lamp's gamma, the global dimmer and the highlight boost,
   * and updates the lamp's easing and highlight state.
   *
   * @param channel Lamp to update
   * @param color Color the lamp should show: captured, previewed or static
   * @param settings Settings of this update
   * @param soundTrigger Sound transient reaching this lamp, in [0, 1]
   * @return glm::vec3 Color to stream, in XYB
   */
  glm::vec3 step(
    Hue::Api::Channel& channel,
    const Imaging::Color& color,
    const Settings& settings,
    float soundTrigger
  );


  /**
   * @brief Streams black for a lamp being switched off, forgets its easing and marks it inactive
   *
   * @param channel Lamp in the PendingShutdown state
   * @param stream Its stream
   */
  void shutDown(
    Hue::Api::Channel& channel,
    Hue::Api::ChannelStream& stream
  );
}
