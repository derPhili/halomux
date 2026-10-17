#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <Halomux/Hue/Api/Channel.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>
#include <Halomux/Stream/DtlsClient.hpp>
#include <Halomux/Stream/HuestreamHeader.hpp>
#include <Halomux/Stream/HuestreamPayload.hpp>


namespace Halomux::Stream
{
  /**
   * @brief Wrapper around UDP requests to submit color data to the bridge
   * 
   */
  /**
   * @brief Highest useful streaming rate [Hz]
   *
   * The bridge forwards colors to the lights at about 25 Hz. Philips recommends
   * streaming at 50-60 Hz to make up for lost UDP packets: anything above is wasted.
   */
  static constexpr unsigned MaxStreamingRate = 60;


  class Streamer
  {
    static const std::string Port;


  public:
    // Constructor
    /**
     * @brief Streamer constructor
     * 
     * @param credentials Hue bridge credentials
     * @param bridgeAddress Hue bridge address
     */
    Streamer(
      const Hue::Auth::Credentials& credentials,
      const std::string& bridgeAddress
    );


    // Setters
    /**
     * @brief Sets the entertainment configuration header field
     * 
     * @param entertainmentConfigurationId ID of the entertainment configuration to set to the request header
     */
    void setEntertainmentConfigurationId(
      const std::string& entertainmentConfigurationId
    );


    // Methods
    /**
     * @brief Submit the channels data to the stream
     *
     * Never blocks on the connection: while it isn't established, the data is dropped and
     * the connection thread is asked to (re)connect.
     *
     * @param channels
     */
    void streamChannels(
      const Hue::Api::ChannelStreams& channels
    );


  private:
    // Private methods
    /**
     * @brief Converts a normalized value to the 16 bits range, clamping it to [0, 1]
     *
     * @param value Normalized value
     * @return uint16_t Value in [0, 0xffff]
     */
    static uint16_t _toUint16(
      float value
    );


    /**
     * @brief Asks the connection thread to (re)establish the DTLS connection
     *
     */
    void _requestConnection();


    /**
     * @brief Establishes the DTLS connection when requested, retrying at a fixed interval
     *
     * Handshakes block for seconds when the bridge doesn't answer (for example while another
     * application streams to the entertainment area): they run here, not in the streaming loop.
     *
     * @param stopToken Stops the thread
     */
    void _connectionLoop(
      std::stop_token stopToken
    );


    // Attributes
    std::unique_ptr<Stream::DtlsClient> m_dtlsClient;
    HuestreamHeader m_header;
    std::vector<std::byte> m_requestBuffer;

    // Set by the connection thread once the handshake succeeded, cleared by the streaming
    // thread when sending fails. The DTLS client is only used by the thread it designates.
    std::atomic<bool> m_connected{false};

    std::mutex m_connectionMutex;
    std::condition_variable_any m_connectionRequested;
    bool m_connectionRequestPending{false};

    // Declared last: stopped and joined (after a running handshake) before the DTLS client is destroyed
    std::jthread m_connectionThread;
  };
}
