#pragma once

#include <chrono>

namespace Halomux
{
  namespace Timing
  {
    using ClockType = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double>;
    using TimePoint = ClockType::time_point;
  }
}
