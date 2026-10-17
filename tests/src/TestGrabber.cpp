#include <chrono>
#include <filesystem>
#include <thread>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Platform/Selector.hpp>

using namespace Halomux;
using namespace Halomux::Core;
using namespace std::chrono_literals;


/**
 * @brief Captures the screen for a few seconds and prints the average colors of a few regions
 *
 * Usage: TestGrabber [seconds]
 */
int main(int argc, char* argv[])
{
  const int seconds = argc >= 2 ? std::atoi(argv[1]) : 10;

  Config config(std::filesystem::temp_directory_path() / "halomux-test");
  config.setSubsampleWidth(160);

  auto grabber = Platform::adapter.getGrabber(&config);
  if(!grabber){
    Logger::error("Could not start any grabber");
    return 1;
  }

  const auto resolution = grabber->displayResolution();
  Logger::log("Started ", grabber->name(), " on a ", resolution.x, "x", resolution.y, " display at ", grabber->displayRefreshRate(), " Hz");

  for(const auto& monitor : grabber->monitors()){
    Logger::log("Monitor ", monitor.description, " ", monitor.geometry.x, ",", monitor.geometry.y, " ", monitor.geometry.width, "x", monitor.geometry.height);
  }

  // On the first monitor, and the whole second one if several are captured
  Grabber::IGrabber::ChannelRegions regions{
    {0, 0, {{0.f, 0.f}, {1.f, 1.f}}},   // Whole screen
    {1, 0, {{0.f, 0.f}, {0.5f, 1.f}}},  // Left half
    {2, 0, {{0.5f, 0.f}, {1.f, 1.f}}}   // Right half
  };
  if(grabber->monitors().size() > 1){
    regions.push_back({3, 1, {{0.f, 0.f}, {1.f, 1.f}}});
  }
  grabber->setChannelRegions(regions);

  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while(std::chrono::steady_clock::now() < end){
    std::this_thread::sleep_for(500ms);

    Grabber::IGrabber::ChannelColors colors;
    if(!grabber->grabChannelColors(colors)){
      Logger::log("No frame yet");
      continue;
    }

    std::ostringstream line;
    for(const auto& [channelId, color] : colors){
      line << "[" << static_cast<int>(channelId) << "] " << static_cast<int>(color.m_r) << " " << static_cast<int>(color.m_g) << " " << static_cast<int>(color.m_b) << "   ";
    }
    Logger::log(line.str());
  }

  Platform::adapter.releaseGrabber();
  Logger::log("Done");

  return 0;
}
