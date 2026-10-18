#include <Halomux/Core/ChannelStore.hpp>

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Core
{
  ChannelStore::Access::Access(
    ChannelStore& store
  ):
  channels(store.m_channels),
  streams(store.m_streams),
  monitors(store.m_monitors),
  m_lock(store.m_mutex),
  m_store(store)
  {}


  void ChannelStore::Access::resolveMonitors(
    Grabber::Monitors newMonitors
  )
  {
    m_store._resolveMonitors(std::move(newMonitors));
  }


  float ChannelStore::Access::desktopCenterX(
    const Hue::Api::Channel& channel
  ) const
  {
    return _desktopCenterX(channel, monitors);
  }


  ChannelStore::Access ChannelStore::access()
  {
    return Access(*this);
  }


  void ChannelStore::replace(
    Hue::Api::Channels channels,
    Grabber::Monitors monitors,
    std::optional<std::string> areaId
  )
  {
    std::lock_guard lock(m_mutex);
    _replace(std::move(areaId), std::move(channels), std::move(monitors));
  }


  void ChannelStore::Access::replace(
    std::string areaId,
    Hue::Api::Channels channels,
    Grabber::Monitors monitors
  )
  {
    m_store._replace(std::move(areaId), std::move(channels), std::move(monitors));
  }


  void ChannelStore::_replace(
    std::optional<std::string> areaId,
    Hue::Api::Channels channels,
    Grabber::Monitors monitors
  )
  {
    m_areaId = std::move(areaId);
    m_channels = std::move(channels);
    m_streams.clear();
    _resolveMonitors(std::move(monitors));
  }


  bool ChannelStore::monitorsLostChanged(
    const Grabber::Monitors& monitors
  ) const
  {
    std::lock_guard lock(m_mutex);
    if(monitors.size() != m_monitors.size()){
      return true;
    }

    for(size_t i = 0; i < monitors.size(); i++){
      if(monitors[i].lost != m_monitors[i].lost){
        return true;
      }
    }

    return false;
  }


  bool ChannelStore::setActivity(
    uint8_t channelId,
    bool active
  )
  {
    std::lock_guard lock(m_mutex);
    auto channelIt = m_channels.find(channelId);
    if(channelIt == m_channels.end()){
      return false;
    }

    auto& channel = channelIt->second;

    // A lamp set up for the first time follows the monitor most other lamps follow
    if(active && !channel.monitor.has_value() && !m_monitors.empty()){
      std::vector<size_t> monitorsOfActiveLamps;
      for(const auto& [otherId, other] : m_channels){
        if(other.state == Hue::Api::Channel::State::Active && other.monitorIndex.has_value()){
          monitorsOfActiveLamps.push_back(*other.monitorIndex);
        }
      }

      const size_t monitor = Grabber::defaultMonitorForNewLamp(monitorsOfActiveLamps, m_monitors.size());
      channel.monitor = Grabber::monitorRef(m_monitors[monitor]);
      channel.monitorIndex = monitor;
    }

    channel.setActive(active);
    return true;
  }


  std::optional<Imaging::UVs> ChannelStore::setUV(
    uint8_t channelId,
    const Imaging::UV& uv,
    Imaging::UVCorner corner
  )
  {
    std::lock_guard lock(m_mutex);
    auto channelIt = m_channels.find(channelId);
    if(channelIt == m_channels.end()){
      return std::nullopt;
    }

    return channelIt->second.setUV(uv, corner);
  }


  std::optional<Imaging::UVs> ChannelStore::setMonitor(
    uint8_t channelId,
    size_t monitor,
    const Imaging::UVs& uvs,
    unsigned configuredSubsampleWidth
  )
  {
    std::lock_guard lock(m_mutex);
    auto channelIt = m_channels.find(channelId);
    if(channelIt == m_channels.end() || monitor >= m_monitors.size()){
      return std::nullopt;
    }

    const auto clamp = [](const Imaging::UV& uv){
      return Imaging::UV{std::clamp(uv.x, 0.f, 1.f), std::clamp(uv.y, 0.f, 1.f)};
    };

    const Imaging::UV a = clamp(uvs.min);
    const Imaging::UV b = clamp(uvs.max);

    // At least one cell of the monitor's subsample, which the region is averaged on
    const auto& target = m_monitors[monitor];
    int widestPixelWidth = 0;
    for(const auto& captured : m_monitors){
      widestPixelWidth = std::max(widestPixelWidth, captured.pixelWidth);
    }
    const int subsampleWidth = Grabber::subsampleWidthFor(configuredSubsampleWidth, widestPixelWidth, target.pixelWidth);
    const int subsampleHeight = std::max(1, target.pixelWidth > 0 ? target.pixelHeight * subsampleWidth / target.pixelWidth : 1);

    auto& channel = channelIt->second;
    channel.uvs = Grabber::minimalRegion({glm::min(a, b), glm::max(a, b)}, 1.f / static_cast<float>(subsampleWidth), 1.f / static_cast<float>(subsampleHeight));
    channel.monitor = Grabber::monitorRef(target);
    channel.monitorIndex = monitor;

    return channel.uvs;
  }


  bool ChannelStore::setGammaFactor(
    uint8_t channelId,
    float gammaFactor
  )
  {
    if(!std::isfinite(gammaFactor)){
      return false;
    }

    std::lock_guard lock(m_mutex);
    auto channelIt = m_channels.find(channelId);
    if(channelIt == m_channels.end()){
      return false;
    }

    channelIt->second.gammaFactor = gammaFactor;
    return true;
  }


  Hue::Api::Channels ChannelStore::snapshot() const
  {
    std::lock_guard lock(m_mutex);
    return m_channels;
  }


  ChannelStore::AreaSnapshot ChannelStore::areaSnapshot() const
  {
    std::lock_guard lock(m_mutex);
    return {.areaId = m_areaId, .channels = m_channels};
  }


  std::vector<std::pair<uint8_t, float>> ChannelStore::activeLampCenters() const
  {
    std::lock_guard lock(m_mutex);

    std::vector<std::pair<uint8_t, float>> centers;
    for(const auto& [channelId, channel] : m_channels){
      if(channel.state == Hue::Api::Channel::State::Active){
        centers.emplace_back(channelId, _desktopCenterX(channel, m_monitors));
      }
    }

    return centers;
  }


  void ChannelStore::_resolveMonitors(
    Grabber::Monitors monitors
  )
  {
    m_monitors = std::move(monitors);

    for(auto& [channelId, channel] : m_channels){
      // Profiles from before monitors per lamp captured only one monitor: the first one
      if(!channel.monitor.has_value() && channel.state == Hue::Api::Channel::State::Active && !m_monitors.empty()){
        channel.monitor = Grabber::monitorRef(m_monitors.front());
      }

      // Lost monitors are never found
      channel.monitorIndex = channel.monitor.has_value() ? Grabber::findMonitor(*channel.monitor, m_monitors) : std::nullopt;

      if(channel.monitor.has_value() && !channel.monitorIndex.has_value()){
        Logger::warn("The monitor of channel ", static_cast<int>(channelId), " isn't captured (", channel.monitor->name, " ", channel.monitor->geometry.width, "x", channel.monitor->geometry.height, "): it stays off while syncing");
      }
    }
  }


  float ChannelStore::_desktopCenterX(
    const Hue::Api::Channel& channel,
    const Grabber::Monitors& monitors
  )
  {
    if(!channel.monitorIndex.has_value() || *channel.monitorIndex >= monitors.size()){
      return 0.5f;
    }

    return Grabber::desktopCenterX(channel.uvs, monitors[*channel.monitorIndex].geometry, Grabber::desktopBounds(monitors));
  }
}
