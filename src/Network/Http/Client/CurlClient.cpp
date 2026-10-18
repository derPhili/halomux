#include <Halomux/Network/Http/Client/Client.hpp>

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>

#include <curl/curl.h>
#include <curl/easy.h>

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
      std::string& responseString
    )
    {
      auto handle = std::unique_ptr<CURL, CurlDeleter>(curl_easy_init());
      if(!handle){
        Core::Logger::error("Failed to initialize CURL handle");
        throw std::runtime_error("CURL initialization failed");
      }

      std::string requestUrl = url;
      UniqueCurlSlist connectTo{nullptr};

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

      return curl_easy_perform(handle.get());
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
    CURLcode code = perform(url, method, body, headers, tls, true, responseString);
    bool verified = true;

    if(code == CURLE_PEER_FAILED_VERIFICATION && tls.has_value() && tls->allowUnverified){
      // The certificate failed the check before anything was sent: retry without checking it
      responseString.clear();
      code = perform(url, method, body, headers, tls, false, responseString);
      verified = false;
    }

    if(code != CURLE_OK){
      Core::Logger::error("HTTP request failed: " + std::string(curl_easy_strerror(code)));
      return std::nullopt;
    }

    return Response(responseString, verified);
  }
}
