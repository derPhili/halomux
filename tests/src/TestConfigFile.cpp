#include <filesystem>
#include <fstream>

#include <sys/stat.h>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
namespace fs = std::filesystem;


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


  mode_t permissions(const fs::path& path)
  {
    struct stat info{};
    stat(path.c_str(), &info);
    return info.st_mode & 0777;
  }
}


int main()
{
  const fs::path root = fs::temp_directory_path() / "halomux-test-config-file";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path configFile = root / "config.json";

  // The configuration holds the bridge credentials: only its owner may read it
  {
    Core::Config config(root);
    config.setBrightness(0.5f);
  }
  expect("New configuration readable by its owner only", permissions(configFile) == 0600);

  // Files written by older versions are tightened when loaded
  fs::permissions(configFile, fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read);
  {
    Core::Config config(root);
  }
  expect("Existing configuration tightened", permissions(configFile) == 0600);

  // Hand-edited values outside what the light manager allows are brought back into range
  {
    std::ofstream file(configFile);
    file << R"({"transitionSmoothing": 1.0, "brightness": 4.0, "highlightBoost": -2.0, "subsampleWidth": 0})";
  }
  {
    Core::Config config(root);
    expect("Smoothing limited, colors keep moving", config.transitionSmoothing() <= 0.97f);
    expect("Brightness limited", config.brightness() == 1.f);
    expect("Highlight boost limited", config.highlightBoost() == 0.f);
    expect("Subsample width left automatic", config.subsampleWidth() == 0);
  }

  fs::remove_all(root);

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Config file: OK");
  return 0;
}
