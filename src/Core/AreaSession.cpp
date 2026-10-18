#include <Halomux/Core/AreaSession.hpp>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Version.hpp>


namespace Halomux::Core
{
  AreaSession::AreaSession(
    std::unique_ptr<Hue::Api::BridgeApi> bridge,
    StreamerFactory streamerFactory,
    SelectedCallback onSelected,
    std::chrono::milliseconds watchInterval
  ):
  m_bridge(std::move(bridge)),
  m_streamerFactory(std::move(streamerFactory)),
  m_onSelected(std::move(onSelected)),
  m_watchInterval(watchInterval)
  {}


  AreaSession::~AreaSession()
  {
    stop();
  }


  void AreaSession::start(
    const std::string& savedAreaId
  )
  {
    {
      std::lock_guard lock(m_operationMutex);
      m_savedAreaId = savedAreaId;
      _ensureSelection();
    }

    if(m_watchInterval.count() > 0 && !m_watchdog.joinable()){
      m_watchdog = std::jthread([this](std::stop_token stopToken){_watch(stopToken);});
    }
  }


  bool AreaSession::select(
    const std::string& areaId
  )
  {
    std::lock_guard lock(m_operationMutex);
    return _select(areaId);
  }


  void AreaSession::tick()
  {
    std::lock_guard lock(m_operationMutex);

    const std::optional<std::string> current = currentAreaId();
    if(!current.has_value()){
      _ensureSelection();
      return;
    }

    const std::optional<bool> active = m_bridge->streamingActive(*current);
    if(!active.has_value()){
      // The area may have been deleted in the Hue app, which the bridge answers with an error:
      // then another one is used. Unreachable, the bridge is asked again on the next check.
      if(_loadAreas() && !areas().contains(*current)){
        Logger::warn("The entertainment area no longer exists: using another one");
        _takeStreamer().reset();
        {
          std::lock_guard stateLock(m_stateMutex);
          m_currentAreaId.reset();
        }
        _ensureSelection(false);
      }
      return;
    }

    if(*active){
      return;
    }

    Logger::warn("The bridge stopped streaming to the entertainment area: starting it again");

    Hue::Api::EntertainmentConfigurationEntry area;
    {
      std::lock_guard stateLock(m_stateMutex);
      auto areaIt = m_areas.find(*current);
      if(areaIt == m_areas.end()){
        return;
      }
      area = *areaIt;
    }

    if(!m_bridge->setStreaming(area, true)){
      Logger::warn("The bridge refused to stream to the entertainment area, trying again later");
      return;
    }

    std::lock_guard streamerLock(m_streamerMutex);
    if(m_streamer){
      m_streamer->reconnect();
    }
  }


  void AreaSession::stream(
    const Hue::Api::ChannelStreams& channels
  )
  {
    std::lock_guard lock(m_streamerMutex);
    if(m_streamer){
      m_streamer->streamChannels(channels);
    }
  }


  void AreaSession::stop()
  {
    if(m_watchdog.joinable()){
      m_watchdog.request_stop();
      m_watchdog.join();
    }

    std::lock_guard lock(m_operationMutex);

    {
      // The DTLS session is closed before the bridge is asked to stop streaming
      _takeStreamer().reset();
    }

    std::optional<Hue::Api::EntertainmentConfigurationEntry> area;
    {
      std::lock_guard stateLock(m_stateMutex);
      if(m_currentAreaId.has_value()){
        auto areaIt = m_areas.find(*m_currentAreaId);
        if(areaIt != m_areas.end()){
          area = *areaIt;
        }
      }
      m_currentAreaId.reset();
    }

    if(area.has_value()){
      m_bridge->setStreaming(*area, false);
    }
  }


  Hue::Api::EntertainmentConfigurations AreaSession::areas() const
  {
    std::lock_guard lock(m_stateMutex);
    return m_areas;
  }


  Hue::Api::EntertainmentConfigurations AreaSession::refreshAreas()
  {
    std::lock_guard lock(m_operationMutex);
    _loadAreas();
    return areas();
  }


