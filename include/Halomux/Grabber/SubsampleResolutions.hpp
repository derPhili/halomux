#pragma once

#include <vector>

#include <glm/vec2.hpp>


namespace Halomux::Grabber
{
  /**
   * @brief Lists the resolutions a captured image can be downscaled to without distorting it
   *
   * Each divides both dimensions by the same integer, a common divisor of the width and the height.
   *
   * @param resolution Resolution of the captured image
   * @return std::vector<glm::ivec2> Resolutions from the largest (the image's own) to the smallest, empty for an empty image
   */
  std::vector<glm::ivec2> subsampleResolutions(
    glm::ivec2 resolution
  );
}
