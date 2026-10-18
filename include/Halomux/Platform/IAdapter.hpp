#pragma once

#include <filesystem>
#include <memory>
#include <stop_token>

#include <Halomux/Grabber/IGrabber.hpp>
#include <Halomux/Core/Logger.hpp>


namespace Halomux::Core
{
  class Config;
}


namespace Halomux::Platform
{
  using SharedGrabber = std::shared_ptr<Grabber::IGrabber>;

  /**
   * @brief Wrapper interface for platform-specific functions and grabber selection
   * 
   */
  class IAdapter
  {
  public:
    /**
     * @brief Constructor
     * 
     */
    IAdapter(
      const std::string& platformName
    ):
    m_platformName(platformName)
    {}


    /**
     * @brief Destructor
     * 
     */
    virtual ~IAdapter(){}


    /**
     * @brief Getter for the platform name
     * 
     */
    const std::string& getPlatformName() const
    {
      return m_platformName;
    }


    /**
     * @brief Getter for config file configuration path
     * 
     * @return std::filesystem::path Path to config path
     */
    virtual std::filesystem::path getConfigFilePath() const = 0;

    /**
     * @brief Getter for OS username
     * 
     * @return std::string OS username
     */
    virtual std::string getUsername() const = 0;


    /**
     * @brief Shows the web UI: in the desktop application if installed, else in the default web browser
     * 
     * @param url Page to open in web browser
     */
    virtual void openWebBrowser(
      const std::string& url
    ) const = 0;


    /**
     * @brief Starts a grabber that restores the saved screen selection, or asks for one
     *
     * The caller owns the grabber: it must be destroyed before the configuration it was given,
     * and before leaving main(), as its teardown relies on other static objects.
     *
     * @param config Halomux current configuration
     * @param stopToken Aborts a screen selection dialog, if the saved selection can't be restored
     * @return SharedGrabber Grabber instance, nullptr if none could be started
     */
    inline SharedGrabber getGrabber(
      Core::Config* config,
      std::stop_token stopToken = {}
    )
    {
      try{
        return _createGrabber(config, false, stopToken);
      }
      catch(const Grabber::GrabberCancelled& e){
        Core::Logger::warn(e.what());
        Core::Logger::warn("Now quitting");
      }
      catch(const std::exception& e){
        Core::Logger::error(e.what());
      }

      return nullptr;
    }


    /**
     * @brief Starts a grabber that asks for the monitors to capture
     *
     * The current grabber keeps running meanwhile: its owner replaces it with the returned one.
     *
     * @param config Halomux current configuration
     * @param stopToken Aborts the screen selection
     * @return SharedGrabber New grabber, nullptr if the selection was cancelled or failed
     */
    inline SharedGrabber replaceGrabber(
      Core::Config* config,
      std::stop_token stopToken
    )
    {
      try{
        return _createGrabber(config, true, stopToken);
      }
      catch(const Grabber::GrabberCancelled&){
        Core::Logger::warn("Screen selection cancelled, keeping the current monitors");
      }
      catch(const std::exception& e){
        Core::Logger::error(e.what());
      }

      return nullptr;
    }

  protected:
    /**
     * @brief Factory method for grabber
     * 
     * @param config Halomux current configuration
     * @param askForMonitors Whether to ask for the monitors instead of restoring the saved selection
     * @param stopToken Aborts the screen selection
     * @return SharedGrabber New grabber
     */
    virtual SharedGrabber _createGrabber(
      Core::Config* config,
      bool askForMonitors,
      std::stop_token stopToken = {}
    ) = 0;

    // Attributes
    const std::string m_platformName;
  };
}
