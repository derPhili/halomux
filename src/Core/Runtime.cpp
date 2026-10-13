#include <Huenicorn/Core/Runtime.hpp>

#include <algorithm>
#include <fstream>
#include <future>

#include <glm/common.hpp>

#include <Huenicorn/Grabber/DummyGrabber.hpp>
#include <Huenicorn/Network/Http/Client/Client.hpp>
#include <Huenicorn/Imaging/ImageProcessing.hpp>
#include <Huenicorn/Imaging/Interpolation.hpp>
#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Platform/Selector.hpp>

#include <Huenicorn/Serialization/Channel.hpp>
#include <Huenicorn/Serialization/Credentials.hpp>

#include <Huenicorn/Imaging/UV.hpp>

#include <Huenicorn/Core/CoreService.hpp>
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

      // TODO : Add tool to create entertainment configurations inside Huenicorn
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
    if(m_config.refreshRate() == 0){
      // The bridge forwards colors to the lights at about 25 Hz, Philips recommends streaming
      // at 50-60 Hz. Following a high refresh rate display would only waste resources.
      static constexpr unsigned DefaultMaxRefreshRate = 60;
      m_config.setRefreshRate(std::min(m_grabber->displayRefreshRate(), DefaultMaxRefreshRate));
    }

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

    {
      std::lock_guard lock(m_streamerMutex);
      m_streamer = std::make_unique<Stream::Streamer>(credentials, m_config.bridgeAddress().value());
      m_streamer->setEntertainmentConfigurationId(m_selector->currentEntertainmentConfigurationId().value());
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
    m_grabber->grabFrameSubsample(m_frameData);
    if(!m_frameData.hasData()){
      // Grabbers with asynchronous capture may produce slower than the core consumes
      return;
    }

    Imaging::ImageData source = m_frameData;

    const auto subsampleWidth = static_cast<int>(m_config.subsampleWidth());

    if(m_frameData.width() > subsampleWidth){
      Imaging::ImageData resized;
      Imaging::ImageProcessing::rescale(m_frameData, resized, subsampleWidth, m_config.interpolation());

      if(resized.hasData()){
        source = std::move(resized);
      }
    }

    Imaging::ImageProcessing::toBgr(source, source);

    std::lock_guard channelsLock(m_channelsMutex);

    for(auto& [channelId, channel] : m_channels){
      if(channel.state == Hue::Api::Channel::State::Inactive){
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
        channel.acknowledgeShutdown();
      }
      else{
        Imaging::ImageData crop;
        Imaging::ImageProcessing::getSubImage(source, crop, channel.uvs);
        auto color = Imaging::ImageProcessing::getDominantColor(crop);

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
    std::lock_guard lock(m_selectorMutex);
    m_selector->disableStreaming();
  }

}
