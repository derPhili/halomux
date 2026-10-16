#pragma once


namespace Huenicorn::Core
{
  /**
   * @brief State of the screen selection started from the light manager
   *
   */
  enum class MonitorSelection
  {
    Idle,
    Pending,
    Cancelled
  };
}
