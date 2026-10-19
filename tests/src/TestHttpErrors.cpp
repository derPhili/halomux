#include <future>
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
    const int value = readBody(req, [](const Json& body){return body.at("value").get<int>();});
    res.body = Json{{"value", value}}.dump();
  });
  // Such as an unexpected answer of the bridge: not the client's error, and not disclosed
  server.addRoute(HttpMethod::Get, "/internalJson", [](const Request&, Response& res){
    res.body = Json::parse(R"({"field": 1})").at("missing").dump();
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

  // DNS rebinding: a page of another name resolving to this address is refused
  const auto foreignHost = client.Get("/channel/3", httplib::Headers{{"Host", "evil.example:" + std::to_string(port)}});
  expect("Foreign Host refused", foreignHost && foreignHost->status == 403);
  const auto localHost = client.Get("/channel/3", httplib::Headers{{"Host", "localhost:" + std::to_string(port)}});
  expect("localhost Host accepted", localHost && localHost->status == 200);

  // A page of another origin may not change anything, the light manager's own page may
  const auto foreignOrigin = client.Put("/json", httplib::Headers{{"Origin", "http://evil.example"}}, R"({"value": 3})", "application/json");
  expect("Change from another origin refused", foreignOrigin && foreignOrigin->status == 403);
  const auto nullOrigin = client.Put("/json", httplib::Headers{{"Origin", "null"}}, R"({"value": 3})", "application/json");
  expect("Change from an opaque origin refused", nullOrigin && nullOrigin->status == 403);
  const auto sameOrigin = client.Put("/json", httplib::Headers{{"Origin", "http://127.0.0.1:" + std::to_string(port)}}, R"({"value": 3})", "application/json");
  expect("Change from the page itself accepted", sameOrigin && sameOrigin->status == 200);
  const auto foreignRead = client.Get("/channel/3", httplib::Headers{{"Origin", "http://evil.example"}});
  expect("Reading from another origin left to the browser's own rules", foreignRead && foreignRead->status == 200);

  // Other sites may not frame the light manager to have its buttons clicked unseen (clickjacking)
  {
    const auto framed = [](const httplib::Result& result){
      return result
        && result->get_header_value("X-Frame-Options") == "DENY"
        && result->get_header_value("Content-Security-Policy") == "frame-ancestors 'none'"
        && result->get_header_value("X-Content-Type-Options") == "nosniff";
    };
    expect("Answers can't be framed", framed(client.Get("/channel/3")));
    expect("Errors can't be framed", framed(client.Get("/conflict")));
    expect("Refused requests can't be framed", framed(foreignHost));
    expect("Unknown paths can't be framed", framed(client.Get("/unknown")));
  }

  const auto internalJson = client.Get("/internalJson");
  expect("Internal JSON error is a server error", internalJson && internalJson->status == 500 && errorShaped(internalJson));
  expect("Internal JSON error not disclosed", internalJson && internalJson->body.find("missing") == std::string::npos);

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

  // A stop right as listening starts, before httplib runs, isn't lost either
  {
    bool allEnded = true;
    for(int i = 0; i < 300 && allEnded; i++){
      auto racing = std::make_unique<HttpServer>();
      racing->addRoute(HttpMethod::Get, "/", [](const Request&, Response&){});
      if(!racing->bind("127.0.0.1", static_cast<unsigned>(port + 2))){
        continue;
      }

      auto ended = std::async(std::launch::async, [&racing](){racing->listen();});
      // Lands anywhere between the stop check and httplib running
      std::this_thread::sleep_for(std::chrono::microseconds(i % 300));
      racing->stop();
      allEnded = ended.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
      if(!allEnded){
        // Unblocked for the test to end: the stop was lost
        racing->stop();
        racing.release();
      }
    }
    expect("A stop racing the start of listening ends it", allEnded);
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("HTTP errors: OK");
  return 0;
}
