#pragma once

#include <memory>
#include <string>
#include <vector>


#include <psa/crypto.h>

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>

#include <mbedtls/debug.h>
#include <mbedtls/error.h>

#ifdef MBEDTLS_PLATFORM_C
#include <mbedtls/platform.h>
#endif

#include <Huenicorn/Stream/DtlsConfig.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Stream
{
  class DtlsClient;
}

namespace Huenicorn::Stream
{
  class Impl
  {
  public:
    ~Impl()
    {
      _deallocate();
    }

  private:
    friend DtlsClient;

    template <auto FreeFunc>
    struct MbedTlsDeleter
    {
      template <typename T>
      void operator()(T* ptr) const noexcept
      {
        if(ptr){
          FreeFunc(ptr);
          ptr = nullptr;
        }
      }
    };

    template <typename T, auto FreeFunc>
    using MbedTlsUniquePtr = std::unique_ptr<T, MbedTlsDeleter<FreeFunc>>;

    using UniqueNetContext = MbedTlsUniquePtr<mbedtls_net_context, mbedtls_net_free>;
    using UniqueCert = MbedTlsUniquePtr<mbedtls_x509_crt, mbedtls_x509_crt_free>;
    using UniqueSsl = MbedTlsUniquePtr<mbedtls_ssl_context, mbedtls_ssl_free>;
    using UniqueSslConfig = MbedTlsUniquePtr<mbedtls_ssl_config, mbedtls_ssl_config_free>;

    UniqueNetContext serverFd;
    UniqueSsl ssl;
    UniqueSslConfig conf;
    UniqueCert cacert;
    mbedtls_timing_delay_context timer;
    std::vector<int> ciphers;
    bool isConnected{false};

    static void debugCallback(
      void* ctx,
      int level,
      const char* file,
      int line,
      const char* str
    )
    {
      const char* p, *basename;
      (void)ctx;

      for(p = basename = file; *p != '\0'; p++){
        if(*p == '/' || *p == '\\'){
          basename = p + 1;
        }
      }

      mbedtls_printf("%s:%04d: |%d| %s", basename, line, level, str);
    }


    void _ensurePsaInit()
    {
      static const psa_status_t status = psa_crypto_init();

      if(status != PSA_SUCCESS){
        throw std::runtime_error("psa_crypto_init failed with code: " + std::to_string(static_cast<int>(status)));
      }
    }


    void _init(
      const DtlsConfig& dtlsConfig
    )
    {
      _ensurePsaInit();

      _initMembers();
      _initConnection(dtlsConfig);
      _initSSL(dtlsConfig);
      _handshake(dtlsConfig);
    }

    /**
     * @brief Initializes MbedTLS objects
     * 
     */
    void _initMembers()
    {
      serverFd.reset(new mbedtls_net_context{});
      mbedtls_net_init(serverFd.get());

      ssl.reset(new mbedtls_ssl_context{});
      mbedtls_ssl_init(ssl.get());

      conf.reset(new mbedtls_ssl_config{});
      mbedtls_ssl_config_init(conf.get());

      cacert.reset(new mbedtls_x509_crt{});
      mbedtls_x509_crt_init(cacert.get());
    }


    /**
     * @brief Initializes the connection
     * 
     */
    void _initConnection(
      const DtlsConfig& dtlsConfig
    )
    {
      int result = mbedtls_net_connect(
        serverFd.get(),
        dtlsConfig.address.c_str(),
        dtlsConfig.port.c_str(),
        MBEDTLS_NET_PROTO_UDP
      );

      if(result != 0){
        throw std::runtime_error("mbedtls_net_connect failed with code: " + std::to_string(result));
      }
    }


