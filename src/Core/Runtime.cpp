#include <Halomux/Core/Runtime.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <ranges>
#include <utility>

#include <glm/common.hpp>

#include <Halomux/Core/ChannelColor.hpp>
#include <Halomux/Core/Profile.hpp>
#include <Halomux/Network/Http/Client/Client.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Platform/Selector.hpp>

#include <Halomux/Serialization/Channel.hpp>
#include <Halomux/Serialization/Credentials.hpp>

#include <Halomux/Imaging/UV.hpp>

#include <Halomux/Core/CoreService.hpp>
#include <Halomux/Core/PreviewSequence.hpp>
#include <Halomux/Hue/Api/Channel.hpp>

#include <Halomux/Hue/Api/ApiTools.hpp>
#include <Halomux/Hue/Api/BridgeAddress.hpp>


using namespace std::chrono_literals;


namespace
{
  // Shown by lamps without a static color yet: warm white
  const Halomux::Imaging::Color DefaultStaticColor{255, 180, 110};
}


namespace Halomux::Core
{
  Runtime::Runtime(
    const std::string& version,
    const std::filesystem::path& configRoot
  ):
  m_version(version),
  m_configRoot(configRoot),
  m_config(m_configRoot),
  m_coreService(std::make_unique<CoreService>(*this, m_config))
  {

  }


  const std::string& Runtime::version() const
  {
    return m_version;
  }


  bool Runtime::start()
  {
    if(!m_coreService->ensureInitialSetup() || !m_keepLooping){
      return true;
    }

    switch(_initGrabber()){
      case GrabberStart::Started:
        break;

      case GrabberStart::Cancelled:
        // The user's choice, or a stop: starting again would only ask again
        Logger::warn("Now quitting");
        return true;

      case GrabberStart::Failed:
        // Such as the portal or Pipewire not ready yet: worth starting again
        Logger::error("Could not start any grabber");
        return false;
    }

    if(!m_keepLooping){
      return true;
    }

    Logger::log("Started ", grabber()->name());

    if(!_initSettings()){
      Logger::error("Could not load suitable entertainment configuration.");
      return false;

      // TODO : Add tool to create entertainment configurations inside the application
      // so the official application would no longer be a requirement
    }

    bool spawnBrowser = (!m_coreService->getProfile().has_value() && !m_openedSetup);
    // Created before the web UI, which can change its interval
    m_loopRegulator = std::make_unique<Timing::LoopRegulator>(Timing::fromHertz(m_config.refreshRate()));

    if(!_initWebUI(spawnBrowser)){
      Logger::error("Could not start the web UI on ", m_config.boundBackendIP(), ":", m_config.restServerPort(), ". Is the port already in use?");
      _shutdown();
      return false;
    }

    _startStreamingLoop();
    return true;
  }


  void Runtime::stop()
  {
    m_keepLooping = false;
    // Closes the screen selection dialog of the start, if it's open
    m_stopSource.request_stop();
    // And the one of a monitor change
    std::lock_guard lock(m_reselectMutex);
    m_reselectThread.request_stop();
    m_coreService->abortInitialSetup();
  }



