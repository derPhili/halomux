#include <Halomux/Core/Logger.hpp>
#include <Halomux/Core/Profile.hpp>
#include <Halomux/Hue/Api/BridgeApi.hpp>

using namespace Halomux;
using Serialization::Json;


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


  Json lamp(int id, double right)
  {
    return {{"channelId", id}, {"active", true}, {"gammaFactor", 0.0}, {"uvs", {{"uvA", {{"x", 0.0}, {"y", 0.0}}}, {"uvB", {{"x", right}, {"y", 1.0}}}}}};
  }
}


int main()
{
  using namespace Core::Profile;

  const Json pcChannels = Json::array({lamp(0, 0.5), lamp(1, 1.0)});
  const Json dancefloorChannels = Json::array({lamp(0, 0.3)});

  // Profiles written before areas had their own lamp settings
  const Json legacy = {{"entertainmentConfigurationId", "pc"}, {"channels", pcChannels}};
  expect("Legacy profile applies to its own area", channelsForArea(legacy, "pc") == pcChannels);
  expect("Legacy profile doesn't apply to another area", channelsForArea(legacy, "dancefloor").empty());
  expect("Legacy area remembered", currentArea(legacy) == "pc");

  // Saving another area keeps the first one's lamps
  const Json both = withArea(legacy, "dancefloor", dancefloorChannels);
  expect("Saved area applies", channelsForArea(both, "dancefloor") == dancefloorChannels);
  expect("Former area kept when saving another one", channelsForArea(both, "pc") == pcChannels);
  expect("Saved area becomes the current one", currentArea(both) == "dancefloor");
  expect("Legacy channels moved under their area", !both.contains("channels"));

  // Saving an area again replaces its lamps only
  const Json updated = withArea(both, "pc", Json::array({lamp(0, 0.4)}));
  expect("Area replaced", channelsForArea(updated, "pc") == Json::array({lamp(0, 0.4)}));
  expect("Other area untouched", channelsForArea(updated, "dancefloor") == dancefloorChannels);

  // Missing or hand-edited profiles
  expect("No profile", channelsForArea(Json(), "pc").empty() && currentArea(Json()).empty());
  expect("Unknown area", channelsForArea(both, "garage").empty());
  expect("Malformed areas ignored", channelsForArea(Json{{"areas", 3}}, "pc").empty());
  expect("Malformed channels ignored", channelsForArea(Json{{"areas", {{"pc", {{"channels", "x"}}}}}}, "pc").empty());
  expect("Malformed current area ignored", currentArea(Json{{"entertainmentConfigurationId", 5}}).empty());
  expect("Saving into a malformed profile", channelsForArea(withArea(Json{{"areas", 3}}, "pc", pcChannels), "pc") == pcChannels);

  // Lamps of an area from its saved settings
  {
    const Hue::Api::ChannelMembers members{{0, {}}, {1, {}}, {2, {}}};
    const Json saved = Json::array({
      lamp(0, 0.5),
      {{"channelId", 1}, {"active", "yes"}, {"uvs", 3}, {"gammaFactor", 0.0}},
      {{"channelId", 7}, {"active", true}, {"gammaFactor", 0.0}, {"uvs", lamp(0, 1.0).at("uvs")}},
      "garbage"
    });

    const auto channels = channelsFromProfile(members, saved);
    expect("Every lamp of the area", channels.size() == 3);
    expect("Saved lamp restored", channels.at(0).state == Hue::Api::Channel::State::Active && channels.at(0).uvs.max.x == 0.5f);
    expect("Malformed lamp falls back to the defaults", channels.at(1).state == Hue::Api::Channel::State::Inactive);
    expect("Lamp without settings inactive", channels.at(2).state == Hue::Api::Channel::State::Inactive);
    expect("Settings of a lamp not in the area ignored", !channels.contains(7));
    expect("No profile at all", channelsFromProfile(members, Json()).size() == 3);
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Profile: OK");
  return 0;
}
