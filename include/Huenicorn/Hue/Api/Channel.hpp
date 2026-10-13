#pragma once

#include <unordered_map>
#include <vector>

#include <glm/exponential.hpp>
#include <glm/vec3.hpp>

#include <Huenicorn/Hue/Api/Device.hpp>
#include <Huenicorn/Imaging/Color.hpp>
#include <Huenicorn/Imaging/UV.hpp>


namespace Huenicorn::Hue::Api
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

    // Attributes
    State state{State::Inactive};
    Devices devices;
    float gammaFactor{0.0};
    Imaging::UVs uvs{};

    // Transient streaming state (not persisted): last xyBrightness value sent,
    // used to ease transitions when Config::transitionSmoothing() > 0.
    glm::vec3 previousXyb{Imaging::Color::XYBBlack};
    bool hasPreviousXyb{false};
  };
}
