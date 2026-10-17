#include <chrono>
#include <memory>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Stream/Streamer.hpp>

using namespace Halomux;
using namespace std::chrono_literals;


/**
 * @brief Checks that streaming never waits for the connection to the bridge
 *
 * The streamer targets an address that never answers (TEST-NET-1), as a bridge busy with
 * another application would: handshakes time out after seconds, which must not hold the
 * streaming loop.
 */
int main()
{
  using Clock = std::chrono::steady_clock;

  const Hue::Auth::Credentials credentials{"test-user", "00112233445566778899aabbccddeeff"};
  auto streamer = std::make_unique<Stream::Streamer>(credentials, "192.0.2.1");
  streamer->setEntertainmentConfigurationId("00000000-0000-0000-0000-000000000000");

  const Hue::Api::ChannelStreams channels = {{.id = 0, .r = 0.3f, .g = 0.3f, .b = 0.5f}, {.id = 1, .r = 0.3f, .g = 0.3f, .b = 0.5f}};

  // Stream at 50 Hz for 3 seconds, while the first handshake runs
  Clock::duration longestFrame{};
  const auto start = Clock::now();
  while(Clock::now() - start < 3s){
    const auto frameStart = Clock::now();
    streamer->streamChannels(channels);
    longestFrame = std::max(longestFrame, Clock::now() - frameStart);
    std::this_thread::sleep_for(20ms);
  }

  const auto destructionStart = Clock::now();
  streamer.reset();
  const auto destruction = Clock::now() - destructionStart;

  const auto toMs = [](Clock::duration duration){return std::chrono::duration<float, std::milli>(duration).count();};
  Core::Logger::log("Longest frame: ", toMs(longestFrame), " ms, destruction: ", toMs(destruction), " ms");

  int failures = 0;

  // A blocking handshake used to take seconds
  if(longestFrame > 5ms){
    Core::Logger::error("Streaming waited for the connection");
    failures++;
  }

  // Waits at most for the running handshake attempt, about 2 s
  if(destruction > 3s){
    Core::Logger::error("Destruction took too long");
    failures++;
  }

  return failures == 0 ? 0 : 1;
}