  bool Runtime::_initSettings()
  {
    // Streaming faster than the bridge can process, or than the display refreshes, only wastes resources
    const unsigned maxRefreshRate = m_coreService->maxRefreshRate();
    if(m_config.refreshRate() == 0 || m_config.refreshRate() > maxRefreshRate){
      Logger::log("Refresh rate set to ", maxRefreshRate, " Hz (display: ", grabber()->displayRefreshRate(), " Hz, bridge limit: ", Stream::MaxStreamingRate, " Hz)");
      m_config.setRefreshRate(maxRefreshRate);
    }

    // Don't let the compositor produce frames that would never be consumed
    grabber()->setFrameRateLimit(m_config.refreshRate());

    // Without a captured image, the width stays unset and is chosen on the next start
    auto subsampleCandidates = grabber()->subsampleResolutionCandidates();
    if(m_config.subsampleWidth() == 0 && !subsampleCandidates.empty()){
      auto displayResolution = grabber()->displayResolution();
      float percentThreshold = 1.0;

      auto bestSubsampleWidth = subsampleCandidates.back().x;
      for(auto i = subsampleCandidates.size(); i--;){
        auto candidate = subsampleCandidates.at(i).x;

        if((static_cast<float>(candidate) / static_cast<float>(displayResolution.x)) * 100 >= percentThreshold){
          bestSubsampleWidth = candidate;
          break;
        }
      }

      m_config.setSubsampleWidth(static_cast<unsigned>(bestSubsampleWidth));
    }

    const float warningThreshold = 50.0;

    float ratio = static_cast<float>(m_config.subsampleWidth()) / static_cast<float>(grabber()->displayResolution().x);

    if(ratio >= warningThreshold / 100){
      Logger::warn("Subsample width is >= ", warningThreshold, "% of the display resolution. Color computation might be intensive.");
    }

    Logger::log("Configuration is ready. Feel free to modify it manually by editing ", std::quoted(m_config.configFilePath().string()));

    const auto credentials = m_config.credentials().value();
    const std::string bridgeAddress = m_config.bridgeAddress().value();

    // Selects the area of the profile, and keeps trying in the background if the bridge is offline
    static constexpr auto AreaCheckInterval = std::chrono::seconds(5);
    m_areaSession = std::make_unique<AreaSession>(
      std::make_unique<Hue::Api::ClipBridgeApi>(credentials, bridgeAddress),
      [credentials, bridgeAddress](const std::string& areaId) -> std::unique_ptr<Stream::IStreamer>{
        // Connects in the background: replacing it doesn't hold the streaming loop
        // Connected by host, not by URL: IPv6 addresses without their brackets
        auto streamer = std::make_unique<Stream::Streamer>(credentials, Hue::Api::bridgeHost(bridgeAddress));
        streamer->setEntertainmentConfigurationId(areaId);
        return streamer;
      },
      [this](const std::string& areaId, const Hue::Api::ChannelMembers& members){_onAreaSelected(areaId, members);},
      std::chrono::duration_cast<std::chrono::milliseconds>(AreaCheckInterval)
    );

    m_areaSession->start(Profile::currentArea(m_coreService->getProfile().value_or(Serialization::Json())));

    return true;
  }


  Runtime::GrabberStart Runtime::_initGrabber()
  {
    std::shared_ptr<Grabber::IGrabber> grabber;
    try{
      grabber = Platform::adapter.getGrabber(&m_config, m_stopSource.get_token());
    }
    catch(const Grabber::GrabberCancelled& e){
      Logger::warn(e.what());
      return GrabberStart::Cancelled;
    }
    catch(const std::exception& e){
      Logger::error(e.what());
      return GrabberStart::Failed;
    }

    std::lock_guard lock(m_grabberMutex);
    m_grabber = std::move(grabber);
    return m_grabber ? GrabberStart::Started : GrabberStart::Failed;
  }


  bool Runtime::_initWebUI(
    bool spawnBrowser
  )
  {
    std::promise<bool> readyWebUIPromise;
    auto readyWebUIFuture = readyWebUIPromise.get_future();

    auto restServerPort = m_config.restServerPort();
    const std::string& boundBackendIP = m_config.boundBackendIP();
    m_webUIService.thread = std::jthread([&server = m_webUIService.server, restServerPort, boundBackendIP, &readyWebUIPromise, this](){
      server.emplace(m_coreService.get());
      server->start(restServerPort, boundBackendIP, std::move(readyWebUIPromise));
    });

    if(!readyWebUIFuture.get()){
      return false;
    }

    if(spawnBrowser){
      std::stringstream serviceUrlStream;
      serviceUrlStream << "http://127.0.0.1:" << m_config.restServerPort();
      std::string serviceURL = serviceUrlStream.str();
      Logger::log("Management WebUI is ready and available at ", serviceURL);

      Platform::adapter.openWebBrowser(serviceURL);
    }

    return true;
  }


  void Runtime::_onAreaSelected(
    const std::string& areaId,
    const Hue::Api::ChannelMembers& members
  )
  {
    // The lamp settings saved for this area only: channel ids repeat across areas
    const auto profileChannels = Profile::channelsForArea(m_coreService->getProfile().value_or(Serialization::Json()), areaId);
    auto channels = Profile::channelsFromProfile(members, profileChannels);

    // The monitors read under the lock: a screen selection finishing meanwhile can't put the
    // former grabber's monitors back. The lamps and their area are replaced together.
    auto access = m_channelStore.access();
    access.replace(areaId, std::move(channels), grabber()->monitors());
  }


  std::shared_ptr<Grabber::IGrabber> Runtime::grabber() const
  {
    std::lock_guard lock(m_grabberMutex);
    return m_grabber;
  }


