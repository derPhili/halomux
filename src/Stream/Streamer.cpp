#include <Halomux/Stream/Streamer.hpp>

#include <algorithm>
#include <chrono>

#include <Halomux/Stream/DtlsClient.hpp>
#include <Halomux/Core/Logger.hpp>


namespace Halomux::Stream
{
  namespace
  {
    // Interval between connection attempts while the bridge doesn't accept the stream
    constexpr auto ReconnectionInterval = std::chrono::seconds(5);
  }


  Streamer::Streamer(
    const Hue::Auth::Credentials& credentials,
    const std::string& bridgeAddress
  )
  {
    // Started first: its stop token also stops a running handshake
    m_connectionThread = std::jthread([this](std::stop_token stopToken){_connectionLoop(stopToken);});

    m_dtlsClient = std::make_unique<DtlsClient>(DtlsConfig{
      .credentials = credentials,
      .address = bridgeAddress,
      .port = "2100",
      .hostname = "Hue",
      .handshakeAttempts = 4,
      .stopToken = m_connectionThread.get_stop_token()
    });
    _requestConnection();
  }


  void Streamer::setEntertainmentConfigurationId(
    const std::string& entertainmentConfigurationId
  )
  {
    m_header.setEntertainmentConfigurationId(entertainmentConfigurationId);
  }


  void Streamer::_requestConnection()
  {
    {
      std::lock_guard lock(m_connectionMutex);
      m_connectionRequestPending = true;
    }

    m_connectionRequested.notify_one();
  }


  void Streamer::_connectionLoop(
    std::stop_token stopToken
  )
  {
    bool failureReported = false;

    while(!stopToken.stop_requested()){
      {
        std::unique_lock lock(m_connectionMutex);
        if(!m_connectionRequested.wait(lock, stopToken, [this](){return m_connectionRequestPending;})){
          return;
        }

        m_connectionRequestPending = false;
      }

      if(m_connected){
        continue;
      }

      try{
        m_dtlsClient->init();
        m_connected = true;
        failureReported = false;
      }
      catch(const std::exception& e){
        if(stopToken.stop_requested()){
          return;
        }

        if(!failureReported){
          Core::Logger::error(e.what());
          Core::Logger::warn("Streaming connection to the bridge is unavailable. Retrying every ", ReconnectionInterval.count(), " seconds.");
          failureReported = true;
        }

        // Wait before the next attempt, unless stopped meanwhile
        std::unique_lock lock(m_connectionMutex);
        m_connectionRequested.wait_for(lock, stopToken, ReconnectionInterval, [](){return false;});
        m_connectionRequestPending = true;
      }
    }
  }


  void Streamer::reconnect()
  {
    m_reconnectRequested = true;
  }


  void Streamer::streamChannels(
    const Hue::Api::ChannelStreams& channels
  )
  {
    // Only this thread hands the DTLS client over to the connection thread
    if(m_reconnectRequested.exchange(false)){
      m_connected = false;
      _requestConnection();
      return;
    }

    if(!m_connected){
      // Dropped: colors are streamed again on the next frames once connected
      return;
    }

    // Reuse the buffer's capacity across frames
    auto& requestBuffer = m_requestBuffer;
    requestBuffer.clear();
    appendHuestreamMessage(requestBuffer, m_header, channels);

    if(!m_dtlsClient->send(requestBuffer)){
      // The session is lost: hand the client over to the connection thread
      m_connected = false;
      _requestConnection();
      return;
    }

    m_header.sequenceId++;
  }
}
