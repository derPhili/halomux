#include <Halomux/Grabber/SubsampleResolutions.hpp>

#include <numeric>


namespace Halomux::Grabber
{
  std::vector<glm::ivec2> subsampleResolutions(
    glm::ivec2 resolution
  )
  {
    if(resolution.x <= 0 || resolution.y <= 0){
      return {};
    }

    // The common divisors of both dimensions are the divisors of their greatest common divisor
    const int commonDivisor = std::gcd(resolution.x, resolution.y);

    std::vector<glm::ivec2> resolutions;
    for(int divisor = 1; divisor <= commonDivisor; divisor++){
      if(commonDivisor % divisor == 0){
        resolutions.emplace_back(resolution.x / divisor, resolution.y / divisor);
      }
    }

    return resolutions;
  }
}
