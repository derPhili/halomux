#pragma once

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>


namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Hue::Auth::Credentials>
  {
    // Deserialization
    static void from_json(
      const Json& jsonCredentials,
      Hue::Auth::Credentials& credentials
    )
    {
      jsonCredentials.at("username").get_to(credentials.m_username);
      jsonCredentials.at("clientkey").get_to(credentials.m_clientkey);
    }

    // Serialization
    static void to_json(
      Json& jsonCredentials,
      const Hue::Auth::Credentials& credentials
    )
    {
      jsonCredentials = Json{
        {"username", credentials.username()},
        {"clientkey", credentials.clientkey()}
      };
    }
  };
}
