#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <Halomux/Core/AreaSession.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
using namespace std::chrono_literals;


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


  struct FakeBridge : Hue::Api::BridgeApi
  {
    bool reachable{true};
    Hue::Api::EntertainmentConfigurations configuredAreas;
    std::optional<bool> status{true};
    bool acceptStart{true};
    std::vector<std::string> calls;

    std::optional<Hue::Api::EntertainmentConfigurations> areas() override
    {
      calls.push_back("areas");
      if(!reachable){
        return std::nullopt;
      }
      return configuredAreas;
    }

    std::optional<Hue::Api::ChannelMembers> channelMembers(const std::string& areaId) override
    {
      calls.push_back("members " + areaId);
      if(!reachable || !configuredAreas.contains(areaId)){
        return std::nullopt;
      }
      return Hue::Api::ChannelMembers{{0, {}}};
    }

    bool setStreaming(const Hue::Api::EntertainmentConfigurationEntry& area, bool active) override
    {
      calls.push_back((active ? "start " : "stop ") + area.first);
      return reachable && (!active || acceptStart);
    }

    bool setLightsOn(const Hue::Api::Devices& lights, bool on) override
    {
      for(const auto& light : lights){
        calls.push_back((on ? "light on " : "light off ") + light.id);
      }
      return reachable;
    }

    std::optional<bool> streamingActive(const std::string& areaId) override
    {
      calls.push_back("status " + areaId);
      if(!reachable){
        return std::nullopt;
      }
      return status;
    }

    bool called(const std::string& call) const
    {
      return std::find(calls.begin(), calls.end(), call) != calls.end();
    }
  };


  struct FakeStreamer : Stream::IStreamer
  {
    int* reconnects;
    // Like a Streamer waiting for a DTLS handshake attempt to end
    std::chrono::milliseconds teardown{0};
    explicit FakeStreamer(int* reconnects, std::chrono::milliseconds teardown = 0ms) : reconnects(reconnects), teardown(teardown) {}
    ~FakeStreamer() override {std::this_thread::sleep_for(teardown);}
    void streamChannels(const Hue::Api::ChannelStreams&) override {}
    void reconnect() override {(*reconnects)++;}
  };


  struct Harness
  {
    FakeBridge* bridge;
    std::vector<std::string> streamersCreated;
    std::vector<std::string> selected;
    int lost{0};
    int reconnects{0};
    std::chrono::milliseconds streamerTeardown{0};
    std::unique_ptr<Core::AreaSession> session;

    explicit Harness(Hue::Api::EntertainmentConfigurations areas = {{"pc", {.name = "PC"}}, {"garage", {.name = "Garage"}}})
    {
      auto fake = std::make_unique<FakeBridge>();
      fake->configuredAreas = std::move(areas);
      bridge = fake.get();

      session = std::make_unique<Core::AreaSession>(
        std::move(fake),
        [this](const std::string& areaId){
          streamersCreated.push_back(areaId);
          return std::make_unique<FakeStreamer>(&reconnects, streamerTeardown);
        },
        [this](const std::string& areaId, const Hue::Api::ChannelMembers&){selected.push_back(areaId);},
        [this](){lost++;},
        0ms // No watchdog thread: tick() is called by the test
      );
    }
  };
}


