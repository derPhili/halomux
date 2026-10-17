#include <Halomux/Hue/Api/ClipParsing.hpp>


namespace Halomux::Hue::Api::ClipParsing
{
  using Serialization::Json;


  std::optional<bool> streamingActive(
    const Json& response
  )
  {
    if(!succeeded(response) || !response.contains("data") || !response.at("data").is_array() || response.at("data").empty()){
      return std::nullopt;
    }

    const Json& area = response.at("data").front();
    if(!area.is_object() || !area.contains("status") || !area.at("status").is_string()){
      return std::nullopt;
    }

    return area.at("status").get<std::string>() == "active";
  }


  bool succeeded(
    const Json& response
  )
  {
    if(!response.is_object()){
      return false;
    }

    if(!response.contains("errors")){
      return true;
    }

    const Json& errors = response.at("errors");
    return errors.is_array() && errors.empty();
  }
}
