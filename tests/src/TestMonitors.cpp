#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/Monitors.hpp>

using namespace Halomux;
using namespace Halomux::Grabber;


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


  MonitorInfo monitor(MonitorGeometry geometry, int pixelWidth, int pixelHeight)
  {
    MonitorInfo info;
    info.geometry = geometry;
    info.pixelWidth = pixelWidth;
    info.pixelHeight = pixelHeight;
    return info;
  }


  // The maintainer's desktop: a smaller monitor on the left, lower than the main one
  const MonitorGeometry Left{0, 249, 1920, 1080};
  const MonitorGeometry Main{1920, 0, 2560, 1440};
}


int main()
{
  const Monitors desktop{monitor(Left, 1920, 1080), monitor(Main, 2560, 1440)};

  // findMonitor: exact geometry, then connector name, then size
  expect("Exact geometry found", findMonitor({Main, "DP-3"}, desktop) == 1u);
  expect("Moved monitor found by name", findMonitor({{0, 0, 2560, 1440}, "DP-3"}, Monitors{monitor(Left, 1920, 1080), [] {auto m = monitor({1920, 0, 2560, 1440}, 2560, 1440); m.name = "DP-3"; return m;}()}) == 1u);
  expect("Unknown name found by size", findMonitor({{5000, 0, 1920, 1080}, "HDMI-A-1"}, desktop) == 0u);
  expect("Nothing matching", !findMonitor({{0, 0, 3840, 2160}, "DP-9"}, desktop).has_value());
  expect("No monitors", !findMonitor({Main, "DP-3"}, Monitors{}).has_value());

  // A monitor gone while capturing isn't followed anymore, not even by name
  {
    Monitors unplugged = desktop;
    unplugged[0].name = "DP-1";
    unplugged[0].lost = true;
    expect("Lost monitor not found by geometry", !findMonitor({Left, "DP-1"}, unplugged).has_value());
    expect("Other monitors still found", findMonitor({Main, "DP-3"}, unplugged) == 1u);
  }

  // monitorRef keeps what findMonitor needs
  {
    MonitorInfo info = monitor(Main, 2560, 1440);
    info.name = "DP-3";
    expect("Reference of a monitor", monitorRef(info) == MonitorRef{Main, "DP-3"});
  }

  // nameMonitors: compositor outputs matched by logical geometry
  {
    Monitors named = desktop;
    nameMonitors(named, {{"DP-3", "LG Electronics 27GL850 (DP-3)", Main}});
    expect("Output name taken", named[1].name == "DP-3");
    expect("Connector stripped from the description", named[1].description == "LG Electronics 27GL850");
    expect("Unmatched monitor has no name", named[0].name.empty());
    expect("Unmatched monitor described by number and size", named[0].description == "Monitor 1 · 1920×1080");
  }

  {
    // Scaled by 1.5: logical size differs from the pixel size
    Monitors scaled{monitor({0, 0, 1707, 960}, 2560, 1440)};
    nameMonitors(scaled, {{"eDP-1", "BOE 0x0BCA", {0, 0, 1707, 960}}});
    expect("Scaled monitor matched by logical geometry", scaled[0].name == "eDP-1" && scaled[0].description == "BOE 0x0BCA");
  }

  // fallbackGeometry: portals without positions, monitors side by side from the left
  {
    const auto geometries = fallbackGeometry({{2560, 1440}, {1920, 1080}});
    expect("Fallback count", geometries.size() == 2);
    expect("Fallback first", geometries.size() == 2 && geometries[0] == MonitorGeometry{0, 0, 2560, 1440});
    expect("Fallback second", geometries.size() == 2 && geometries[1] == MonitorGeometry{2560, 0, 1920, 1080});
  }

  // desktopBounds, also with a monitor left of the origin
  expect("Desktop bounds", desktopBounds(desktop) == MonitorGeometry{0, 0, 4480, 1440});
  expect("Desktop bounds with negative coordinates", desktopBounds(Monitors{monitor({-1920, 0, 1920, 1080}, 1920, 1080), monitor({0, 0, 2560, 1440}, 2560, 1440)}) == MonitorGeometry{-1920, 0, 4480, 1440});
  expect("Empty desktop bounds", desktopBounds(Monitors{}) == MonitorGeometry{});

  // desktopCenterX: the right edge of the left monitor is left of the left edge of the right one
  {
    const MonitorGeometry bounds = desktopBounds(desktop);
    const float leftLamp = desktopCenterX({{0.6f, 0.f}, {1.f, 1.f}}, Left, bounds);
    const float rightLamp = desktopCenterX({{0.f, 0.f}, {0.4f, 1.f}}, Main, bounds);
    expect("Center on the left monitor", std::abs(leftLamp - 1536.f / 4480.f) < 1e-4f);
    expect("Center on the right monitor", std::abs(rightLamp - 2432.f / 4480.f) < 1e-4f);
    expect("Left lamp left of right lamp", leftLamp < rightLamp);

    const MonitorGeometry shifted{-1920, 0, 4480, 1440};
    expect("Center with negative coordinates", std::abs(desktopCenterX({{0.f, 0.f}, {1.f, 1.f}}, {-1920, 0, 1920, 1080}, shifted) - 960.f / 4480.f) < 1e-4f);
  }

  // subsampleWidthFor: same ratio on every monitor
  expect("Subsample of a smaller monitor", subsampleWidthFor(256, 2560, 1920) == 192);
  expect("Subsample of the reference monitor", subsampleWidthFor(256, 2560, 2560) == 256);
  expect("Subsample capped to the monitor", subsampleWidthFor(5000, 2560, 1920) == 1920);
  expect("Subsample at least one pixel", subsampleWidthFor(0, 2560, 1920) == 1);
  // Beyond int: capped too, not cast out of range
  expect("Huge subsample capped", subsampleWidthFor(std::numeric_limits<unsigned>::max(), 2560, 1920) == 1920);
  expect("Huge subsample capped without reference", subsampleWidthFor(std::numeric_limits<unsigned>::max(), 0, 1920) == 1920);

  // minimalRegion: a region never collapses below one subsample cell
  {
    const auto collapsed = minimalRegion({{0.5f, 0.25f}, {0.5f, 0.25f}}, 0.1f, 0.2f);
    expect("Collapsed region gets one cell wide", std::abs((collapsed.max.x - collapsed.min.x) - 0.1f) < 1e-5f);
    expect("Collapsed region gets one cell high", std::abs((collapsed.max.y - collapsed.min.y) - 0.2f) < 1e-5f);

    const auto atEdge = minimalRegion({{1.f, 1.f}, {1.f, 1.f}}, 0.1f, 0.2f);
    expect("Region at the edge grows inwards", atEdge.max.x <= 1.f && atEdge.max.y <= 1.f && atEdge.max.x - atEdge.min.x > 0.099f && atEdge.max.y - atEdge.min.y > 0.199f);

    const Imaging::UVs large{{0.1f, 0.1f}, {0.6f, 0.9f}};
    expect("Large region unchanged", minimalRegion(large, 0.1f, 0.2f) == large);
  }

  // defaultMonitorForNewLamp: where most lamps are, the first one on a tie
  {
    const Monitors two(2);
    expect("Most followed monitor", defaultMonitorForNewLamp({1, 1, 0}, two) == 1u);
    expect("Tie goes to the first", defaultMonitorForNewLamp({0, 1}, two) == 0u);
    expect("No lamps yet", defaultMonitorForNewLamp({}, two) == 0u);
    expect("Stale index ignored", defaultMonitorForNewLamp({5, 5, 1}, two) == 1u);

    // An unplugged monitor gets no new lamp, even if most lamps still follow it
    Monitors firstLost(2);
    firstLost[0].lost = true;
    expect("Lost monitor skipped", defaultMonitorForNewLamp({0, 0}, firstLost) == 1u);
    Monitors allLost(1);
    allLost[0].lost = true;
    expect("No monitor left", !defaultMonitorForNewLamp({0}, allLost).has_value());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Monitors: OK");
  return 0;
}
