#include <filesystem>
#include <fstream>

#include <csignal>

#include <sys/resource.h>
#include <sys/stat.h>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>

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

  {
    // A verified certificate binds its bridge: another bridge's id starts unverified again
    Core::Config config(root);
    config.setBridgeIdentity("aabbccddeeff0011", true);
    config.setBridgeIdentity("aabbccddeeff0011", false);
    expect("Same bridge stays verified", config.bridgeCertificateVerified());
    config.setBridgeIdentity("AABBCCDDEEFF0011", false);
    expect("Same bridge in other case stays verified", config.bridgeCertificateVerified());
    config.setBridgeIdentity("0011aabbccddeeff", false);
    expect("Another bridge isn't verified", !config.bridgeCertificateVerified());
  }

  {
    // A full disk while saving: the configuration on disk, with the credentials, stays as it was
    {
      Core::Config config(root);
      config.setCredentials(Hue::Auth::Credentials("user", "00112233445566778899aabbccddeeff"));
    }

    std::signal(SIGXFSZ, SIG_IGN);
    rlimit previous{};
    getrlimit(RLIMIT_FSIZE, &previous);
    {
      Core::Config config(root);
      rlimit limited = previous;
      limited.rlim_cur = 64;
      setrlimit(RLIMIT_FSIZE, &limited);
      config.setBrightness(0.25f);
      setrlimit(RLIMIT_FSIZE, &previous);
    }

    Core::Config config(root);
    expect("Failed save keeps the credentials", config.credentials().has_value() && config.credentials()->username() == "user");
    expect("Failed save keeps the previous values", config.brightness() != 0.25f);
  }

  fs::remove_all(root);

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Config file: OK");
  return 0;
}
