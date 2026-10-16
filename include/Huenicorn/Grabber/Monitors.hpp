#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Huenicorn/Imaging/UV.hpp>


namespace Huenicorn::Grabber
{
  /**
   * @brief Position and size of a monitor in logical desktop coordinates
   *
   */
  struct MonitorGeometry
  {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    bool operator==(const MonitorGeometry& other) const = default;
  };


  /**
   * @brief Identifies the monitor a channel follows, persisted in the profile
   *
   */
  struct MonitorRef
  {
    MonitorGeometry geometry;
    std::string name; // Connector, such as "DP-3", empty if unknown

    bool operator==(const MonitorRef& other) const = default;
  };


  /**
   * @brief Monitor as the compositor describes it
   *
   */
  struct WaylandOutput
  {
    std::string name;
    std::string description;
    MonitorGeometry geometry;
  };


  /**
   * @brief Captured monitor
   *
   */
  struct MonitorInfo
  {
    MonitorGeometry geometry;
    int pixelWidth{0};
    int pixelHeight{0};
    std::string name;
    std::string description; // Manufacturer and model, or "Monitor N · W×H"
    bool gpu{false};
    std::string processingDetails;
    bool active{false}; // Captured right now: followed by a lamp, and not paused
    bool lost{false}; // Its stream ended while capturing, such as when it was unplugged
  };

  using Monitors = std::vector<MonitorInfo>;


  /**
   * @brief Finds the monitor a channel follows: same geometry, else same connector, else same size
   *
   * Lost monitors are never found.
   *
   * @param ref Monitor of the channel
   * @param monitors Captured monitors
   * @return std::optional<size_t> Index of the monitor, nothing if none matches
   */
  std::optional<size_t> findMonitor(
    const MonitorRef& ref,
    const Monitors& monitors
  );


  /**
   * @brief Returns what identifies a monitor in the profile
   *
   * @param monitor Captured monitor
   * @return MonitorRef Reference to store
   */
  MonitorRef monitorRef(
    const MonitorInfo& monitor
  );


  /**
   * @brief Names the monitors after the compositor outputs with the same logical geometry
   *
   * Monitors without a matching output are described by their number and pixel size.
   *
   * @param monitors Captured monitors to name
   * @param outputs Outputs of the compositor
   */
  void nameMonitors(
    Monitors& monitors,
    const std::vector<WaylandOutput>& outputs
  );


  /**
   * @brief Places monitors side by side from the left, for portals that give no position
   *
   * @param pixelSizes Width and height of each monitor
   * @return std::vector<MonitorGeometry> Geometry of each monitor
   */
  std::vector<MonitorGeometry> fallbackGeometry(
    const std::vector<std::pair<int, int>>& pixelSizes
  );


  /**
   * @brief Returns the rectangle containing every monitor
   *
   * @param monitors Captured monitors
   * @return MonitorGeometry Bounds of the desktop, empty without monitors
   */
  MonitorGeometry desktopBounds(
    const Monitors& monitors
  );


  /**
   * @brief Returns the horizontal center of a region on the whole desktop
   *
   * @param uvs Region on its monitor
   * @param monitor Geometry of the region's monitor
   * @param bounds Bounds of the desktop
   * @return float 0 at the left edge of the desktop, 1 at the right one
   */
  float desktopCenterX(
    const Imaging::UVs& uvs,
    const MonitorGeometry& monitor,
    const MonitorGeometry& bounds
  );


  /**
   * @brief Returns the subsample width of a monitor, in the same ratio as the configured one
   *
   * @param configuredWidth Configured subsample width, for the reference monitor
   * @param referencePixelWidth Pixel width of the reference (widest) monitor
   * @param monitorPixelWidth Pixel width of the monitor
   * @return int Subsample width, between 1 and the monitor's width
   */
  int subsampleWidthFor(
    unsigned configuredWidth,
    int referencePixelWidth,
    int monitorPixelWidth
  );


  /**
   * @brief Grows a region smaller than one subsample cell to one cell, inside [0, 1]
   *
   * A region moved onto a monitor with a coarser grid could otherwise collapse to nothing.
   *
   * @param uvs Region
   * @param cellWidth Width of a subsample cell, normalized
   * @param cellHeight Height of a subsample cell, normalized
   * @return Imaging::UVs Region of at least one cell
   */
  Imaging::UVs minimalRegion(
    const Imaging::UVs& uvs,
    float cellWidth,
    float cellHeight
  );


  /**
   * @brief Returns the monitor a newly activated lamp follows: the one most other lamps follow
   *
   * @param monitorsOfActiveLamps Monitor index of each active lamp
   * @param monitorCount Number of captured monitors
   * @return size_t Monitor index, the lowest on a tie, 0 without lamps
   */
  size_t defaultMonitorForNewLamp(
    const std::vector<size_t>& monitorsOfActiveLamps,
    size_t monitorCount
  );
}
