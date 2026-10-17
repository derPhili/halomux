#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <utility>
#include <vector>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Imaging/Color.hpp>


namespace Halomux::Core::Scenes
{
  /**
   * @brief Static scene going from a color on the leftmost lamp to another on the rightmost one
   *
   */
  struct BuiltinScene
  {
    std::string_view name;
    Imaging::Color left;
    Imaging::Color right;
  };


  inline const std::array<BuiltinScene, 10> Builtin = {{
    {"Warm white", {255, 180, 110}, {255, 180, 110}},
    {"Daylight", {230, 238, 255}, {230, 238, 255}},
    {"Sunset", {255, 94, 58}, {255, 170, 60}},
    {"Ocean", {0, 90, 255}, {0, 200, 200}},
    {"Forest", {30, 160, 60}, {150, 200, 40}},
    {"Neon", {255, 0, 170}, {0, 200, 255}},
    {"Galaxy", {90, 40, 255}, {200, 60, 255}},
    {"Fire", {255, 40, 0}, {255, 140, 0}},
    {"Candlelight", {255, 122, 30}, {180, 70, 10}},
    {"Off", {0, 0, 0}, {0, 0, 0}}
  }};


  /**
   * @brief Returns the built-in scene with the given name
   *
   * @param name Scene name
   * @return const BuiltinScene* Scene, nullptr if none has that name
   */
  inline const BuiltinScene* findBuiltin(std::string_view name)
  {
    auto it = std::find_if(Builtin.begin(), Builtin.end(), [name](const BuiltinScene& scene){
      return scene.name == name;
    });

    return it != Builtin.end() ? &*it : nullptr;
  }


  /**
   * @brief Spreads a built-in scene over lamps, from left to right
   *
   * @param scene Scene to spread
   * @param lampCenters Channel ID and horizontal center of the screen region of each lamp
   * @return Config::StaticColors Color by channel ID, interpolated by rank from left to right
   */
  inline Config::StaticColors spread(const BuiltinScene& scene, std::vector<std::pair<uint8_t, float>> lampCenters)
  {
    std::stable_sort(lampCenters.begin(), lampCenters.end(), [](const auto& a, const auto& b){
      return a.second < b.second;
    });

    auto mix = [](Imaging::Color::ChannelDepth a, Imaging::Color::ChannelDepth b, float t){
      return static_cast<Imaging::Color::ChannelDepth>(std::lround(a * (1.f - t) + b * t));
    };

    Config::StaticColors colors;
    for(size_t i = 0; i < lampCenters.size(); i++){
      const float t = lampCenters.size() > 1 ? static_cast<float>(i) / static_cast<float>(lampCenters.size() - 1) : 0.f;
      colors.emplace(lampCenters[i].first, Imaging::Color{
        mix(scene.left.m_r, scene.right.m_r, t),
        mix(scene.left.m_g, scene.right.m_g, t),
        mix(scene.left.m_b, scene.right.m_b, t)
      });
    }

    return colors;
  }
}