    /**
     * @brief Initializes the SSL layer with PSK
     * 
     */
    void _initSSL(
      const DtlsConfig& dtlsConfig
    )
    {
      auto pskRawArray = dtlsConfig.credentials.clientkeyBytes();
      auto pskIdRawArray = dtlsConfig.credentials.usernameBytes();

      {
        int result = mbedtls_ssl_config_defaults(
          conf.get(),
          MBEDTLS_SSL_IS_CLIENT,
          MBEDTLS_SSL_TRANSPORT_DATAGRAM,
          MBEDTLS_SSL_PRESET_DEFAULT
        );

        if(result != 0){
          throw std::runtime_error("mbedtls_ssl_config_defaults failed with code: " + std::to_string(result));
        }
      }

      mbedtls_ssl_conf_authmode(conf.get(), MBEDTLS_SSL_VERIFY_OPTIONAL);
      mbedtls_ssl_conf_ca_chain(conf.get(), cacert.get(), NULL);

      {
        int result = mbedtls_ssl_conf_psk(
          conf.get(),
          reinterpret_cast<const unsigned char*>(pskRawArray.data()),
          pskRawArray.size() * sizeof(unsigned char),
          reinterpret_cast<const unsigned char*>(pskIdRawArray.data()),
          pskIdRawArray.size() * sizeof(unsigned char)
        );

        if(result != 0){
          throw std::runtime_error("mbedtls_ssl_conf_psk failed with code: " + std::to_string(result));
        }
      }

      ciphers.push_back(MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256);
      ciphers.push_back(0);

      mbedtls_ssl_conf_ciphersuites(conf.get(), ciphers.data());

      {
        int result = mbedtls_ssl_setup(ssl.get(), conf.get());

        if(result != 0){
          throw std::runtime_error("mbedtls_ssl_setup failed with code: " + std::to_string(result));
        }
      }

      {
        int result = mbedtls_ssl_set_hostname(ssl.get(), dtlsConfig.hostname.c_str());

        if(result != 0){
          throw std::runtime_error("mbedtls_ssl_set_hostname failed with code: " + std::to_string(result));
        }
      }

      mbedtls_ssl_set_bio(
        ssl.get(),
        serverFd.get(),
        mbedtls_net_send,
        mbedtls_net_recv,
        mbedtls_net_recv_timeout
      );

      mbedtls_ssl_set_timer_cb(
        ssl.get(),
        &timer,
        mbedtls_timing_set_delay,
        mbedtls_timing_get_delay
      );
    }


    /**
     * @brief Execute handshake
     * 
     */
    void _handshake(
      const DtlsConfig& dtlsConfig
    )
    {
      int result{0};
      for(unsigned attempt = 0; attempt <  dtlsConfig.handshakeAttempts; attempt++){
        if(attempt > 0){
          // A failed handshake leaves the context in an error state
          int resetResult = mbedtls_ssl_session_reset(ssl.get());
          if(resetResult != 0){
            result = resetResult;
            break;
          }
        }

        mbedtls_ssl_conf_handshake_timeout(conf.get(), 400, 1000);
        do{
          result = mbedtls_ssl_handshake(ssl.get());
        }
        while(result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE);

        if(result == 0){
          break;
        }
      }

      if(result != 0){
        throw std::runtime_error("mbedtls_ssl_handshake failed with code: " + std::to_string(result));
      }

      isConnected = true;
      Core::Logger::log("Dtls handshake successful");
    }


    int _send(
      std::span<const std::byte> requestBuffer
    )
    {
      int result = mbedtls_ssl_write(ssl.get(), reinterpret_cast<const unsigned char*>(requestBuffer.data()), requestBuffer.size());
      if(result < 0){
        isConnected = false;
      }

      return result;
    }


    /**
     * @brief Free all the allocated MbedTLS members
     * 
     */
    void _deallocate()
    {
      if(isConnected && ssl){
        // Tell the bridge the session is over instead of letting it time out
        mbedtls_ssl_close_notify(ssl.get());
      }

      isConnected = false;
      serverFd.reset();
      cacert.reset();
      ssl.reset();
      conf.reset();
    }
  };
}
