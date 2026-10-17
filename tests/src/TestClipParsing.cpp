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

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("CLIP parsing: OK");
  return 0;
}
