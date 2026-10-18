#pragma once

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>


namespace Halomux::Imaging
{
  /**
   * @brief Color data structure providing conversion and manipulation methods
   * 
   */
  class Color
  {
  public:
    using ChannelDepth = uint8_t;
    static constexpr float Max = static_cast<float>(std::numeric_limits<ChannelDepth>().max());

    inline static constexpr glm::vec3 XYBBlack{0.315f, 0.3312f, 0.f};

    /**
     * @brief Color constructor
     * 
     * @param r Red channel
     * @param g Green channel
     * @param b Blue channel
     */
    Color(
      ChannelDepth r = 0,
      ChannelDepth g = 0,
      ChannelDepth b = 0
    ):
    m_r(r),
    m_g(g),
    m_b(b)
    {}

    /**
     * @brief Equality comparison operator
     * 
     * @param other Other color to compare
     * @return true Other color is equal
     * @return false Other color is not equal
     */
    bool operator==(const Color& other) const
    {
      return  m_r == other.m_r &&
              m_g == other.m_g &&
              m_b == other.m_b;
    }

    /**
     * @brief Inequality comparison operator
     * 
     * @param other Other color to compare
     * @return true Other color is not equal
     * @return false Other color is equal
     */
    bool operator!=(const Color& other) const
    {
      return  !(*this == other);
    }


    /**
     * @brief Parses a color in the #rrggbb notation
     *
     * @param hex Color notation
     * @return std::optional<Color> Color, empty if the notation is invalid
     */
    static std::optional<Color> fromHex(
      std::string_view hex
    )
    {
      if(hex.size() != 7 || hex.front() != '#'){
        return std::nullopt;
      }

      std::array<ChannelDepth, 3> channels;
      for(size_t i = 0; i < channels.size(); i++){
        const char* begin = hex.data() + 1 + i * 2;
        const auto [end, error] = std::from_chars(begin, begin + 2, channels[i], 16);
        if(error != std::errc() || end != begin + 2){
          return std::nullopt;
        }
      }

      return Color{channels[0], channels[1], channels[2]};
    }


    /**
     * @brief Returns the color in the #rrggbb notation
     *
     * @return std::string Color notation
     */
    std::string toHex() const
    {
      std::array<char, 8> hex;
      std::snprintf(hex.data(), hex.size(), "#%02x%02x%02x", m_r, m_g, m_b);
      return hex.data();
    }


    /**
     * @brief Returns a rgb value in normalized 0-1 floating range
     * 
     * @return glm::vec3 normalized color
     */
    glm::vec3 toNormalized() const
    {
      return glm::vec3(
        m_r / Color::Max,
        m_g / Color::Max,
        m_b / Color::Max
      );
    }


    /**
     * @brief Returns the CIE xy chromaticity and the brightness of the color
     *
     * Colors outside a lamp's gamut are left to the lamp, which shows the closest one it can.
     *
     * @return glm::vec3 x, y and brightness
     */
    glm::vec3 toXYB() const
    {
      // Following https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d
      glm::vec3 normalizedRgb = this->toNormalized();

      // Apply gamma
      for(int i = 0; i < normalizedRgb.length(); i++){
        auto& channel = normalizedRgb[i];
        channel = (channel > 0.04045f) ? std::pow((channel + 0.055f) / (1.0f + 0.055f), 2.4f) : (channel / 12.92f);
      }

      // Apply some magic "Wide RGB D65 conversion formula"
      float& r = normalizedRgb.r;
      float& g = normalizedRgb.g;
      float& b = normalizedRgb.b;

      float X = r * 0.649926f + g * 0.103455f + b * 0.197109f;
      float Y = r * 0.234327f + g * 0.743075f + b * 0.022598f;
      float Z = r * 0.000000f + g * 0.053077f + b * 1.035763f;

      float sum = X + Y + Z;

      // White coordinates to be neutral in case of black (skip dividing by zero)
      glm::vec3 xyb = XYBBlack;

      if(sum != 0.f){
        xyb[0] = X / sum;
        xyb[1] = Y / sum;
        xyb[2] = brightness();
      }

      return xyb;
    }


    /**
     * @brief Returns the ponderated brightness of RGB color
     * 
     * @return float Color brightness
     */
    float brightness() const
    {
      return (m_r * 0.3f + m_g * 0.59f + m_b * 0.11f) / Color::Max;
    }


  public:
    // Attributes
    ChannelDepth m_r;
    ChannelDepth m_g;
    ChannelDepth m_b;
  };
}
