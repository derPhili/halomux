#include <chrono>
#include <thread>

#include <httplib.h>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Network/Http/Server/HttpServer.hpp>
#include <Halomux/Network/Http/Server/RouteTools.hpp>
#include <Halomux/Serialization/Json.hpp>

using namespace Halomux;
using namespace Halomux::Network::Http::Server;
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


  bool errorShaped(const httplib::Result& result)
  {
    try{
      const Json body = Json::parse(result->body);
      return body.at("succeeded") == false && body.at("error").is_string();
    }
    catch(const std::exception&){
      return false;
    }
  }
}


int main()
{
  // Channel ids in paths: whole numbers from 0 to 255 only
  expect("Channel id", parseChannelId("7") == 7);
  expect("Highest channel id", parseChannelId("255") == 255);
  expect("Channel id out of range", !parseChannelId("256").has_value());
  expect("Negative channel id", !parseChannelId("-1").has_value());
  expect("Trailing garbage", !parseChannelId("1x").has_value());
  expect("Not a number", !parseChannelId("abc").has_value());
  expect("Empty", !parseChannelId("").has_value());
  expect("Leading space", !parseChannelId(" 1").has_value());

  // Corners of a region
  expect("Corner", parseUVCorner(3) == Imaging::UVCorner::BottomRight);
  expect("Unknown corner", !parseUVCorner(4).has_value() && !parseUVCorner(-1).has_value());

  // Errors of the handlers become answers of the same shape
  HttpServer server;
  server.addRoute(HttpMethod::Put, "/json", [](const Request& req, Response& res){
    res.body = Json{{"value", Json::parse(req.body).at("value").get<int>()}}.dump();
  });
  server.addRoute(HttpMethod::Get, "/channel/:channelId", [](const Request& req, Response& res){
    res.body = Json{{"channelId", channelIdOf(req)}}.dump();
  });
  server.addRoute(HttpMethod::Get, "/conflict", [](const Request&, Response&){
    throw HttpError(409, "not now");
  });
  server.addRoute(HttpMethod::Get, "/bug", [](const Request&, Response&){
    throw std::runtime_error("internal detail");
  });

  const int port = 18000 + static_cast<int>(getpid() % 1000);
  expect("Bound", server.bind("127.0.0.1", static_cast<unsigned>(port)));
  std::thread listener([&server](){server.listen();});

  httplib::Client client("127.0.0.1", port);
  client.set_connection_timeout(2);

  // Wait for the listener
  for(int i = 0; i < 100 && !client.Get("/channel/1"); i++){
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  const auto ok = client.Put("/json", R"({"value": 3})", "application/json");
  expect("Valid request", ok && ok->status == 200);

  const auto malformed = client.Put("/json", "{not json", "application/json");
  expect("Malformed JSON is a bad request", malformed && malformed->status == 400 && errorShaped(malformed));

  const auto missing = client.Put("/json", R"({"other": 3})", "application/json");
  expect("Missing field is a bad request", missing && missing->status == 400 && errorShaped(missing));

  const auto wrongType = client.Put("/json", R"({"value": "x"})", "application/json");
  expect("Wrong type is a bad request", wrongType && wrongType->status == 400);

  const auto channel = client.Get("/channel/12");
  expect("Channel id read", channel && channel->status == 200 && Json::parse(channel->body).at("channelId") == 12);

  const auto wrapped = client.Get("/channel/256");
  expect("Channel 256 isn't channel 0", wrapped && wrapped->status == 404 && errorShaped(wrapped));

  const auto conflict = client.Get("/conflict");
  expect("Chosen status kept", conflict && conflict->status == 409 && errorShaped(conflict));

  const auto bug = client.Get("/bug");
  expect("Unexpected error is a server error", bug && bug->status == 500 && errorShaped(bug));
  expect("Internal detail not leaked", bug && bug->body.find("internal detail") == std::string::npos);

  server.stop();
  listener.join();

  // A stop requested before the server is bound, such as SIGTERM during startup, isn't lost
  {
    HttpServer early;
    early.addRoute(HttpMethod::Get, "/", [](const Request&, Response&){});
    early.stop();
    expect("Bound after an early stop", early.bind("127.0.0.1", static_cast<unsigned>(port + 1)));

    const auto start = std::chrono::steady_clock::now();
    std::thread earlyListener([&early](){early.listen();});
    earlyListener.join();
    expect("Listening ends at once after an early stop", std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("HTTP errors: OK");
  return 0;
}