  bool Runtime::_reselectMonitors()
  {
    MonitorSelection expected = m_monitorSelection.load();
    if(expected == MonitorSelection::Pending || !m_monitorSelection.compare_exchange_strong(expected, MonitorSelection::Pending)){
      return false;
    }

    std::lock_guard lock(m_reselectMutex);

    // A stop meanwhile: no new dialog
    if(!m_keepLooping){
      m_monitorSelection = MonitorSelection::Idle;
      return false;
    }

    // The previous selection is over: its thread only needs joining
    if(m_reselectThread.joinable()){
      m_reselectThread.join();
    }

    m_reselectThread = std::jthread([this](std::stop_token stopToken){
      auto newGrabber = Platform::adapter.replaceGrabber(&m_config, stopToken);
      if(!newGrabber){
        m_monitorSelection = MonitorSelection::Cancelled;
        return;
      }

      std::shared_ptr<Grabber::IGrabber> oldGrabber;
      {
        auto channels = m_channelStore.access();
        {
          std::lock_guard grabberLock(m_grabberMutex);
          oldGrabber = std::exchange(m_grabber, newGrabber);

          // Read once it's the grabber a change of the rate reaches: a change while the user
          // selected the monitors went to the former one
          newGrabber->setFrameRateLimit(m_config.refreshRate());
        }

        // Channel monitor indices refer to the new monitors from now on
        channels.resolveMonitors(newGrabber->monitors());
        m_grabberReplaced = true;
      }

      m_monitorSelection = MonitorSelection::Idle;
      Logger::log("Switched to the newly selected monitors");

      // Stops the former capture, unless an update still uses it
      oldGrabber.reset();
    });

    return true;
  }


  void Runtime::_startStreamingLoop()
  {
    m_loopRegulator->start();

    while(m_keepLooping){
      try{
        _update();
      }
      catch(const std::exception& e){
        // An exception escaping this thread would terminate the whole application
        Logger::error("Failed to update channels: ", e.what());
      }

      if(!m_loopRegulator->sync()){
        const auto& lastExcess = m_loopRegulator->lastExcess();
        float percentage = lastExcess.rate * 100;
        Logger::warn("Scheduled interval has been exceeded of ", lastExcess.extra.count(), "[s] (", percentage, "%).\n Please reduce refresh rate if this warning persists.");
      }
    }

    _shutdown();

    m_webUIService.server->stop();
    m_webUIService.thread.join();
  }


