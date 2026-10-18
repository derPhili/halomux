#include <Halomux/Grabber/Monitors.hpp>

#include <algorithm>
#include <cmath>


namespace Halomux::Grabber
{
  std::optional<size_t> findMonitor(
    const MonitorRef& ref,
    const Monitors& monitors
  )
  {
    const auto find = [&](auto matches) -> std::optional<size_t> {
      for(size_t i = 0; i < monitors.size(); i++){
        if(!monitors[i].lost && matches(monitors[i])){
          return i;
        }
      }
      return std::nullopt;
    };

    if(auto index = find([&](const MonitorInfo& monitor){return monitor.geometry == ref.geometry;})){
      return index;
    }

    if(!ref.name.empty()){
      if(auto index = find([&](const MonitorInfo& monitor){return monitor.name == ref.name;})){
        return index;
      }
    }

    return find([&](const MonitorInfo& monitor){
      return monitor.geometry.width == ref.geometry.width && monitor.geometry.height == ref.geometry.height;
    });
  }


  MonitorRef monitorRef(
    const MonitorInfo& monitor
  )
  {
    return {monitor.geometry, monitor.name};
  }


  void nameMonitors(
    Monitors& monitors,
    const std::vector<WaylandOutput>& outputs
  )
  {
    for(size_t i = 0; i < monitors.size(); i++){
      MonitorInfo& monitor = monitors[i];

      auto output = std::find_if(outputs.begin(), outputs.end(), [&](const WaylandOutput& candidate){
        return candidate.geometry == monitor.geometry;
      });

      if(output == outputs.end()){
        monitor.name.clear();
        monitor.description = "Monitor " + std::to_string(i + 1) + " · " + std::to_string(monitor.pixelWidth) + "×" + std::to_string(monitor.pixelHeight);
        continue;
      }

      monitor.name = output->name;
      monitor.description = output->description;

      // KWin appends the connector, already shown as the name
      const std::string suffix = " (" + output->name + ")";
      if(!output->name.empty() && monitor.description.ends_with(suffix)){
        monitor.description.resize(monitor.description.size() - suffix.size());
      }

      if(monitor.description.empty()){
        monitor.description = output->name;
      }
    }
  }


  std::vector<MonitorGeometry> fallbackGeometry(
    const std::vector<std::pair<int, int>>& pixelSizes
  )
  {
    std::vector<MonitorGeometry> geometries;
    int x = 0;
    for(const auto& [width, height] : pixelSizes){
      geometries.push_back({x, 0, width, height});
      x += width;
    }

    return geometries;
  }


  MonitorGeometry desktopBounds(
    const Monitors& monitors
  )
  {
    if(monitors.empty()){
      return {};
    }

    int left = monitors.front().geometry.x;
    int top = monitors.front().geometry.y;
    int right = left + monitors.front().geometry.width;
    int bottom = top + monitors.front().geometry.height;

    for(const auto& monitor : monitors){
      left = std::min(left, monitor.geometry.x);
      top = std::min(top, monitor.geometry.y);
      right = std::max(right, monitor.geometry.x + monitor.geometry.width);
      bottom = std::max(bottom, monitor.geometry.y + monitor.geometry.height);
    }

    return {left, top, right - left, bottom - top};
  }


  float desktopCenterX(
    const Imaging::UVs& uvs,
    const MonitorGeometry& monitor,
    const MonitorGeometry& bounds
  )
  {
    if(bounds.width <= 0){
      return 0.5f;
    }

    const float centerOnMonitor = (uvs.min.x + uvs.max.x) / 2.f;
    const float centerOnDesktop = static_cast<float>(monitor.x) + centerOnMonitor * static_cast<float>(monitor.width);
    return std::clamp((centerOnDesktop - static_cast<float>(bounds.x)) / static_cast<float>(bounds.width), 0.f, 1.f);
  }


  int subsampleWidthFor(
    unsigned configuredWidth,
    int referencePixelWidth,
    int monitorPixelWidth
  )
  {
    if(monitorPixelWidth <= 0){
      return 1;
    }

    if(referencePixelWidth <= 0){
      return std::clamp(static_cast<int>(configuredWidth), 1, monitorPixelWidth);
    }

    const double width = std::round(static_cast<double>(configuredWidth) * monitorPixelWidth / referencePixelWidth);
    return std::clamp(static_cast<int>(width), 1, monitorPixelWidth);
  }


  Imaging::UVs minimalRegion(
    const Imaging::UVs& uvs,
    float cellWidth,
    float cellHeight
  )
  {
    Imaging::UVs region = uvs;

    const auto grow = [](float& min, float& max, float cell){
      cell = std::min(cell, 1.f);
      if(max - min >= cell){
        return;
      }

      // Grows towards the inside when at the edge
      max = std::min(min + cell, 1.f);
      min = max - cell;
    };

    grow(region.min.x, region.max.x, cellWidth);
    grow(region.min.y, region.max.y, cellHeight);
    return region;
  }


  std::optional<size_t> defaultMonitorForNewLamp(
    const std::vector<size_t>& monitorsOfActiveLamps,
    const Monitors& monitors
  )
  {
    std::vector<size_t> counts(monitors.size(), 0);
    for(size_t monitor : monitorsOfActiveLamps){
      if(monitor < monitors.size()){
        counts[monitor]++;
      }
    }

    std::optional<size_t> best;
    for(size_t i = 0; i < counts.size(); i++){
      if(!monitors[i].lost && (!best.has_value() || counts[i] > counts[*best])){
        best = i;
      }
    }

    return best;
  }
}