int main()
{
  // The saved area is selected and streamed to
  {
    Harness h;
    h.session->start("garage");
    expect("Saved area selected", h.session->currentAreaId() == "garage");
    expect("Streaming started", h.bridge->called("start garage"));
    expect("Streamer created for it", h.streamersCreated == std::vector<std::string>{"garage"});
    expect("Lamps loaded once", h.selected == std::vector<std::string>{"garage"});
  }

  // The saved area was deleted in the Hue app: the first area instead
  {
    Harness h({{"pc", {.name = "PC"}}});
    h.session->start("deleted");
    expect("Fallback to an existing area", h.session->currentAreaId() == "pc");
  }

  // Switching areas
  {
    Harness h;
    h.session->start("pc");
    expect("Switch accepted", h.session->select("garage"));
    expect("Former area stopped", h.bridge->called("stop pc"));
    expect("New area started", h.bridge->called("start garage"));
    expect("Now on the new area", h.session->currentAreaId() == "garage");

    expect("Unknown area refused", !h.session->select("cellar"));
    expect("Unknown area changes nothing", h.session->currentAreaId() == "garage" && !h.bridge->called("stop garage"));
  }

  // An area created in the Hue app after the start
  {
    Harness h;
    h.session->start("pc");
    h.bridge->configuredAreas["cellar"] = {.name = "Cellar"};
    expect("New area selectable", h.session->select("cellar"));
    expect("New area listed", h.session->areas().contains("cellar"));
  }

  // Lamps that can't be loaded leave the current area untouched
  {
    Harness h;
    h.session->start("pc");
    h.bridge->reachable = false;
    expect("Switch refused while the bridge is unreachable", !h.session->select("garage"));
    expect("Current area kept", h.session->currentAreaId() == "pc");
    expect("Current area not stopped", !h.bridge->called("stop pc"));
  }

  // Lamps switched off: no stream, the lamps off on the bridge, and the checks don't restart it
  {
    Harness h({{"pc", {.name = "PC", .devices = {{.id = "light-1", .name = "Play"}, {.id = "light-2", .name = "Strip"}}}}});
    h.session->start("pc");
    h.bridge->calls.clear();

    expect("Lamps switched off", h.session->setLightsOn(false));
    expect("Stream stopped", h.bridge->called("stop pc"));
    expect("Each lamp off", h.bridge->called("light off light-1") && h.bridge->called("light off light-2"));
    expect("Area kept", h.session->currentAreaId() == "pc");

    h.bridge->calls.clear();
    h.bridge->status = false;
    h.session->tick();
    expect("Checks don't start the stream while off", !h.bridge->called("start pc"));

    const size_t streamers = h.streamersCreated.size();
    expect("Lamps switched on", h.session->setLightsOn(true));
    expect("Each lamp on", h.bridge->called("light on light-1") && h.bridge->called("light on light-2"));
    expect("Stream started again", h.bridge->called("start pc") && h.streamersCreated.size() == streamers + 1);
  }

  // Off at the start, such as switched off before the service restarted: nothing streamed, and
  // the lamps left as they are, which the Hue app may have switched on since
  {
    Harness h({{"pc", {.name = "PC", .devices = {{.id = "light-1", .name = "Play"}}}}});
    h.session->setLightsOn(false);
    h.session->start("pc");
    expect("Area selected while off", h.session->currentAreaId() == "pc");
    expect("No stream while off", !h.bridge->called("start pc") && h.streamersCreated.empty());
    expect("Lamps left alone at the start", !h.bridge->called("light off light-1"));

    // Switching to another area while off: its lamps go off too
    h.bridge->configuredAreas.insert({"garage", {.name = "Garage", .devices = {{.id = "light-9", .name = "Garage"}}}});
    expect("Switched while off", h.session->select("garage"));
    expect("Other area's lamps off", h.bridge->called("light off light-9") && !h.bridge->called("start garage"));
  }

  // Bridge offline at startup: the session keeps trying, and runs the check hook each time
  {
    Harness h;
    int hooked = 0;
    h.session->setCheckHook([&hooked](){hooked++;});
    h.bridge->reachable = false;
    h.session->start("pc");
    expect("Nothing selected while offline", !h.session->currentAreaId().has_value());
    h.session->tick();
    expect("Still nothing while offline", !h.session->currentAreaId().has_value());
    h.bridge->reachable = true;
    h.session->tick();
    expect("Selected once the bridge answers", h.session->currentAreaId() == "pc");
    expect("Streaming started then", h.bridge->called("start pc"));
    expect("Check hook run at each check", hooked == 2);
  }

  // No area yet: created later in the Hue app
  {
    Harness h({});
    h.session->start("");
    expect("Nothing to select", !h.session->currentAreaId().has_value());
    h.bridge->configuredAreas = {{"pc", {.name = "PC"}}};
    h.session->tick();
    expect("New area selected", h.session->currentAreaId() == "pc");
  }

  // The bridge stopped the stream, such as after a reboot: started again
  {
    Harness h;
    h.session->start("pc");
    h.bridge->calls.clear();
    h.bridge->status = false;
    h.session->tick();
    expect("Streaming started again", h.bridge->called("start pc"));
    expect("Connection renewed", h.reconnects == 1);

    h.bridge->status = true;
    h.bridge->calls.clear();
    h.session->tick();
    expect("Nothing done while streaming", !h.bridge->called("start pc") && h.reconnects == 1);

    // Unknown status, such as an unreachable bridge: wait instead of hammering it
    h.bridge->status = std::nullopt;
    h.session->tick();
    expect("Nothing done while the status is unknown", !h.bridge->called("start pc") && h.reconnects == 1);
  }

  // Shutdown stops the streaming
  {
    Harness h;
    h.session->start("pc");
    h.session->stop();
    expect("Streaming stopped", h.bridge->called("stop pc"));
    expect("Nothing selected after stopping", !h.session->currentAreaId().has_value());
  }

  // The areas, for the light manager
  {
    Harness h;
    h.session->start("pc");
    expect("Areas listed", h.session->areas().size() == 2);
  }

  // An area created in the Hue app shows up when the light manager lists them again
  {
    Harness h;
    h.session->start("pc");
    h.bridge->configuredAreas.insert({"cellar", {.name = "Cellar"}});
    expect("New area listed after a refresh", h.session->refreshAreas().contains("cellar") && h.session->areas().contains("cellar"));

    h.bridge->reachable = false;
    expect("Unreachable bridge keeps the known areas", h.session->refreshAreas().size() == 3);
  }

  // The current area deleted in the Hue app: another one is streamed to
  {
    Harness h;
    h.session->start("garage");
    h.bridge->configuredAreas.erase("garage");
    h.bridge->status = std::nullopt; // The bridge answers an error for an unknown area
    h.session->tick();
    expect("Deleted area replaced", h.session->currentAreaId() == "pc");
    expect("Lamps of the other area loaded", !h.selected.empty() && h.selected.back() == "pc");
    expect("Streamer for the other area", !h.streamersCreated.empty() && h.streamersCreated.back() == "pc");
  }

  // The only area deleted in the Hue app: its lamps are dropped, not kept as if it still existed
  {
    Harness h({{"garage", {.name = "Garage"}}});
    h.session->start("garage");
    h.bridge->configuredAreas.clear();
    h.bridge->status = std::nullopt;
    h.session->tick();
    expect("No area left", !h.session->currentAreaId().has_value());
    expect("Lost area reported", h.lost == 1);
  }

  // Replacing a streamer that takes long to end doesn't hold the streaming loop
  {
    Harness h;
    h.streamerTeardown = 600ms;
    h.session->start("pc");

    std::thread switcher([&](){h.session->select("garage");});
    std::this_thread::sleep_for(100ms);
    const auto before = std::chrono::steady_clock::now();
    h.session->stream({});
    const auto waited = std::chrono::steady_clock::now() - before;
    switcher.join();

    expect("Streaming not held by the former streamer's end", waited < 200ms);
    h.streamerTeardown = 0ms;
  }

  // An unreachable bridge is no deleted area
  {
    Harness h;
    h.session->start("garage");
    h.bridge->reachable = false;
    h.session->tick();
    expect("Unreachable bridge keeps the area", h.session->currentAreaId() == "garage");
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Area session: OK");
  return 0;
}
