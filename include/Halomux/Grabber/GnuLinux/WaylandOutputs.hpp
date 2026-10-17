#pragma once

#include <vector>

#include <Halomux/Grabber/Monitors.hpp>


namespace Halomux::Grabber
{
  /**
   * @brief Lists the monitors of the Wayland compositor, with their names and logical geometry
   *
   * Uses wl_output (version 4 for the name and description) and xdg-output (logical position
   * and size, the coordinates the screencast portal uses).
   *
   * @return std::vector<WaylandOutput> Monitors, empty without a Wayland connection or xdg-output
   */
  std::vector<WaylandOutput> listWaylandOutputs();
}
