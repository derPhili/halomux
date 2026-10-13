#include <Huenicorn/Stream/DtlsClient.hpp>

#include <memory>


#include <mbedtls/version.h>

#if MBEDTLS_VERSION_MAJOR >= 4
#include <Huenicorn/Stream/Impl/MbedTls/MbedTlsClient4Impl.hpp>
#elif MBEDTLS_VERSION_MAJOR == 3
#include <Huenicorn/Stream/Impl/MbedTls/MbedTlsClient3Impl.hpp>
#else
#error Unsupported Mbed TLS version
#endif


#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Stream
{
  DtlsClient::DtlsClient(
    const DtlsConfig& dtlsConfig
  ):
  m_dtlsConfig(dtlsConfig)
  {}


  DtlsClient::~DtlsClient()
  {
    shutdown();
  }


  bool DtlsClient::isConnected() const
  {
    return m_clientImpl && m_clientImpl->isConnected;
  }


  void DtlsClient::init()
  {
    m_clientImpl = std::make_unique<Impl>();
    m_clientImpl->_init(m_dtlsConfig);
  }


  void DtlsClient::shutdown()
  {
    m_clientImpl.reset();
  }


  bool DtlsClient::send(
    std::span<const std::byte> requestBuffer
  )
  {
    if(!isConnected()){
      Core::Logger::error("Dtls client is not connected");
      return false;
    }

    int result = m_clientImpl->_send(requestBuffer);

    return result >= 0;
  }
}
