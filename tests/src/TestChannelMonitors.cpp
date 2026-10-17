#include <string>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Serialization/Channel.hpp>

using namespace Halomux;


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


int main()
{
  using Serialization::Json;
  using Serialization::monitorRefFromJson;

  // Profiles written before monitors were chosen per lamp
  const Json legacy = Json::parse(R"({"channelId": 0, "active": true, "gammaFactor": 0.0, "uvs": {"uvA": {"x": 0, "y": 0}, "uvB": {"x": 0.4, "y": 1}}})");
  expect("No monitor in a legacy channel", !monitorRefFromJson(legacy).has_value());

  const Json withMonitor = Json::parse(R"({"channelId": 0, "monitor": {"x": 1920, "y": 0, "width": 2560, "height": 1440, "name": "DP-3"}})");
  const auto ref = monitorRefFromJson(withMonitor);
  expect("Monitor read", ref == Grabber::MonitorRef{{1920, 0, 2560, 1440}, "DP-3"});

  const Json negative = Json::parse(R"({"monitor": {"x": -1920, "y": -200, "width": 1920, "height": 1080, "name": ""}})");
  expect("Negative position read", monitorRefFromJson(negative) == Grabber::MonitorRef{{-1920, -200, 1920, 1080}, ""});

  // Hand-edited profiles must not stop the service
  expect("Malformed monitor ignored", !monitorRefFromJson(Json::parse(R"({"monitor": 3})")).has_value());
  expect("Incomplete monitor ignored", !monitorRefFromJson(Json::parse(R"({"monitor": {"x": 0, "y": 0}})")).has_value());
  expect("Wrong field type ignored", !monitorRefFromJson(Json::parse(R"({"monitor": {"x": "0", "y": 0, "width": 1, "height": 1, "name": ""}})")).has_value());
  expect("Missing name tolerated", monitorRefFromJson(Json::parse(R"({"monitor": {"x": 0, "y": 0, "width": 1920, "height": 1080}})")) == Grabber::MonitorRef{{0, 0, 1920, 1080}, ""});

  // Round trip through the profile serialization
  Hue::Api::Channel channel(true, {}, 0.f, {{0.1f, 0.2f}, {0.4f, 0.9f}});
  channel.monitor = Grabber::MonitorRef{{0, 249, 1920, 1080}, "DP-1"};
  const Json saved = Json(channel);
  expect("Monitor saved", saved.contains("monitor"));
  expect("Monitor round trip", monitorRefFromJson(saved) == channel.monitor);

  Hue::Api::Channel noMonitor(true, {}, 0.f);
  expect("No monitor saved without one", !Json(noMonitor).contains("monitor"));

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Channel monitors: OK");
  return 0;
}
