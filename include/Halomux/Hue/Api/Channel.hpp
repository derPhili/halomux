#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/exponential.hpp>
#include <glm/vec3.hpp>

#include <Halomux/Grabber/Monitors.hpp>
#include <Halomux/Hue/Api/Device.hpp>
#include <Halomux/Imaging/Color.hpp>
#include <Halomux/Imaging/UV.hpp>


namespace Halomux::Hue::Api
{
  // Type definitions
  struct Channel;
  using Channels = std::unordered_map<uint8_t, Channel>;

  /**
   * @brief Channel structure matching Hue streaming protocol
   *
   */
  struct ChannelStream
  {
    uint8_t id;
    float r{0.0};
    float g{0.0};
    float b{0.0};
  };
  using ChannelStreams = std::vector<ChannelStream>;

  /**
   * @brief Wrapper around Hue Channel extended with UV and Gamma control
   *
   */
  struct Channel
  {
  public:
    // Type definitions
    enum class State
    {
      Inactive,
      Active,
      PendingShutdown
    };

    /**
     * @brief Channel constructor
     *
     * @param active Whether the channel is active or not
     * @param devices List of devices driven by the channel
     * @param gammaFactor Gamma factor of the light
     * @param uvs UVs of the screen portion
     */
    Channel(
      bool active,
      const Devices &devices,
      float gammaFactor,
      const Imaging::UVs &uvs = {{0, 0}, {1, 1}}
    );


    /**
     * @brief Returns the exponent factor for gamma
     *
     * @return float Channel gamma factor
     */
    inline float gammaExponent() const
    {
      float factor = 2.f;
      float exponent = glm::pow(2.f, -gammaFactor * factor);
      return exponent;
    }

    // Setters
    /**
     * @brief Set the channel activity
     *
     * @param active Whether the channel must be active or not
     */
    void setActive(bool active);

    /**
     * @brief Sets the UVs of the channel
     *
     * @param uv UV coordinate to set
     * @param uvCorner Specified corner
     * @return UVs& Clamped UVs
     */
    Imaging::UVs &setUV(
      const Imaging::UV& uv,
      Imaging::UVCorner uvCorner
    );


    // Methods
    /**
     * @brief Call to move from pending state to inactive
     *
     */
    void acknowledgeShutdown();


    /**
     * @brief Tracks sudden brightening of the channel's region and returns the resulting boost
     *
     * A rise of the measured (unsmoothed) brightness above BoostThreshold triggers a boost,
     * proportional to the rise up to BoostFullJump. The boost starts instantly and decays
     * by decay on each call; darkening or steady content never triggers it.
     *
     * @param measuredBrightness Brightness of the region in this frame, in [0, 1]
     * @param strength Boost reached by a full jump, in [0, 1]
     * @param decay Fraction of the boost kept since the previous call, in [0, 1]
     * @param externalTrigger Additional trigger in [0, 1] (e.g. a sound transient), the stronger one wins
     * @return float Boost in [0, 1], the fraction of the remaining brightness headroom to add
     */
    float updateHighlightBoost(
      float measuredBrightness,
      float strength,
      float decay,
      float externalTrigger = 0.f
    );


    // Brightness rise ignored as noise, and rise giving the full boost
    static constexpr float BoostThreshold = 0.05f;
    static constexpr float BoostFullJump = 0.30f;

    // Attributes
    State state{State::Inactive};
    Devices devices;
    float gammaFactor{0.0};
    Imaging::UVs uvs{};
    // Monitor the region is on, nothing until it's known (profiles from before monitors per lamp)
    std::optional<Grabber::MonitorRef> monitor;

    // Index of the monitor among the captured ones, nothing if it isn't captured (not persisted)
    std::optional<size_t> monitorIndex;

    // Transient streaming state (not persisted): last xyBrightness value sent,
    // used to ease transitions when Config::transitionSmoothing() > 0.
    glm::vec3 previousXyb{Imaging::Color::XYBBlack};
    bool hasPreviousXyb{false};

    // Transient highlight boost state (see updateHighlightBoost())
    float previousMeasuredBrightness{0.f};
    bool hasPreviousMeasuredBrightness{false};
    float highlightBoost{0.f};
  };
}
