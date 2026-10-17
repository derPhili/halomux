#pragma once

#include <Halomux/Serialization/JsonSerializer.hpp>
#include <Halomux/Imaging/UV.hpp>


namespace Halomux::Serialization
{
  template<>
  struct JsonSerializer<Halomux::Imaging::UV>
  {
    static void from_json(
      const Json& jsonUV,
      Imaging::UV& uv
    )
    {
      uv = Imaging::UV{
        jsonUV.at("x"),
        jsonUV.at("y")
      };
    }


    static void to_json(
      Json& jsonUv,
      const Imaging::UV& uv
    )
    {
      jsonUv = {
        {"x", uv.x},
        {"y", uv.y}
      };
    }
  };


  template<>
  struct JsonSerializer<Halomux::Imaging::UVs>
  {
    static void to_json(
      Json& jsonUvs,
      const Imaging::UVs& uvs
    )
    {
      jsonUvs = {
        {"uvA", uvs.min},
        {"uvB", uvs.max}
      };
    }

    static void from_json(
      const Json& jsonUVs,
      Imaging::UVs& uvs
    )
    {
      Imaging::UV min = jsonUVs.at("uvA").get<Imaging::UV>();
      Imaging::UV max = jsonUVs.at("uvB").get<Imaging::UV>();

      uvs = Imaging::UVs{
        .min = min,
        .max = max
      };
    }
  };
}
