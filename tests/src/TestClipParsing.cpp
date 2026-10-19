#include <fstream>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Api/ClipParsing.hpp>

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


  // Made-up resource list in the shape of a bridge's answer to GET /clip/v2/resource
  Json fixture()
  {
    std::ifstream file(HALOMUX_TEST_FIXTURES "/clip-resources.json");
    return Json::parse(file);
  }


  std::vector<std::string> names(const Hue::Api::Devices& devices)
  {
    std::vector<std::string> result;
    for(const auto& device : devices){
      result.push_back(device.name);
    }

    return result;
  }


  std::vector<std::string> ids(const Hue::Api::Devices& devices)
  {
    std::vector<std::string> result;
    for(const auto& device : devices){
      result.push_back(device.id);
    }

    return result;
  }
}


int main()
{
  using namespace Hue::Api::ClipParsing;

  // Status of an entertainment area
  expect("Active area", streamingActive(Json::parse(R"({"errors": [], "data": [{"status": "active"}]})")) == true);
  expect("Inactive area", streamingActive(Json::parse(R"({"errors": [], "data": [{"status": "inactive"}]})")) == false);
  expect("Deleted area: empty data", !streamingActive(Json::parse(R"({"errors": [], "data": []})")).has_value());
  expect("Error answer", !streamingActive(Json::parse(R"({"errors": [{"description": "not found"}], "data": []})")).has_value());
  expect("Malformed answer", !streamingActive(Json::parse(R"({"data": "x"})")).has_value());
  expect("Missing status", !streamingActive(Json::parse(R"({"data": [{}]})")).has_value());
  expect("Not an object", !streamingActive(Json::parse("[1, 2]")).has_value());

  // Answers to requests changing a resource
  expect("Succeeded", succeeded(Json::parse(R"({"errors": [], "data": [{"rid": "x"}]})")));
  expect("Failed", !succeeded(Json::parse(R"({"errors": [{"description": "x"}], "data": []})")));
  expect("Malformed errors", !succeeded(Json::parse(R"({"errors": 3})")));
  expect("Not an object fails", !succeeded(Json::parse("null")));

  // Pairing with the bridge (v1 API): the link button pressed, not pressed, or something else
  {
    const auto paired = pairing(Json::parse(R"([{"success": {"username": "u", "clientkey": "00112233445566778899AABBCCDDEEFF"}}])"));
    expect("Paired", paired.credentials.has_value() && paired.credentials->at("username") == "u");

    // The clientkey is the pre-shared key of the streaming connection: hexadecimal
    const auto badKey = pairing(Json::parse(R"([{"success": {"username": "u", "clientkey": "K"}}])"));
    expect("Non-hex clientkey refused", !badKey.credentials.has_value() && !badKey.error.empty());

    const auto notPressed = pairing(Json::parse(R"([{"error": {"type": 101, "description": "link button not pressed"}}])"));
    expect("Link button not pressed", !notPressed.credentials.has_value() && notPressed.error == "link button not pressed");

    expect("Not an array", !pairing(Json::parse(R"({"error": "x"})")).credentials.has_value());
    expect("Empty array", !pairing(Json::parse("[]")).credentials.has_value());
    expect("Success without credentials", !pairing(Json::parse(R"([{"success": {"username": "u"}}])")).credentials.has_value());
    expect("Error without description", !pairing(Json::parse(R"([{"error": {}}])")).error.empty());
  }

  {
    // Entertainment areas from the whole resource list
    const auto parsed = areas(fixture());
    expect("Areas parsed", parsed.has_value());
    if(parsed){
      expect("Malformed area left out", parsed->size() == 2 && !parsed->contains("a0000000-0000-4000-8000-000000000003"));

      const auto desk = parsed->find("a0000000-0000-4000-8000-000000000001");
      expect("Area found", desk != parsed->end());
      if(desk != parsed->end()){
        expect("Area name", desk->second.name == "Desk");
        expect("Area lamps named after their lights", names(desk->second.devices) == std::vector<std::string>{"Play Left", "Play Right"});
        expect("Area lamps keep their light IDs", ids(desk->second.devices) == std::vector<std::string>{"10000000-0000-4000-8000-000000000001", "10000000-0000-4000-8000-000000000002"});
        expect("Area channels, inactive", desk->second.channels.size() == 2 && desk->second.channels.contains(0) && desk->second.channels.contains(1) && desk->second.channels.at(0).state == Hue::Api::Channel::State::Inactive);
      }

      const auto tv = parsed->find("a0000000-0000-4000-8000-000000000002");
      expect("Second area found", tv != parsed->end());
      if(tv != parsed->end()){
        expect("Missing light keeps an empty name", names(tv->second.devices) == std::vector<std::string>{"Lightstrip", ""});
      }
    }

    const auto none = areas(Json::parse(R"({"errors": [], "data": []})"));
    expect("Bridge without areas", none.has_value() && none->empty());
    expect("Error answer has no areas", !areas(Json::parse(R"({"errors": [{"description": "unauthorized user"}], "data": []})")).has_value());
    expect("Malformed answer has no areas", !areas(Json::parse(R"({"data": {}})")).has_value());
  }

  {
    // Lamps of each channel, named after their devices
    const auto desk = channelMembers(fixture(), "a0000000-0000-4000-8000-000000000001");
    expect("Channel members parsed", desk.has_value() && desk->size() == 2);
    if(desk && desk->size() == 2){
      expect("Channel 0 lamp", names(desk->at(0)) == std::vector<std::string>{"Play Left"} && ids(desk->at(0)) == std::vector<std::string>{"e0000000-0000-4000-8000-000000000001"});
      expect("Channel 1 lamp", names(desk->at(1)) == std::vector<std::string>{"Play Right"});
    }

    // Several lamps on one channel, in the order of the device list; the member without service is left out
    const auto tv = channelMembers(fixture(), "a0000000-0000-4000-8000-000000000002");
    expect("Several lamps on a channel", tv.has_value() && tv->size() == 1 && names(tv->at(0)) == std::vector<std::string>{"Play Left", "Lightstrip"});

    expect("Unknown area", !channelMembers(fixture(), "a0000000-0000-4000-8000-00000000000f").has_value());

    // Fields of unexpected types in other resources are skipped, not thrown
    Json odd = fixture();
    odd.at("data").push_back(Json::parse(R"({"id": "d1", "type": "device", "metadata": {"name": 7}, "services": [{"rid": "x", "rtype": 5}, {"rid": 3, "rtype": "entertainment"}, 4]})"));
    odd.at("data").push_back(Json::parse(R"({"id": "l1", "type": "light", "metadata": "x"})"));
    try{
      const auto oddMembers = channelMembers(odd, "a0000000-0000-4000-8000-000000000001");
      expect("Odd resources skipped for members", oddMembers.has_value() && oddMembers->size() == 2);
      const auto oddAreas = areas(odd);
      expect("Odd resources skipped for areas", oddAreas.has_value() && oddAreas->size() == 2);
    }
    catch(const std::exception& e){
      expect("Odd resources don't throw", false);
    }
    expect("Error answer has no members", !channelMembers(Json::parse(R"({"errors": [{"description": "x"}], "data": []})"), "a").has_value());
  }

  // Channel ids beyond a byte or with a fraction aren't wrapped or cut onto another channel
  for(const char* channelId : {"4294967297", "1.9", "256"}){
    Json odd = fixture();
    for(auto& resource : odd.at("data")){
      if(resource.value("id", "") == "a0000000-0000-4000-8000-000000000001"){
        resource.at("channels").at(1).at("channel_id") = Json::parse(channelId);
      }
    }
    const auto oddAreas = areas(odd);
    expect("Area with an odd channel id left out", oddAreas.has_value() && oddAreas->size() == 1);
  }

  // The discovery service of Philips, an answer from the internet
  {
    const auto bridges = discoveredBridges(Json::parse(R"([
      {"id": "001788fffe0a0b0c", "internalipaddress": "192.168.0.42", "port": 443},
      {"id": 5, "internalipaddress": "192.168.0.43"},
      {"internalipaddress": "192.168.0.44"},
      {"id": "001788fffe0a0b0d"},
      {"id": "001788fffe0a0b0e", "internalipaddress": ""},
      {"id": "not a bridge id", "internalipaddress": "192.168.0.45"},
      {"id": "001788fffe0a0b0f", "internalipaddress": "evil.example"},
      "garbage"
    ])"));
    expect("Discovered bridges", bridges.size() == 2);
    expect("Discovered bridge read", !bridges.empty() && bridges[0].id == "001788fffe0a0b0c" && bridges[0].address == "192.168.0.42");
    expect("Discovered bridge without id", bridges.size() == 2 && bridges[1].id.empty() && bridges[1].address == "192.168.0.44");
    expect("Discovery answer that isn't a list", discoveredBridges(Json::parse(R"({"error": "rate limited"})")).empty());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("CLIP parsing: OK");
  return 0;
}
