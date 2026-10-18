#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Halomux/Imaging/UV.hpp>
#include <Halomux/Network/Http/Server/HttpDataStructs.hpp>
#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Network::Http::Server
{
  /**
   * @brief Thrown by a handler to answer with an error status
   *
   * The server answers {"succeeded": false, "error": what()} with the status. Bodies that can't
   * be read are reported through readBody(); any other exception becomes 500.
   */
  class HttpError : public std::runtime_error
  {
  public:
    HttpError(
      int status,
      const std::string& message
    ):
    std::runtime_error(message),
    m_status(status)
    {}

    int status() const
    {
      return m_status;
    }

  private:
    int m_status;
  };


  /**
   * @brief Reads the JSON body of a request
   *
   * Everything read from the body goes in read: a body that isn't valid JSON, misses a field or
   * holds a wrong type or value is the client's error, answered 400. Exceptions after it are the
   * light manager's, answered 500.
   *
   * @param request Request
   * @param read Takes the parsed body and returns what the route needs from it
   * @return What read returns
   * @throw HttpError 400 if the body couldn't be read
   */
  template<typename Read>
  auto readBody(
    const Request& request,
    Read&& read
  )
  {
    try{
      return read(Serialization::Json::parse(request.body));
    }
    catch(const std::exception& e){
      throw HttpError(400, std::string("malformed request: ") + e.what());
    }
  }


  /**
   * @brief Reads a channel id from a path
   *
   * @param text Path parameter
   * @return std::optional<uint8_t> Id, nothing unless the whole text is a number from 0 to 255
   */
  std::optional<uint8_t> parseChannelId(
    std::string_view text
  );


  /**
   * @brief Returns the channel id of a request's path, from its "channelId" parameter
   *
   * @param request Request
   * @return uint8_t Channel id
   * @throws HttpError 404 if it isn't a valid channel id
   */
  uint8_t channelIdOf(
    const Request& request
  );


  /**
   * @brief Reads a corner of a region
   *
   * @param value 0 top left, 1 top right, 2 bottom left, 3 bottom right
   * @return std::optional<Imaging::UVCorner> Corner, nothing if out of range
   */
  std::optional<Imaging::UVCorner> parseUVCorner(
    int value
  );
}
