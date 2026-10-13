#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <Huenicorn/Hue/Api/Channel.hpp>
#include <Huenicorn/Hue/Auth/Credentials.hpp>
#include <Huenicorn/Stream/DtlsClient.hpp>
#include <Huenicorn/Stream/HuestreamHeader.hpp>
#include <Huenicorn/Stream/HuestreamPayload.hpp>


namespace Huenicorn::Stream
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
     * @brief (Re)establishes the DTLS connection, at most once per reconnection interval
     *
     * @return true The client is connected
     * @return false The client is not connected
     */
    bool _ensureConnection();


    // Attributes
    std::unique_ptr<Stream::DtlsClient> m_dtlsClient;
    HuestreamHeader m_header;
    std::vector<std::byte> m_requestBuffer;
    std::chrono::steady_clock::time_point m_lastConnectionAttempt{};
    bool m_connectionLossReported{false};
  };
}
