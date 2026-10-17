#pragma once

#include <span>
#include <string>

#include <Halomux/Stream/DtlsConfig.hpp>


namespace Halomux::Stream
{
  class Impl;

  class DtlsClient
  {
  public:
    // Constructor / destructor
    /**
     * @brief DTlsClient constructor
     * 
     * @param dtlsConfig DTLS configuration
     */
    DtlsClient(const DtlsConfig& dtlsConfig);

    /**
     * @brief MbedTlsClient destructor
     * 
     */
    ~DtlsClient();

    bool isConnected() const;

    /**
     * @brief Calls all the inner initializations
     * 
     */
    void init();

    void shutdown();

    /**
     * @brief Sends byte buffer to the server
     * 
     * @param requestBuffer Byte buffer to send to the server
     * @return true Sending was successful
     * @return false Sending failed
     */
    bool send(
      std::span<const std::byte> data
    );

    DtlsConfig m_dtlsConfig;
    std::unique_ptr<Impl> m_clientImpl;
  };
}
