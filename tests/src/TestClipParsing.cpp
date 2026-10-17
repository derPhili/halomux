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
    const auto paired = pairing(Json::parse(R"([{"success": {"username": "u", "clientkey": "K"}}])"));
    expect("Paired", paired.credentials.has_value() && paired.credentials->at("username") == "u");

    const auto notPressed = pairing(Json::parse(R"([{"error": {"type": 101, "description": "link button not pressed"}}])"));
    expect("Link button not pressed", !notPressed.credentials.has_value() && notPressed.error == "link button not pressed");

    expect("Not an array", !pairing(Json::parse(R"({"error": "x"})")).credentials.has_value());
    expect("Empty array", !pairing(Json::parse("[]")).credentials.has_value());
    expect("Success without credentials", !pairing(Json::parse(R"([{"success": {"username": "u"}}])")).credentials.has_value());
    expect("Error without description", !pairing(Json::parse(R"([{"error": {}}])")).error.empty());
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("CLIP parsing: OK");
  return 0;
}
