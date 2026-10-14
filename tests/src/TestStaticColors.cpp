#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Serialization/Config.hpp>

using namespace Huenicorn;
using namespace Huenicorn::Core;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }
}


/**
 * @brief Checks the #rrggbb notation and the serialization of static colors
 *
 */
int main()
{
  // Notation
  expect("Parse", Imaging::Color::fromHex("#ff8000") == Imaging::Color{255, 128, 0});
  expect("Parse upper case", Imaging::Color::fromHex("#FFB46E") == Imaging::Color{255, 180, 110});
  expect("Format", Imaging::Color{1, 171, 255}.toHex() == "#01abff");
  expect("Missing #", !Imaging::Color::fromHex("ff8000").has_value());
  expect("Too short", !Imaging::Color::fromHex("#ff800").has_value());
  expect("Not hexadecimal", !Imaging::Color::fromHex("#ff80zz").has_value());
  expect("Sign", !Imaging::Color::fromHex("#+f8000").has_value());

  // Round trip through the configuration file format
  const Config::StaticColors colors = {{0, {255, 94, 58}}, {1, {0, 90, 255}}};
  const Serialization::Json jsonColors = colors;
  expect("Keyed by channel ID", jsonColors.at("1") == "#005aff");
  expect("Round trip", jsonColors.get<Config::StaticColors>() == colors);

  const Config::Scenes scenes = {{"Ocean", colors}};
  expect("Scenes round trip", Serialization::Json(scenes).get<Config::Scenes>() == scenes);

  // Invalid input is rejected instead of silently changed
  auto rejects = [](const Serialization::Json& json){
    try{
      json.get<Config::StaticColors>();
      return false;
    }
    catch(const std::exception&){
      return true;
    }
  };

  expect("Invalid color", rejects(Serialization::Json{{"0", "red"}}));
  expect("Invalid channel", rejects(Serialization::Json{{"256", "#ffffff"}}));
  expect("Channel not a number", rejects(Serialization::Json{{"left", "#ffffff"}}));

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Static colors: OK");
  return 0;
}
