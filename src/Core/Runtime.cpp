#include <Huenicorn/Core/Runtime.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>

#include <glm/common.hpp>

#include <Huenicorn/Network/Http/Client/Client.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Platform/Selector.hpp>

#include <Huenicorn/Serialization/Channel.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>

#include <Huenicorn/Imaging/UV.hpp>

#include <Huenicorn/Core/CoreService.hpp>
#include <Huenicorn/Core/PreviewSequence.hpp>
#include <Huenicorn/Hue/Api/Channel.hpp>

#include <Huenicorn/Hue/Api/ApiTools.hpp>


using namespace std::chrono_literals;


namespace Huenicorn::Core
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


  void Runtime::start()
  {
    if(!m_coreService->ensureInitialSetup() || !m_keepLooping){
      return;
    }

    if(!_initGrabber()){
      Logger::error("Could not start any grabber");
      return;
    }

    if(!m_keepLooping){
      return;
    }

    Logger::log("Started ", m_grabber->name());

    if(!_initSettings()){
      Logger::error("Could not load suitable entertainment configuration.");
      return;

      // TODO : Add tool to create entertainment configurations inside the application
      // so the official application would no longer be a requirement
    }

    bool spawnBrowser = (!m_coreService->getProfile().has_value() && !m_openedSetup);
    // Created before the web UI, which can change its interval
    m_loopRegulator = std::make_unique<Timing::LoopRegulator>(Timing::fromHertz(m_config.refreshRate()));

    if(!_initWebUI(spawnBrowser)){
      Logger::error("Could not start the web UI on ", m_config.boundBackendIP(), ":", m_config.restServerPort(), ". Is the port already in use?");
      _shutdown();
      return;
    }

    _startStreamingLoop();
  }


  void Runtime::stop()
  {
    m_keepLooping = false;
    m_coreService->abortInitialSetup();
  }



  bool Runtime::_initSettings()
  {
    // Streaming faster than the bridge can process, or than the display refreshes, only wastes resources
    const unsigned maxRefreshRate = m_coreService->maxRefreshRate();
    if(m_config.refreshRate() == 0 || m_config.refreshRate() > maxRefreshRate){
      Logger::log("Refresh rate set to ", maxRefreshRate, " Hz (display: ", m_grabber->displayRefreshRate(), " Hz, bridge limit: ", Stream::MaxStreamingRate, " Hz)");
      m_config.setRefreshRate(maxRefreshRate);
    }

    // Don't let the compositor produce frames that would never be consumed
    m_grabber->setFrameRateLimit(m_config.refreshRate());

    if(m_config.subsampleWidth() == 0){
      auto displayResolution = m_grabber->displayResolution();
      float percentThreshold = 1.0;
      auto subsampleCandidates = m_grabber->subsampleResolutionCandidates();

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

    float ratio = static_cast<float>(m_config.subsampleWidth()) / static_cast<float>(m_grabber->displayResolution().x);

    if(ratio >= warningThreshold / 100){
      Logger::warn("Subsample width is >= ", warningThreshold, "% of the display resolution. Color computation might be intensive.");
    }

    Logger::log("Configuration is ready. Feel free to modify it manually by editing ", std::quoted(m_config.configFilePath().string()));

    const auto& credentials = m_config.credentials().value();
    const std::string& bridgeAddress =  m_config.bridgeAddress().value();

    m_selector = std::make_unique<Hue::Api::EntertainmentConfigurationSelector>(credentials, bridgeAddress);

    std::string entertainmentConfigurationId = {};
    auto optJsonProfile = m_coreService->getProfile();

    if(optJsonProfile.has_value()){
      const auto& jsonProfile = optJsonProfile.value();
      if(jsonProfile.contains("entertainmentConfigurationId")){
        entertainmentConfigurationId = jsonProfile.at("entertainmentConfigurationId");
      }
    }

    _enableEntertainmentConfiguration(entertainmentConfigurationId);

    return true;
  }


  bool Runtime::_initGrabber()
  {
    m_grabber = Platform::adapter.getGrabber(&m_config);
    return m_grabber != nullptr;
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


  void Runtime::_initChannels(
    const Serialization::Json& jsonChannels
  )
  {
    const auto& username = m_config.credentials().value().username();
    const auto& bridgeAddress =  m_config.bridgeAddress().value();
    auto devices = Hue::Api::ApiTools::loadDevices(username, bridgeAddress);
    auto entertainmentConfigurationsChannels = Hue::Api::ApiTools::loadEntertainmentConfigurationsChannels(username, bridgeAddress);

    Hue::Api::Channels channels;

    for(const auto& [id, channel] : m_selector->currentEntertainmentConfiguration().channels){
      bool found = false;
      const auto& members = Hue::Api::ApiTools::matchDevices(entertainmentConfigurationsChannels.at(m_selector->currentEntertainmentConfigurationId().value()).at(id), devices);
      for(const auto& jsonProfileChannel : jsonChannels){
        if(jsonProfileChannel.at("channelId") == id){
          bool active = jsonProfileChannel.at("active");
          Serialization::Json jsonUVs = jsonProfileChannel.at("uvs");

          Imaging::UVs uvs = jsonUVs.get<Imaging::UVs>();

          float gammaFactor = jsonProfileChannel.at("gammaFactor");
          channels.emplace(id, Hue::Api::Channel{active, members, gammaFactor, uvs});

          found = true;
          break;
        }
      }

      if(!found){
        channels.emplace(id, Hue::Api::Channel{false, members, 0.0f});
      }
    }

    std::lock_guard lock(m_channelsMutex);
    m_channels = std::move(channels);
    m_channelStreams.clear();
  }


  bool Runtime::_enableEntertainmentConfiguration(
    const std::string& entertainmentConfigurationId
  )
  {
    if(!m_selector->selectEntertainmentConfiguration(entertainmentConfigurationId)){
      return false;
    }

    const auto& credentials = m_config.credentials().value();

    // The DTLS handshake can take seconds: perform it before taking the lock the streaming loop waits on
    auto streamer = std::make_unique<Stream::Streamer>(credentials, m_config.bridgeAddress().value());
    streamer->setEntertainmentConfigurationId(m_selector->currentEntertainmentConfigurationId().value());

    {
      std::lock_guard lock(m_streamerMutex);
      m_streamer = std::move(streamer);
    }

    Serialization::Json jsonChannels = Serialization::Json::object();

    auto optJsonProfile = m_coreService->getProfile();
    if(optJsonProfile.has_value() && optJsonProfile->contains("channels")){
      jsonChannels = optJsonProfile->at("channels");
    }

    _initChannels(jsonChannels);

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
    // Start or stop listening to the sound when the setting changes
    if(m_config.audioReactive() != static_cast<bool>(m_audioAnalyzer)){
      if(m_audioAnalyzer){
        m_audioAnalyzer.reset();
      }
      else{
        m_audioAnalyzer = std::make_unique<Audio::AudioAnalyzer>();
      }
    }

    const Audio::AudioAnalyzer::Triggers soundTriggers = m_audioAnalyzer ? m_audioAnalyzer->takeTriggers() : Audio::AudioAnalyzer::Triggers{0.f, 0.f};

    std::lock_guard channelsLock(m_channelsMutex);

    // The grabber averages the regions of the active channels on each captured frame
    Grabber::IGrabber::ChannelRegions regions;
    for(const auto& [channelId, channel] : m_channels){
      if(channel.state == Hue::Api::Channel::State::Active){
        regions.emplace_back(channelId, channel.uvs);
      }
    }

    if(regions != m_channelRegions){
      m_grabber->setChannelRegions(regions);
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
    if(!m_grabber->grabChannelColors(channelColors) && !previewing){
      // Grabbers with asynchronous capture may produce slower than the core consumes
      return;
    }

    for(auto& [channelId, channel] : m_channels){
      if(channel.state == Hue::Api::Channel::State::Inactive){
        continue;
      }

      auto colorIt = std::find_if(channelColors.begin(), channelColors.end(), [channelId](const auto& channelColor){
        return channelColor.first == channelId;
      });

      if(channel.state == Hue::Api::Channel::State::Active && colorIt == channelColors.end() && !previewing){
        // Activated after the last captured frame: wait for the next one
        continue;
      }

      // Streams are only created once a channel has been active, so channels
      // that were never enabled are left untouched on the bridge. Shut down
      // channels keep being streamed as black.
      auto streamIt = std::find_if(m_channelStreams.begin(), m_channelStreams.end(), [channelId](const auto& stream){
        return stream.id == channelId;
      });

      if(streamIt == m_channelStreams.end()){
        streamIt = m_channelStreams.insert(m_channelStreams.end(), Hue::Api::ChannelStream{.id = channelId});
      }

      auto& channelStream = *streamIt;

      if(channel.state == Hue::Api::Channel::State::PendingShutdown){
        channelStream.id = channelId;
        channelStream.r = 0;
        channelStream.g = 0;
        channelStream.b = 0;
        channel.hasPreviousXyb = false;
        channel.hasPreviousMeasuredBrightness = false;
        channel.highlightBoost = 0.f;
        channel.acknowledgeShutdown();
      }
      else{
        // Horizontal position of the lamp's region, 0 = left, 1 = right
        const float pan = glm::clamp((channel.uvs.min.x + channel.uvs.max.x) / 2.f, 0.f, 1.f);

        const Imaging::Color color = previewing ? PreviewSequence::colorAt(previewTime, pan) : colorIt->second;

        glm::vec3 correctedColor;
        /*/
        glm::vec3 normalized = color.toNormalized();
        glm::vec3 correctedColor = glm::pow(normalized, glm::vec3(channel.gammaExponent()));
        /*/
        glm::vec3 xybColor = color.toXYB();

        // Ease toward the newly measured color instead of snapping to it, so
        // fast-changing content doesn't produce an abrupt strobe-like cut.
        // transitionSmoothing == 0 reproduces the previous (instant) behavior.
        float smoothing = m_config.transitionSmoothing();
        if(channel.hasPreviousXyb && smoothing > 0.f){
          xybColor = glm::mix(channel.previousXyb, xybColor, 1.f - smoothing);
        }
        channel.previousXyb = xybColor;
        channel.hasPreviousXyb = true;

        xybColor.z = glm::pow(xybColor.z, channel.gammaExponent());

        // Global dimmer, applied last so it scales what the lamp would otherwise show
        xybColor.z *= m_config.brightness();

        // Flash on sudden brightening, after smoothing so it isn't eased away: the boost takes
        // the lamp part of the way to full brightness and fades with a fixed time constant
        static constexpr float BoostTimeConstant = 0.25f; // [s]
        const float tickDuration = 1.f / static_cast<float>(std::max(1u, m_config.refreshRate()));
        const float decay = std::exp(-tickDuration / BoostTimeConstant);
        // Sound: pan the left and right channel triggers by the horizontal center of the lamp's region
        const float soundTrigger = (1.f - pan) * soundTriggers[0] + pan * soundTriggers[1];

        const float boost = channel.updateHighlightBoost(color.brightness(), m_config.highlightBoost(), decay, soundTrigger);
        xybColor.z += boost * (1.f - xybColor.z);

        correctedColor = xybColor;
        //*/

        channelStream.id = channelId;
        channelStream.r = correctedColor.r;
        channelStream.g = correctedColor.g;
        channelStream.b = correctedColor.b;
      }
    }

    {
      std::lock_guard lock(m_streamerMutex);
      if(m_streamer.get()){
        m_streamer->streamChannels(m_channelStreams);
      }
    }

    {
      std::lock_guard lock(m_currentColorsMutex);
      m_currentColorsSnapshot = m_channelStreams;
    }
  }


  void Runtime::_shutdown()
  {
    {
      // Close the DTLS session before asking the bridge to stop streaming
      std::lock_guard lock(m_streamerMutex);
      m_streamer.reset();
    }

    std::lock_guard lock(m_selectorMutex);
    m_selector->disableStreaming();
  }

}
