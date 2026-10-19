#include <Halomux/Platform/Adapters/GnuLinux/GnuLinuxAdapter.hpp>

#include <pwd.h>
#include <unistd.h>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Version.hpp>

#include <Halomux/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>


namespace Halomux::Platform
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

    // Take over the configuration of a former name of the application once, the most recent
    // first, keeping the original
    for(const char* legacyName : {"lightspill", "hue-desktop-sync", "huenicorn"}){
      const std::filesystem::path legacyConfigPath = configRoot / legacyName;
      if(!std::filesystem::exists(configPath) && std::filesystem::is_directory(legacyConfigPath)){
        std::filesystem::copy(legacyConfigPath, configPath, std::filesystem::copy_options::recursive);
        Core::Logger::log("Took over the configuration from ", legacyConfigPath.string());
      }
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
    // The desktop application shows the web UI in its own window. It's started as its own
    // systemd unit, so it doesn't belong to the service's control group and survives it
    const std::string desktopApplication = std::string(AppId) + "-gui";
    if(system(("command -v " + desktopApplication + " >/dev/null 2>&1").c_str()) == 0){
      if(system(("systemd-run --user --quiet --collect " + desktopApplication + " >/dev/null 2>&1").c_str()) == 0){
        return;
      }
    }

    // A browser started from here would belong to the service: confined by its hardening, and
    // ended with it. Started as a unit of its own too, directly only without systemd-run.
    if(system(("systemd-run --user --quiet --collect xdg-open " + url + " >/dev/null 2>&1").c_str()) == 0){
      return;
    }

    if(system(std::string("xdg-open " + url).c_str()) != 0){
      Core::Logger::error("Failed to open browser");
    }
  }


  SharedGrabber GnuLinuxAdapter::_createGrabber(
    Core::Config* config,
    bool askForMonitors,
    std::stop_token stopToken
  )
  {
    const char* sessionTypeEnv = std::getenv("XDG_SESSION_TYPE");
    std::string sessionType = sessionTypeEnv ? sessionTypeEnv : "";

    if(sessionType == "wayland"){
      return std::make_shared<Grabber::PipewireGrabber>(config, askForMonitors, stopToken);
    }

    throw Grabber::GrabberUnavailable(
      "Unsupported session type: " + sessionType
    );
  }
}
