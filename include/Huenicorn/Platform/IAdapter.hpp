#pragma once

#include <filesystem>
#include <memory>
#include <stop_token>

#include <Huenicorn/Grabber/IGrabber.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Core
{
  class Config;
}


namespace Huenicorn::Platform
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
     * @brief Destroys the grabber, if any
     *
     * Must be called before leaving main(): the grabber's teardown relies on
     * other static objects that would otherwise be destroyed first.
     */
    inline void releaseGrabber()
    {
      m_grabber.reset();
    }


    /**
     * @brief Getter to instanciate / store valid grabber
     * 
     * @param config Huenicorn current configuration
     * @return SharedGrabber Grabber instance, nullptr if none could be started
     */
    inline SharedGrabber getGrabber(
      Core::Config* config
    )
    {
      if(!m_grabber){
        try{
          m_grabber = _createGrabber(config, false);
        }
        catch(const Grabber::GrabberCancelled& e){
          Core::Logger::warn(e.what());
          Core::Logger::warn("Now quitting");
        }
        catch(const std::exception& e){
          Core::Logger::error(e.what());
        }
      }

      return m_grabber;
    }


    /**
     * @brief Starts a grabber that asks for the monitors to capture, and keeps it on success
     *
     * The current grabber keeps running meanwhile: its owners replace it with the returned one.
     *
     * @param config Huenicorn current configuration
     * @param stopToken Aborts the screen selection
     * @return SharedGrabber New grabber, nullptr if the selection was cancelled or failed
     */
    inline SharedGrabber replaceGrabber(
      Core::Config* config,
      std::stop_token stopToken
    )
    {
      try{
        auto grabber = _createGrabber(config, true, stopToken);
        m_grabber = grabber;
        return grabber;
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
     * @param config Huenicorn current configuration
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
    SharedGrabber m_grabber;
  };
}
