#include <cmath>
#include <limits>

#include <Halomux/Core/ChannelStore.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
using Hue::Api::Channel;


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


  Grabber::MonitorInfo monitor(Grabber::MonitorGeometry geometry, const char* name)
  {
    Grabber::MonitorInfo info;
    info.geometry = geometry;
    info.pixelWidth = geometry.width;
    info.pixelHeight = geometry.height;
    info.name = name;
    return info;
  }


  // A small monitor on the left, the main one on the right
  const Grabber::Monitors Desktop{monitor({0, 249, 1920, 1080}, "DP-1"), monitor({1920, 0, 2560, 1440}, "DP-3")};


  Hue::Api::Channels lamps()
  {
    Hue::Api::Channels channels;
    channels.emplace(0, Channel{true, {}, 0.f, {{0.f, 0.f}, {0.5f, 1.f}}});
    channels.emplace(1, Channel{true, {}, 0.f, {{0.5f, 0.f}, {1.f, 1.f}}});
    channels.emplace(2, Channel{false, {}, 0.f});
    return channels;
  }
}


int main()
{
  // The lamps belong to an area: saving reads both together, never one area's lamps with another's id
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop, "pc");
    auto saved = store.areaSnapshot();
    expect("Lamps read with their area", saved.areaId == "pc" && saved.channels.size() == 3);

    {
      auto access = store.access();
      Hue::Api::Channels other;
      other.emplace(5, Channel{true, {}, 0.f});
      access.replace("garage", std::move(other), Desktop);
    }
    saved = store.areaSnapshot();
    expect("Area and lamps replaced together", saved.areaId == "garage" && saved.channels.size() == 1 && saved.channels.contains(5));
  }

  // Profiles from before monitors per lamp: active lamps follow the first monitor
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    const auto channels = store.snapshot();
    expect("Active lamp gets the first monitor", channels.at(0).monitorIndex == 0u && channels.at(0).monitor->name == "DP-1");
    expect("Inactive lamp gets no monitor yet", !channels.at(2).monitor.has_value());
  }

  // A lamp activated for the first time follows the monitor most other lamps follow
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    expect("Lamp moved", store.setMonitor(0, 1, {{0.f, 0.f}, {0.5f, 1.f}}, 256).has_value());
    expect("Lamp moved too", store.setMonitor(1, 1, {{0.5f, 0.f}, {1.f, 1.f}}, 256).has_value());
    expect("Activation of a known lamp", store.setActivity(2, true));
    expect("New lamp on the busiest monitor", store.snapshot().at(2).monitorIndex == 1u);
    expect("Unknown lamp not activated", !store.setActivity(9, true));
  }

  // Deactivation streams black once before the lamp is released
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    store.setActivity(0, false);
    expect("Deactivated lamp pending shutdown", store.snapshot().at(0).state == Channel::State::PendingShutdown);
  }

  // Corners stay inside the screen and never cross the opposite one
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    const auto uvs = store.setUV(0, {0.8f, -0.5f}, Imaging::UVCorner::TopLeft);
    expect("Corner set", uvs.has_value());
    expect("Corner clamped to the screen", uvs && uvs->min.y == 0.f);
    expect("Opposite corner pushed along", uvs && uvs->max.x >= uvs->min.x);
    expect("Unknown lamp's corner", !store.setUV(9, {0.f, 0.f}, Imaging::UVCorner::TopLeft).has_value());
  }

  // Unplugged monitors take no lamp
  {
    Core::ChannelStore store;
    Grabber::Monitors firstLost = Desktop;
    firstLost[0].lost = true;
    store.replace(lamps(), firstLost);
    store.setActivity(2, true);
    expect("New lamp avoids the unplugged monitor", store.snapshot().at(2).monitorIndex == 1u);
    expect("Lamp not moved onto an unplugged monitor", !store.setMonitor(1, 0, {{0.f, 0.f}, {1.f, 1.f}}, 256).has_value());
  }

  // Moving a region onto a monitor
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    const auto uvs = store.setMonitor(0, 1, {{1.2f, 0.4f}, {-0.1f, 0.4f}}, 256);
    expect("Region moved", uvs.has_value());
    expect("Region clamped and ordered", uvs && uvs->min.x == 0.f && uvs->max.x == 1.f);
    expect("Region at least one subsample cell high", uvs && uvs->max.y - uvs->min.y >= 1.f / 144.f - 1e-6f);
    expect("Region follows the new monitor", store.snapshot().at(0).monitor->name == "DP-3");
    expect("Unknown monitor rejected", !store.setMonitor(0, 5, {{0.f, 0.f}, {1.f, 1.f}}, 256).has_value());
    expect("Unknown lamp rejected", !store.setMonitor(9, 0, {{0.f, 0.f}, {1.f, 1.f}}, 256).has_value());
  }

  // Gamma
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    expect("Gamma set", store.setGammaFactor(0, 0.5f) && store.snapshot().at(0).gammaFactor == 0.5f);
    expect("Invalid gamma rejected", !store.setGammaFactor(0, std::numeric_limits<float>::quiet_NaN()));
    expect("Unknown lamp's gamma rejected", !store.setGammaFactor(9, 0.f));
  }

  // Scenes are spread over the lamps from left to right on the whole desktop
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    store.setMonitor(1, 0, {{0.f, 0.f}, {0.2f, 1.f}}, 256);
    store.setMonitor(0, 1, {{0.f, 0.f}, {0.5f, 1.f}}, 256);
    const auto centers = store.activeLampCenters();
    float left = -1.f;
    float right = -1.f;
    for(const auto& [channelId, center] : centers){
      (channelId == 1 ? left : right) = center;
    }
    expect("Only active lamps spread", centers.size() == 2);
    expect("Lamp on the left monitor is left", left >= 0.f && right >= 0.f && left < right);
  }

  // A monitor lost while capturing
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    Grabber::Monitors unplugged = Desktop;
    unplugged[0].lost = true;
    expect("Lost monitor noticed", store.monitorsLostChanged(unplugged));
    expect("Unchanged monitors", !store.monitorsLostChanged(Desktop));
    store.access().resolveMonitors(unplugged);
    expect("Lamp of the lost monitor goes dark", !store.snapshot().at(0).monitorIndex.has_value());
  }

  // Replacing the channels drops the former streams
  {
    Core::ChannelStore store;
    store.replace(lamps(), Desktop);
    store.access().streams.push_back({.id = 0});
    store.replace(lamps(), Desktop);
    expect("Streams cleared", store.access().streams.empty());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Channel store: OK");
  return 0;
}
