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
     * @brief (Re)establishes the DTLS connection, at most once per reconnection interval
     *
     * @return true The client is connected
     * @return false The client is not connected
     */
    bool _ensureConnection();


    // Attributes
    std::unique_ptr<Stream::DtlsClient> m_dtlsClient;
    HuestreamHeader m_header;
    std::chrono::steady_clock::time_point m_lastConnectionAttempt{};
    bool m_connectionLossReported{false};
  };
}
