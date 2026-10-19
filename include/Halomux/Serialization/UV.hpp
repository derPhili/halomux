#pragma once

#include <algorithm>

#include <glm/common.hpp>

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
      // Corners lie on their monitor: others, such as hand-edited ones, are brought onto it
      const auto onMonitor = [](const Json& value){
        return static_cast<float>(std::clamp(value.get<double>(), 0.0, 1.0));
      };

      uv = Imaging::UV{
        onMonitor(jsonUV.at("x")),
        onMonitor(jsonUV.at("y"))
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

      // In either order
      uvs = Imaging::UVs{
        .min = glm::min(min, max),
        .max = glm::max(min, max)
      };
    }
  };
}
