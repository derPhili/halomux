#include <algorithm>
#include <memory>
#include <string>
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
    explicit FakeStreamer(int* reconnects) : reconnects(reconnects) {}
    void streamChannels(const Hue::Api::ChannelStreams&) override {}
    void reconnect() override {(*reconnects)++;}
  };


  struct Harness
  {
    FakeBridge* bridge;
    std::vector<std::string> streamersCreated;
    std::vector<std::string> selected;
    int reconnects{0};
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
          return std::make_unique<FakeStreamer>(&reconnects);
        },
        [this](const std::string& areaId, const Hue::Api::ChannelMembers&){selected.push_back(areaId);},
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

  // Bridge offline at startup: the session keeps trying
  {
    Harness h;
    h.bridge->reachable = false;
    h.session->start("pc");
    expect("Nothing selected while offline", !h.session->currentAreaId().has_value());
    h.session->tick();
    expect("Still nothing while offline", !h.session->currentAreaId().has_value());
    h.bridge->reachable = true;
    h.session->tick();
    expect("Selected once the bridge answers", h.session->currentAreaId() == "pc");
    expect("Streaming started then", h.bridge->called("start pc"));
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

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Area session: OK");
  return 0;
}
