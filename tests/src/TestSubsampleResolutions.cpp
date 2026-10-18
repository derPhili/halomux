#include <algorithm>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/SubsampleResolutions.hpp>

using namespace Halomux;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }


  bool contains(const std::vector<glm::ivec2>& resolutions, glm::ivec2 resolution)
  {
    return std::ranges::find(resolutions, resolution) != resolutions.end();
  }
}


/**
 * @brief Checks the resolutions a captured image can be downscaled to
 *
 */
int main()
{
  {
    // 2560x1440: the common divisors of the width and the height are those of 160
    const auto resolutions = Grabber::subsampleResolutions({2560, 1440});
    expect("All the common divisors", resolutions.size() == 12);
    expect("Largest first", !resolutions.empty() && resolutions.front() == glm::ivec2(2560, 1440));
    expect("Smallest last", !resolutions.empty() && resolutions.back() == glm::ivec2(16, 9));
    expect("Undistorted", std::ranges::all_of(resolutions, [](glm::ivec2 r){
      return r.x * 1440 == r.y * 2560 && 2560 % r.x == 0;
    }));
  }

  {
    // The largest proper divisor is half the number
    const auto resolutions = Grabber::subsampleResolutions({1440, 720});
    expect("Half the height as divisor", contains(resolutions, {2, 1}));
    expect("Half the width as divisor", contains(Grabber::subsampleResolutions({4, 4}), {2, 2}));
  }

  {
    expect("Prime dimensions", Grabber::subsampleResolutions({7, 5}) == std::vector<glm::ivec2>{{7, 5}});
    expect("Empty image", Grabber::subsampleResolutions({0, 0}).empty());
    expect("Empty height", Grabber::subsampleResolutions({1920, 0}).empty());
    expect("Negative size", Grabber::subsampleResolutions({-4, 2}).empty());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Subsample resolutions: OK");
  return 0;
}
