#include <Huenicorn/Platform/Adapters/GnuLinux/GnuLinuxAdapter.hpp>

#include <pwd.h>
#include <unistd.h>

#include <Huenicorn/Core/Logger.hpp>

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

    return std::filesystem::path(homeDir) / ".config/huenicorn";
  }


  std::string GnuLinuxAdapter::getUsername() const{
    return std::string(getlogin());
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

    // Gamescope (Steam Deck / Bazzite Game Mode) sets XDG_SESSION_TYPE=x11
    // for its children (for legacy X11 game compatibility) and doesn't run
    // an xdg-desktop-portal ScreenCast backend, so both the usual X11 grab
    // (of the nested Xwayland root, which gamescope doesn't render games
    // into) and the usual portal-based Wayland capture black-screen there.
    // GAMESCOPE_WAYLAND_DISPLAY is gamescope's own marker for this and takes
    // priority over XDG_SESSION_TYPE: capture gamescope's Pipewire node
    // directly instead.
    if(std::getenv("GAMESCOPE_WAYLAND_DISPLAY") != nullptr){
      return std::make_unique<Grabber::PipewireGrabber>(config, true);
    }

    if(sessionType == "wayland"){
      return std::make_unique<Grabber::PipewireGrabber>(config, false);
    }

    throw Grabber::GrabberUnavailable(
      "Unsupported session type: " + sessionType
    );
  }
}
