#pragma once

#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Serialization
{
  template <typename T>
  struct JsonSerializer;

  template <typename T>
  concept JsonWritable =
    requires (Json& j, const T& t) {
      JsonSerializer<T>::to_json(j, t);
    };

  template <typename T>
  concept JsonReadable =
    requires (const Json& j, T& t) {
      JsonSerializer<T>::from_json(j, t);
    };
}


namespace nlohmann
{
  template <Halomux::Serialization::JsonWritable T>
  struct adl_serializer<T>
  {
    static void to_json(
      json& j,
      const T& value
    )
    {
      Halomux::Serialization::JsonSerializer<T>::to_json(j, value);
    }

    static void from_json(
      const json& j,
      T& value
    )
      requires Halomux::Serialization::JsonReadable<T>
    {
      Halomux::Serialization::JsonSerializer<T>::from_json(j, value);
    }
  };
}
