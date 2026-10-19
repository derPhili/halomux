#include <filesystem>
#include <fstream>
#include <limits>

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

  // A configuration without profile name, such as one written again after deleting it: the
  // profile is still the one saved, read and kept, not replaced
  {
    std::ofstream file(configFile);
    file << R"({"brightness": 0.5})";
  }
  {
    Core::Config config(root);
    expect("Profile name by default", config.profileName() == "profile");
  }
  fs::remove(configFile);
  {
    Core::Config config(root);
    expect("Profile name of a new configuration", config.profileName() == "profile");
  }

  // Addresses saved by former versions, which kept the port: as the setup saves them now
  {
    std::ofstream file(configFile);
    file << R"({"bridgeAddress": "192.168.0.42:443"})";
  }
  {
    Core::Config config(root);
    expect("Saved address without its port", config.bridgeAddress() == "192.168.0.42");
  }

  // A negative width reads as a huge unsigned one: limited like what the setter takes
  {
    std::ofstream file(configFile);
    file << R"({"subsampleWidth": -1})";
  }
  {
    Core::Config config(root);
    expect("Huge subsample width limited", config.subsampleWidth() == Core::Config::MaxSubsampleWidth);
    config.setSubsampleWidth(std::numeric_limits<unsigned>::max());
    expect("Huge subsample width set limited", config.subsampleWidth() == Core::Config::MaxSubsampleWidth);
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
    // The setup takes over a bridge: credentials of another bridge don't go with it
    Core::Config config(root);
    config.setBridge("192.168.0.42", "aabbccddeeff0011", true);
    config.setCredentials(Hue::Auth::Credentials("user", "00112233445566778899aabbccddeeff"));
    config.setBridge("192.168.0.42", "AABBCCDDEEFF0011", false);
    expect("Same bridge keeps its credentials", config.credentials().has_value());
    expect("Same bridge stays verified when taken over again", config.bridgeCertificateVerified());
    config.setBridge("192.168.0.43", "0011aabbccddeeff", false);
    expect("Another bridge drops the credentials", !config.credentials().has_value());
    expect("Another bridge's address saved", config.bridgeAddress() == "192.168.0.43");
    expect("Another bridge isn't verified", !config.bridgeCertificateVerified());
    config.setCredentials(Hue::Auth::Credentials("user", "00112233445566778899aabbccddeeff"));
    config.setBridge("192.168.0.44", "0011aabbccddeeff", false);
    expect("Same bridge at another address drops the credentials", !config.credentials().has_value());

    // The public key of a bridge whose certificate isn't signed by the CA, pinned from then on
    config.setBridge("192.168.0.45", "0011aabbccddeeff", false, "sha256//key");
    expect("Public key saved", config.bridgePublicKey() == "sha256//key");
    config.setBridge("192.168.0.46", "0011aabbccddee00", false);
    expect("Another bridge has no key yet", !config.bridgePublicKey().has_value());
    config.setBridgePublicKey("sha256//other");
    expect("Public key learnt later", config.bridgePublicKey() == "sha256//other");

    // Whether a bridge would be verified, before it's taken over
    config.setBridge("192.168.0.42", "aabbccddeeff0011", true);
    expect("Verified bridge again", config.bridgeVerifiedWith("AABBCCDDEEFF0011", false));
    expect("Other bridge unverified", !config.bridgeVerifiedWith("0011aabbccddeeff", false));
    expect("Other bridge verified by its certificate", config.bridgeVerifiedWith("0011aabbccddeeff", true));
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
