#pragma once

#include <map>
#include <optional>
#include <string>

#include <Halomux/Serialization/Json.hpp>


/**
 * @brief Provides an abstraction around HTTP requests and returns JSON data structs
 *
 * Server certificates are checked against the system's CA certificates by default. Hue bridges
 * need a TlsPolicy: their certificates are signed by a private CA, with the bridge id as name.
 */
namespace Halomux::Network::Http::Client
{
  using Headers = std::multimap<std::string, std::string>;

  // Larger answers are refused: they come from the network, and a bridge's whole resource list
  // is about 140 KB
  inline constexpr size_t MaxResponseSize = 8 * 1024 * 1024;


  /**
   * @brief How the server certificate of a request is checked
   *
   */
  struct TlsPolicy
  {
    // Trusted CA certificates (PEM), instead of the system's
    std::string caPem;

    // Name the certificate must carry, instead of the URL's host. Empty: the name isn't checked
    std::string expectedName;

    // Whether a certificate failing the check is accepted anyway, unverified
    bool allowUnverified{false};

    // Whether the answer tells the certificate's public key (see Response::publicKeyPin())
    bool readPublicKey{false};

    // Public key a certificate failing the check must have, "sha256//" and the base64 SHA-256
    // of its DER encoding; empty for any. A certificate passing the check needs none
    std::string pinnedPublicKey;
  };

  class Response
  {
  public:
    Response(
      const std::string& response,
      bool tlsVerified = true,
      const std::string& publicKeyPin = {}
    ):
    m_response(response),
    m_tlsVerified(tlsVerified),
    m_publicKeyPin(publicKeyPin)
    {}

    const std::string& asString() const
    {
      return m_response;
    }

    const Serialization::Json asJson() const
    {
      return Serialization::Json::parse(m_response);
    }

    /**
     * @brief Returns whether the server certificate passed the check
     *
     * @return false The server was accepted unverified, see TlsPolicy::allowUnverified
     */
    bool tlsVerified() const
    {
      return m_tlsVerified;
    }

    /**
     * @brief Returns the public key of the server certificate, as TlsPolicy::pinnedPublicKey
     *        takes it
     *
     * @return const std::string& Its pin, empty unless TlsPolicy::readPublicKey asked for it
     */
    const std::string& publicKeyPin() const
    {
      return m_publicKeyPin;
    }

  private:
    const std::string m_response{};
    const bool m_tlsVerified{true};
    const std::string m_publicKeyPin{};
  };


  /**
   * @brief Performs a HTTP(S) request and returns a JSON response
   * 
   * @param url Target URL
   * @param method HTTP method
   * @param body HTTP request body
   * @param headers HTTP request headers
   * @param tls How the server certificate is checked, the system's CA certificates if empty
   * @return Serialization::Json JSON response
   */
  std::optional<Response> sendRequest(
    const std::string& url,
    const std::string& method,
    const std::string& body = "",
    const Headers& headers = {},
    const std::optional<TlsPolicy>& tls = std::nullopt
  );
}

