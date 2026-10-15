#include <Huenicorn/Platform/Adapters/GnuLinux/GnuLinuxAdapter.hpp>

#include <pwd.h>
#include <unistd.h>

#include <Huenicorn/Core/Logger.hpp>
#include <Huenicorn/Version.hpp>

#include <Huenicorn/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>


namespace Huenicorn::Platform
{
  std::filesystem::path GnuLinuxAdapter::getConfigFilePath() const
  {
    const char* homeDir = getenv("HOME");

    if(!homeDir){
      homeDir = getpwuid(getuid())->pw_dir;
    }

    if(!homeDir){
      throw std::runtime_error("Failed to get the current user name");
    }

    const std::filesystem::path configRoot = std::filesystem::path(homeDir) / ".config";
    const std::filesystem::path configPath = configRoot / AppId;

    // Take over the configuration of the application's former name once, keeping the original
    const std::filesystem::path legacyConfigPath = configRoot / "huenicorn";
    if(!std::filesystem::exists(configPath) && std::filesystem::is_directory(legacyConfigPath)){
      std::filesystem::copy(legacyConfigPath, configPath, std::filesystem::copy_options::recursive);
      Core::Logger::log("Took over the configuration from ", legacyConfigPath.string());
    }

    return configPath;
  }


  std::string GnuLinuxAdapter::getUsername() const{
    // getlogin() needs a controlling terminal, which a systemd service doesn't have
    if(const char* login = getlogin()){
      return login;
    }

    if(const passwd* user = getpwuid(getuid())){
      return user->pw_name;
    }

    return "user";
  }


  void GnuLinuxAdapter::openWebBrowser(
    const std::string& url
  ) const
  {
    if(system(std::string("xdg-open " + url).c_str()) != 0){
      Core::Logger::error("Failed to open browser");
    }
  }


  UniqueGrabber GnuLinuxAdapter::_createGrabber(
    Core::Config* config
  )
  {
    const char* sessionTypeEnv = std::getenv("XDG_SESSION_TYPE");
    std::string sessionType = sessionTypeEnv ? sessionTypeEnv : "";

    if(sessionType == "wayland"){
      return std::make_unique<Grabber::PipewireGrabber>(config);
    }

    throw Grabber::GrabberUnavailable(
      "Unsupported session type: " + sessionType
    );
  }
}