  std::optional<std::string> AreaSession::currentAreaId() const
  {
    std::lock_guard lock(m_stateMutex);
    return m_currentAreaId;
  }


  std::unique_ptr<Stream::IStreamer> AreaSession::_takeStreamer()
  {
    std::lock_guard lock(m_streamerMutex);
    return std::move(m_streamer);
  }


  bool AreaSession::_loadAreas()
  {
    auto areas = m_bridge->areas();
    if(!areas.has_value()){
      if(!m_unreachableReported){
        Logger::warn("The bridge is unreachable: trying again in the background");
        m_unreachableReported = true;
      }
      return false;
    }

    m_unreachableReported = false;

    std::lock_guard lock(m_stateMutex);
    m_areas = std::move(*areas);
    return true;
  }


  void AreaSession::_ensureSelection(
    bool loadAreas
  )
  {
    if(loadAreas && !_loadAreas()){
      return;
    }

    std::string areaId;
    {
      std::lock_guard lock(m_stateMutex);
      if(m_areas.empty()){
        if(!m_noAreaReported){
          Logger::error("The bridge has no entertainment area yet. Create one in the Philips Hue app to use it with ", AppName);
          m_noAreaReported = true;
        }
        return;
      }

      m_noAreaReported = false;

      if(m_areas.contains(m_savedAreaId)){
        areaId = m_savedAreaId;
      }
      else{
        if(!m_savedAreaId.empty()){
          Logger::warn("The saved entertainment area no longer exists: using another one");
        }
        areaId = m_areas.begin()->first;
      }
    }

    _select(areaId);
  }


  bool AreaSession::_select(
    const std::string& areaId
  )
  {
    // Possibly created in the Hue app since the areas were loaded
    bool known;
    {
      std::lock_guard lock(m_stateMutex);
      known = m_areas.contains(areaId);
    }
    if(!known){
      _loadAreas();
    }

    Hue::Api::EntertainmentConfigurationEntry area;
    std::optional<Hue::Api::EntertainmentConfigurationEntry> former;
    {
      std::lock_guard lock(m_stateMutex);
      auto areaIt = m_areas.find(areaId);
      if(areaIt == m_areas.end()){
        Logger::error("Unknown entertainment area ", areaId);
        return false;
      }
      area = *areaIt;

      if(m_currentAreaId.has_value() && *m_currentAreaId != areaId){
        auto formerIt = m_areas.find(*m_currentAreaId);
        if(formerIt != m_areas.end()){
          former = *formerIt;
        }
      }
    }

    // Loaded first: if this fails, nothing changed
    const auto members = m_bridge->channelMembers(areaId);
    if(!members.has_value()){
      Logger::error("Could not load the lamps of the entertainment area ", area.second.name);
      return false;
    }

    // Ended outside the lock: it may wait for a DTLS handshake attempt, which must not hold the
    // streaming loop
    _takeStreamer().reset();

    if(former.has_value()){
      m_bridge->setStreaming(*former, false);
    }

    // A refusal (such as another application streaming) is retried by the checks
    if(!m_bridge->setStreaming(area, true)){
      Logger::warn("The bridge refused to stream to ", area.second.name, ", trying again later");
    }

    {
      std::lock_guard streamerLock(m_streamerMutex);
      m_streamer = m_streamerFactory(areaId);
    }

    {
      std::lock_guard lock(m_stateMutex);
      m_currentAreaId = areaId;
    }

    Logger::log("Streaming to the entertainment area ", area.second.name);
    m_onSelected(areaId, *members);
    return true;
  }


  void AreaSession::_watch(
    std::stop_token stopToken
  )
  {
    while(!stopToken.stop_requested()){
      {
        std::unique_lock lock(m_wakeMutex);
        m_wake.wait_for(lock, stopToken, m_watchInterval, [](){return false;});
      }

      if(stopToken.stop_requested()){
        return;
      }

      try{
        tick();
      }
      catch(const std::exception& e){
        Logger::error("Checking the entertainment area failed: ", e.what());
      }
    }
  }
}
