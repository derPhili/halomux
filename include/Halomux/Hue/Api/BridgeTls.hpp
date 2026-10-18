#pragma once

#include <string>
#include <string_view>

#include <Halomux/Network/Http/Client/Client.hpp>


namespace Halomux::Hue::Api
{
  /**
   * @brief Signify's private CA, which signs the certificates of the Hue bridges
   *
   * A bridge certificate carries the bridge id (lowercase) as common name. Valid until 2038.
   */
  inline constexpr const char* BridgeRootCertificate = R"(-----BEGIN CERTIFICATE-----
MIICMjCCAdigAwIBAgIUO7FSLbaxikuXAljzVaurLXWmFw4wCgYIKoZIzj0EAwIw
OTELMAkGA1UEBhMCTkwxFDASBgNVBAoMC1BoaWxpcHMgSHVlMRQwEgYDVQQDDAty
b290LWJyaWRnZTAiGA8yMDE3MDEwMTAwMDAwMFoYDzIwMzgwMTE5MDMxNDA3WjA5
MQswCQYDVQQGEwJOTDEUMBIGA1UECgwLUGhpbGlwcyBIdWUxFDASBgNVBAMMC3Jv
b3QtYnJpZGdlMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEjNw2tx2AplOf9x86
aTdvEcL1FU65QDxziKvBpW9XXSIcibAeQiKxegpq8Exbr9v6LBnYbna2VcaK0G22
jOKkTqOBuTCBtjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNV
HQ4EFgQUZ2ONTFrDT6o8ItRnKfqWKnHFGmQwdAYDVR0jBG0wa4AUZ2ONTFrDT6o8
ItRnKfqWKnHFGmShPaQ7MDkxCzAJBgNVBAYTAk5MMRQwEgYDVQQKDAtQaGlsaXBz
IEh1ZTEUMBIGA1UEAwwLcm9vdC1icmlkZ2WCFDuxUi22sYpLlwJY81Wrqy11phcO
MAoGCCqGSM49BAMCA0gAMEUCIEBYYEOsa07TH7E5MJnGw557lVkORgit2Rm1h3B2
sFgDAiEA1Fj/C3AN5psFMjo0//mrQebo0eKd3aWRx+pQY08mk48=
-----END CERTIFICATE-----
)";


  /**
   * @brief Tells whether a text is a bridge id: 16 hexadecimal digits, the name in its certificate
   *
   * @param bridgeId Text, such as the bridgeid a bridge answers
   * @return true It's a bridge id
   */
  bool isBridgeId(
    std::string_view bridgeId
  );


  /**
   * @brief Registers which bridge an address belongs to, for the certificate checks
   *
   * @param bridgeAddress Address of the bridge
   * @param bridgeId Id of the bridge, as in its certificate
   * @param requireVerification Whether the bridge's certificate passed the check before: then it
   *                            must always pass it. Bridges whose certificate never did (other CA,
   *                            old firmware) are accepted unverified, as before the checks existed
   * @return false The id isn't a bridge id, and nothing was registered
   */
  bool registerBridge(
    const std::string& bridgeAddress,
    const std::string& bridgeId,
    bool requireVerification
  );


  /**
   * @brief Returns how the certificate of a bridge is checked
   *
   * @param bridgeAddress Address of the bridge
   * @return Network::Http::Client::TlsPolicy Signify's CA, with the bridge id as name once the
   *         bridge is registered
   */
  Network::Http::Client::TlsPolicy bridgeTls(
    const std::string& bridgeAddress
  );
}
