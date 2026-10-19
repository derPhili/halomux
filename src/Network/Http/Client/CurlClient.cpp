#include <Halomux/Network/Http/Client/Client.hpp>

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <curl/curl.h>
#include <curl/easy.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Network::Http::Client
{
  class CurlDeleter
  {
  public:
    void operator()(
      CURL* curl
    ) const
    {
      curl_easy_cleanup(curl);
    }
  };

  struct CurlSlistDeleter
  {
    void operator()(
      curl_slist* slist
    ) const
    {
      curl_slist_free_all(slist);
    }
  };

  using UniqueCurlSlist = std::unique_ptr<curl_slist, CurlSlistDeleter>;


  size_t writeCallback(
    char* ptr,
    size_t size,
    size_t nmemb,
    std::string* data
  )
  {
    // Returning less than given aborts the transfer
    if(data->size() + size * nmemb > MaxResponseSize){
      return 0;
    }

    data->append(ptr, size * nmemb);
    return size * nmemb;
  }


  namespace
  {
    /**
     * @brief Returns the pin of a certificate's public key, as CURLOPT_PINNEDPUBLICKEY takes it
     *
     * @param pem Certificate (PEM)
     * @return std::string "sha256//" and the base64 SHA-256 of the key's DER encoding, empty if the
     *         certificate can't be read
     */
    std::string publicKeyPinOf(
      const std::string& pem
    )
    {
      std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
      std::unique_ptr<X509, decltype(&X509_free)> certificate(bio ? PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr) : nullptr, X509_free);
      if(!certificate){
        return {};
      }

      unsigned char* der = nullptr;
      const int derLength = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(certificate.get()), &der);
      if(derLength <= 0){
        return {};
      }

      std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
      SHA256(der, static_cast<size_t>(derLength), hash.data());
      OPENSSL_free(der);

      std::array<unsigned char, 4 * ((SHA256_DIGEST_LENGTH + 2) / 3) + 1> base64{};
      const int base64Length = EVP_EncodeBlock(base64.data(), hash.data(), static_cast<int>(hash.size()));
      return "sha256//" + std::string(reinterpret_cast<const char*>(base64.data()), static_cast<size_t>(base64Length));
    }


    // Pin of the server certificate's public key, from the certificates curl collected
    std::string serverPublicKeyPin(
      CURL* handle
    )
    {
      curl_certinfo* certificates = nullptr;
      if(curl_easy_getinfo(handle, CURLINFO_CERTINFO, &certificates) != CURLE_OK || !certificates || certificates->num_of_certs < 1){
        return {};
      }

      // The server's own certificate first
      static constexpr std::string_view CertField = "Cert:";
      for(const curl_slist* field = certificates->certinfo[0]; field; field = field->next){
        const std::string_view data(field->data);
        if(data.starts_with(CertField)){
          return publicKeyPinOf(std::string(data.substr(CertField.size())));
        }
      }

      return {};
    }


    /**
     * @brief Splits an URL around its host
     *
     * @param url URL such as "https://192.168.0.42/api/0/config" or "https://[fe80::1]/api"
     * @return std::array<std::string, 3> Scheme with "://", host, and the rest (port and path)
     */
    std::array<std::string, 3> splitHost(
      const std::string& url
    )
    {
      const size_t hostStart = url.find("://");
      if(hostStart == std::string::npos){
        return {"", url, ""};
      }

      // IPv6 addresses are bracketed and contain colons
      const size_t searchFrom = url.compare(hostStart + 3, 1, "[") == 0 ? url.find(']', hostStart) : hostStart + 3;
      const size_t hostEnd = searchFrom == std::string::npos ? std::string::npos : url.find_first_of(":/", searchFrom);
      const size_t hostLength = hostEnd == std::string::npos ? std::string::npos : hostEnd - hostStart - 3;

      return {
        url.substr(0, hostStart + 3),
        url.substr(hostStart + 3, hostLength),
        hostEnd == std::string::npos ? "" : url.substr(hostEnd)
      };
    }


    /**
     * @brief Performs a request
     *
     * @param tls Certificate check, the system's if empty
     * @param verify Whether the certificate is checked at all
     * @param responseString Receives the response body
     * @return CURLcode Result
     */
    CURLcode perform(
      const std::string& url,
      const std::string& method,
      const std::string& body,
      const Headers& headers,
      const std::optional<TlsPolicy>& tls,
      bool verify,
      bool pinned,
      std::string& responseString,
      std::string& publicKeyPin
    )
    {
      auto handle = std::unique_ptr<CURL, CurlDeleter>(curl_easy_init());
      if(!handle){
        Core::Logger::error("Failed to initialize CURL handle");
        throw std::runtime_error("CURL initialization failed");
      }

      std::string requestUrl = url;
      UniqueCurlSlist connectTo{nullptr};

      // The bridge is in the local network: a proxy of the environment would see its traffic,
      // and could answer in its place where its certificate can't be checked
      if(tls.has_value()){
        curl_easy_setopt(handle.get(), CURLOPT_NOPROXY, "*");

        // Checked by curl whether the certificate is checked or not
        if(pinned && !tls->pinnedPublicKey.empty()){
          curl_easy_setopt(handle.get(), CURLOPT_PINNEDPUBLICKEY, tls->pinnedPublicKey.c_str());
        }

        if(tls->readPublicKey){
          curl_easy_setopt(handle.get(), CURLOPT_CERTINFO, 1L);
        }
      }

      if(!verify){
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L);
      }
      else if(tls.has_value()){
        if(!tls->caPem.empty()){
          curl_blob caBlob{const_cast<char*>(tls->caPem.data()), tls->caPem.size(), CURL_BLOB_COPY};
          curl_easy_setopt(handle.get(), CURLOPT_CAINFO_BLOB, &caBlob);
          // Only this CA: some curl builds also trust a default directory of system certificates
          curl_easy_setopt(handle.get(), CURLOPT_CAINFO, nullptr);
          curl_easy_setopt(handle.get(), CURLOPT_CAPATH, nullptr);
        }

        if(tls->expectedName.empty()){
          curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, 0L);
        }
        else{
          // Address the server by the expected name, so it's the name checked, and connect to the
          // URL's host instead of resolving the name
          const auto [scheme, host, rest] = splitHost(url);
          requestUrl = scheme + tls->expectedName + rest;
          const std::string mapping = tls->expectedName + "::" + host + ":";
          connectTo.reset(curl_slist_append(nullptr, mapping.c_str()));
          curl_easy_setopt(handle.get(), CURLOPT_CONNECT_TO, connectTo.get());
        }
      }

      curl_easy_setopt(handle.get(), CURLOPT_URL, requestUrl.c_str());
      curl_easy_setopt(handle.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
      // The full resource list of a bridge with many devices takes a while: 0.3 s for 140 KB on a
      // bridge with about 20 lights. An unreachable bridge is still noticed quickly.
      curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, 3L);
      curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, 10L);

      if(body.size() > 0){
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.length()));
      }

      UniqueCurlSlist concatenatedHeaders{nullptr};
      for(const auto& header : headers){
        std::string concat = header.first + ": " + header.second;
        concatenatedHeaders.reset(curl_slist_append(concatenatedHeaders.release(), concat.c_str()));
      }

      if(concatenatedHeaders){
        curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, concatenatedHeaders.get());
      }

      // Refused at once when announced, otherwise while it's received
      curl_easy_setopt(handle.get(), CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(MaxResponseSize));
      curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, writeCallback);
      curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &responseString);

      const CURLcode code = curl_easy_perform(handle.get());
      if(code == CURLE_OK && tls.has_value() && tls->readPublicKey){
        publicKeyPin = serverPublicKeyPin(handle.get());
      }

      return code;
    }
  }


  std::optional<Response> sendRequest(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const Headers& headers,
    const std::optional<TlsPolicy>& tls
  )
  {
    std::string responseString;
    std::string publicKeyPin;
    CURLcode code = perform(url, method, body, headers, tls, true, true, responseString, publicKeyPin);
    bool verified = true;

    if(code == CURLE_PEER_FAILED_VERIFICATION && tls.has_value() && tls->allowUnverified){
      // The certificate failed the check before anything was sent: retry without checking it,
      // but with the pinned key, if any
      responseString.clear();
      code = perform(url, method, body, headers, tls, false, true, responseString, publicKeyPin);
      verified = false;
    }

    if(code == CURLE_SSL_PINNEDPUBKEYNOTMATCH && tls.has_value() && !tls->pinnedPublicKey.empty()){
      // Another key: accepted only with a certificate that passes the check, such as one the
      // bridge got from its CA since
      responseString.clear();
      code = perform(url, method, body, headers, tls, true, false, responseString, publicKeyPin);
      verified = true;
    }

    if(code != CURLE_OK){
      Core::Logger::error("HTTP request failed: " + std::string(curl_easy_strerror(code)));
      return std::nullopt;
    }

    return Response(responseString, verified, publicKeyPin);
  }
}
