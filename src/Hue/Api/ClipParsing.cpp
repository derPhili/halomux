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


  Pairing pairing(
    const Json& response
  )
  {
    if(!response.is_array() || response.empty() || !response.at(0).is_object()){
      return {.credentials = std::nullopt, .error = "unexpected answer of the bridge"};
    }

    const Json& result = response.at(0);

    if(result.contains("success")){
      const Json& success = result.at("success");
      if(success.is_object() && success.contains("username") && success.at("username").is_string() && success.contains("clientkey") && success.at("clientkey").is_string()){
        return {.credentials = Json{{"username", success.at("username")}, {"clientkey", success.at("clientkey")}}, .error = ""};
      }

      return {.credentials = std::nullopt, .error = "the bridge sent no credentials"};
    }

    if(result.contains("error") && result.at("error").is_object() && result.at("error").contains("description") && result.at("error").at("description").is_string()){
      return {.credentials = std::nullopt, .error = result.at("error").at("description").get<std::string>()};
    }

    return {.credentials = std::nullopt, .error = "unexpected answer of the bridge"};
  }
}
