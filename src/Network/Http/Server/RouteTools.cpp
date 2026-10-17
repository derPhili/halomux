#include <Halomux/Network/Http/Server/RouteTools.hpp>

#include <charconv>


namespace Halomux::Network::Http::Server
{
  std::optional<uint8_t> parseChannelId(
    std::string_view text
  )
  {
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if(text.empty() || error != std::errc() || end != text.data() + text.size() || value > 255){
      return std::nullopt;
    }

    return static_cast<uint8_t>(value);
  }


  uint8_t channelIdOf(
    const Request& request
  )
  {
    const auto parameter = request.pathParams.find("channelId");
    const auto channelId = parameter != request.pathParams.end() ? parseChannelId(parameter->second) : std::nullopt;
    if(!channelId.has_value()){
      throw HttpError(404, "unknown channel");
    }

    return *channelId;
  }


  std::optional<Imaging::UVCorner> parseUVCorner(
    int value
  )
  {
    if(value < Imaging::UVCorner::TopLeft || value > Imaging::UVCorner::BottomRight){
      return std::nullopt;
    }

    return static_cast<Imaging::UVCorner>(value);
  }
}
