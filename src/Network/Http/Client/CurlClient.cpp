#include <Huenicorn/Network/Http/Client/Client.hpp>

#include <memory>
#include <optional>
#include <stdexcept>

#include <curl/curl.h>
#include <curl/easy.h>

#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Network::Http::Client
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
    data->append(ptr, size * nmemb);
    return size * nmemb;
  }


  std::optional<Response> sendRequest(
    const std::string& url,
    const std::string& method,
    const std::string& body,
    const Headers& headers
  )
  {
    auto handle = std::unique_ptr<CURL, CurlDeleter>(curl_easy_init());
    if(!handle){
      Core::Logger::error("Failed to initialize CURL handle");
      throw std::runtime_error("CURL initialization failed");
    }

    Serialization::Json jsonBody = Serialization::Json::object();

    curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, 1);

    {
      // Requirement for self-signed Hue bridge
      curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER, false);
      curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYHOST, false);
    }

    if(body.size() > 0){
      curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE, body.length());
    }

    UniqueCurlSlist concatenatedHeaders{nullptr};
    if(!headers.empty()){
      // Disable ssl checks for the sake of getting data without trouble

      for(const auto& header : headers){
        std::string concat = header.first + ": " + header.second;
        concatenatedHeaders.reset(curl_slist_append(concatenatedHeaders.release(), concat.c_str()));
      }

      curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, concatenatedHeaders.get());
    }

    std::string responseString;
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &responseString);

    CURLcode code = curl_easy_perform(handle.get());

    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, nullptr);

    if(code != CURLE_OK){
      Core::Logger::error("HTTP request failed: " + std::string(curl_easy_strerror(code)));
      return std::nullopt;
    }

    return Response(responseString);
  }
}
