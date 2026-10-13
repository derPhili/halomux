#pragma once

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>


namespace Huenicorn::Imaging
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

    using GamutCoordinates = std::array<glm::vec2, 3>;

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
     * @brief Returns a XY conversion of RGB color
     * 
     * @param gamutCoordinates Boundaries of the gammut
     * @return glm::vec2 XY color coordinates
     */
    glm::vec3 toXYB(
      /*const GamutCoordinates& gamutCoordinates*/
    ) const
    {
      // Following https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d
      glm::vec3 normalizedRgb = this->toNormalized();

      // Apply gamma
      for(int i = 0; i < normalizedRgb.length(); i++){
        auto& channel = normalizedRgb[i];
        channel = (channel > 0.04045f) ? pow((channel + 0.055f) / (1.0f + 0.055f), 2.4f) : (channel / 12.92f);
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

      // Checking xy boundaries
      //unsigned (void)gamutCoordinates;
      /*
      if(!_xyInGamut(xy, gamutCoordinates)){
        // ToDo implement at some point
        // This cas has not been observed yet
      }
      */
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


  private:
    // Private methods
    /**
     * @brief Computes a sign check on boundaries
     * 
     * @param a Gammut vertex a
     * @param b Gammut vertex b
     * @param c Gammut vertex c
     * @return float Signed value for boundary check
     */
    static inline float _sign(
      const glm::vec2& a,
      const glm::vec2& b,
      const glm::vec2& c
    )
    {
      return (a.x - c.x) * (b.y - c.y) - (b.x - c.x) * (a.y - c.y);
    }


    /**
     * @brief Returns whether the XY color coordinates fits in gammut
     * 
     * @param xy 
     * @param gamutCoordinates 
     * @return true XY color fits in gammut boundaries
     * @return false XY color doesn't fit in gammut boundaries
     */
    inline static bool _xyInGamut(
      const glm::vec2& xy,
      const GamutCoordinates& gamutCoordinates
    )
    {
      bool has_neg, has_pos;

      const auto& a = gamutCoordinates.at(0);
      const auto& b = gamutCoordinates.at(1);
      const auto& c = gamutCoordinates.at(2);

      float d1 = _sign(xy, a, b);
      float d2 = _sign(xy, b, c);
      float d3 = _sign(xy, c, a);

      has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
      has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

      return !(has_neg && has_pos);
    }

  public:
    // Attributes
    ChannelDepth m_r;
    ChannelDepth m_g;
    ChannelDepth m_b;
  };
}
