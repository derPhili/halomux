#pragma once

#include <map>
#include <optional>
#include <string>

#include <Huenicorn/Serialization/Json.hpp>


/**
 * @brief Provides an abstraction around HTTP requests and returns JSON data structs
 * 
 * Beware : For a compatible workflow with Hue Bridge,
 *          The related implementations must disable SLL verification for both peer and host
 *          In consequence, this HTTP client should not be trusted for any other use
 */
namespace Huenicorn::Network::Http::Client
{
  using Headers = std::multimap<std::string, std::string>;

  class Response
  {
  public:
    Response(
      const std::string& response
    ):
    m_response(response)
    {}

    const std::string& asString() const
    {
      return m_response;
    }

    const Serialization::Json asJson() const
    {
      return Serialization::Json::parse(m_response);
    }

  private:
    const std::string m_response{};
  };


  /**
   * @brief Performs a HTTP(S) request and returns a JSON response
   * 
   * @param url Target URL
   * @param method HTTP method
   * @param body HTTP request body
   * @param headers HTTP request headers
   * @return Serialization::Json JSON response
   */
  std::optional<Response> sendRequest(
    const std::string& url,
    const std::string& method,
    const std::string& body = "",
    const Headers& headers = {}
  );
}

