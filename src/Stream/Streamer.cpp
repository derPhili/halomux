#include <Huenicorn/Stream/Streamer.hpp>

#include <Huenicorn/Stream/DtlsClient.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Stream
{
  Streamer::Streamer(
    const Hue::Auth::Credentials& credentials,
    const std::string& bridgeAddress
  )
  {
    m_dtlsClient = std::make_unique<DtlsClient>(DtlsConfig{
      .credentials = credentials,
      .address = bridgeAddress,
      .port = "2100",
      .hostname = "Hue",
      .handshakeAttempts = 4
    });
    _ensureConnection();
  }


  void Streamer::setEntertainmentConfigurationId(
    const std::string& entertainmentConfigurationId
  )
  {
    m_header.setEntertainmentConfigurationId(entertainmentConfigurationId);
  }


  bool Streamer::_ensureConnection()
  {
    if(m_dtlsClient->isConnected()){
      return true;
    }

    // Handshakes block for up to a few seconds: don't retry on every frame
    static constexpr auto ReconnectionInterval = std::chrono::seconds(5);
    auto now = std::chrono::steady_clock::now();
    if(m_lastConnectionAttempt != std::chrono::steady_clock::time_point{} && now - m_lastConnectionAttempt < ReconnectionInterval){
      return false;
    }

    m_lastConnectionAttempt = now;

    try{
      m_dtlsClient->init();
      m_connectionLossReported = false;
      return true;
    }
    catch(const std::exception& e){
      if(!m_connectionLossReported){
        Core::Logger::error(e.what());
        Core::Logger::warn("Streaming connection to the bridge is unavailable. Retrying every ", ReconnectionInterval.count(), " seconds.");
        m_connectionLossReported = true;
      }
    }

    return false;
  }


  void Streamer::streamChannels(
    const Hue::Api::ChannelStreams& channels
  )
  {
    if(!_ensureConnection()){
      return;
    }

    std::vector<std::byte> requestBuffer;
    requestBuffer.insert(requestBuffer.end(), reinterpret_cast<std::byte*>(&m_header), reinterpret_cast<std::byte*>(&m_header) + sizeof(HuestreamHeader));

    for(const auto channel : channels){
      HuestreamPayload payload;
      payload.setChannelId(channel.id);
      payload.setR(static_cast<uint16_t>(0xffff * channel.r));
      payload.setG(static_cast<uint16_t>(0xffff * channel.g));
      payload.setB(static_cast<uint16_t>(0xffff * channel.b));

      requestBuffer.insert(requestBuffer.end(), reinterpret_cast<std::byte*>(&payload), reinterpret_cast<std::byte*>(&payload) + sizeof(HuestreamPayload));
    }

    m_dtlsClient->send(requestBuffer);
    m_header.sequenceId++;
  }
}