  void Runtime::_update()
  {
    // Without sync, the lamps show the static colors: nothing to capture or listen to
    const bool syncing = m_config.syncEnabled();

    // Start or stop listening to the sound when the setting changes
    if((m_config.audioReactive() && syncing) != static_cast<bool>(m_audioAnalyzer)){
      if(m_audioAnalyzer){
        m_audioAnalyzer.reset();
      }
      else{
        m_audioAnalyzer = std::make_unique<Audio::AudioAnalyzer>();
      }
    }

    const Audio::AudioAnalyzer::Triggers soundTriggers = m_audioAnalyzer ? m_audioAnalyzer->takeTriggers() : Audio::AudioAnalyzer::Triggers{0.f, 0.f};

    auto access = m_channelStore.access();
    auto& channels = access.channels;
    auto& channelStreams = access.streams;

    // Taken under the channels lock, like the replacement of a screen selection: the regions sent
    // below always reach the grabber that m_grabberReplaced refers to. Kept alive for this update.
    const auto grabber = this->grabber();

    // A new grabber captures everything until told otherwise
    if(m_grabberReplaced.exchange(false)){
      m_channelRegions.clear();
      grabber->setPaused(m_capturePaused);
    }

    // A monitor unplugged while capturing: its lamps go off, and the light manager says why
    const auto now = std::chrono::steady_clock::now();
    if(now - m_lastLostMonitorsCheck >= std::chrono::seconds(1)){
      m_lastLostMonitorsCheck = now;

      auto monitors = grabber->monitors();
      const bool lostChanged = monitors.size() != access.monitors.size() || std::ranges::any_of(std::views::iota(size_t{0}, monitors.size()), [&](size_t i){
        return monitors[i].lost != access.monitors[i].lost;
      });

      if(lostChanged){
        access.resolveMonitors(std::move(monitors));
      }
    }

    if(syncing == m_capturePaused){
      m_capturePaused = !syncing;
      grabber->setPaused(m_capturePaused);

      // A jump between the static colors and the screen is no highlight
      for(auto& [channelId, channel] : channels){
        channel.hasPreviousMeasuredBrightness = false;
      }
    }

    const Config::StaticColors staticColors = syncing ? Config::StaticColors{} : m_config.staticColors();

    // The grabber averages the regions of the active channels on each captured frame
    Grabber::IGrabber::ChannelRegions regions;
    for(const auto& [channelId, channel] : channels){
      // Channels whose monitor isn't captured stay off while syncing
      if(channel.state == Hue::Api::Channel::State::Active && channel.monitorIndex.has_value()){
        regions.push_back({channelId, *channel.monitorIndex, channel.uvs});
      }
    }

    if(regions != m_channelRegions){
      grabber->setChannelRegions(regions);
      m_channelRegions = std::move(regions);
    }

    // Preview: synthetic content replaces the screen for a few seconds
    float previewTime = -1.f;
    if(const int64_t previewStart = m_previewStart; previewStart != 0){
      const auto elapsed = std::chrono::steady_clock::now().time_since_epoch() - std::chrono::steady_clock::duration(previewStart);
      previewTime = std::chrono::duration<float>(elapsed).count();

      if(previewTime >= PreviewSequence::Duration){
        m_previewStart = 0;
        previewTime = -1.f;
      }
    }

    const bool previewing = previewTime >= 0.f;

    Grabber::IGrabber::ChannelColors channelColors;
    if(syncing && !grabber->grabChannelColors(channelColors) && !previewing){
      // Grabbers with asynchronous capture may produce slower than the core consumes
      return;
    }

    const ChannelColor::Settings colorSettings{
      .tickDuration = 1.f / static_cast<float>(std::max(1u, m_config.refreshRate())),
      .transitionSmoothing = m_config.transitionSmoothing(),
      .brightness = m_config.brightness(),
      .highlightBoost = m_config.highlightBoost(),
      .syncing = syncing,
      .previewing = previewing
    };

    for(auto& [channelId, channel] : channels){
      if(channel.state == Hue::Api::Channel::State::Inactive){
        continue;
      }

      auto colorIt = std::find_if(channelColors.begin(), channelColors.end(), [channelId](const auto& channelColor){
        return channelColor.first == channelId;
      });

      const bool monitorMissing = !channel.monitorIndex.has_value();

      if(channel.state == Hue::Api::Channel::State::Active && syncing && colorIt == channelColors.end() && !previewing && !monitorMissing){
        // Activated after the last captured frame: wait for the next one
        continue;
      }

      // Streams are only created once a channel has been active, so channels
      // that were never enabled are left untouched on the bridge. Shut down
      // channels keep being streamed as black.
      auto streamIt = std::find_if(channelStreams.begin(), channelStreams.end(), [channelId](const auto& stream){
        return stream.id == channelId;
      });

      if(streamIt == channelStreams.end()){
        streamIt = channelStreams.insert(channelStreams.end(), Hue::Api::ChannelStream{.id = channelId});
      }

      auto& channelStream = *streamIt;

      if(channel.state == Hue::Api::Channel::State::PendingShutdown){
        ChannelColor::shutDown(channel, channelStream);
        continue;
      }

      // Horizontal position of the lamp's region on the whole desktop, 0 = left, 1 = right
      const float pan = access.desktopCenterX(channel);

      Imaging::Color color;
      if(previewing){
        color = PreviewSequence::colorAt(previewTime, pan);
      }
      else if(!syncing){
        const auto staticColorIt = staticColors.find(channelId);
        color = staticColorIt != staticColors.end() ? staticColorIt->second : DefaultStaticColor;
      }
      else if(monitorMissing){
        color = Imaging::Color(0, 0, 0);
      }
      else{
        color = colorIt->second;
      }

      // Sound: pan the left and right channel triggers by the horizontal center of the lamp's region
      const float soundTrigger = (1.f - pan) * soundTriggers[0] + pan * soundTriggers[1];

      const glm::vec3 xyb = ChannelColor::step(channel, color, colorSettings, soundTrigger);
      channelStream.r = xyb.x;
      channelStream.g = xyb.y;
      channelStream.b = xyb.z;
    }

    if(m_areaSession){
      m_areaSession->stream(channelStreams);
    }

    {
      std::lock_guard lock(m_currentColorsMutex);
      m_currentColorsSnapshot = channelStreams;
    }
  }


  void Runtime::_shutdown()
  {
    if(m_areaSession){
      m_areaSession->stop();
    }
  }

}
