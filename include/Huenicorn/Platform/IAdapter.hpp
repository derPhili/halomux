#pragma once

#include <filesystem>
#include <memory>

#include <Huenicorn/Grabber/IGrabber.hpp>
#include <Huenicorn/Grabber/DummyGrabber.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Core
{
  class Config;
}


namespace Huenicorn::Platform
{
  using UniqueGrabber = std::unique_ptr<Grabber::IGrabber>;

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
     * @brief Method to open default web browser at given URL
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
     * @return UniqueGrabber Grabber instance
     */
    inline Grabber::IGrabber* getGrabber(
      Core::Config* config
    )
    {
      if(!m_grabber){
        try{
          m_grabber = _createGrabber(config);
        }
        catch(const Grabber::GrabberUnavailable& e){
          // Fallback to DummyGrabber
          Core::Logger::warn(e.what());
          Core::Logger::warn("Could not start propper grabber. Now falling back to DummyGrabber.");

          m_grabber = std::make_unique<Grabber::DummyGrabber>(config);
        }
        catch(const Grabber::GrabberCancelled& e){
          Core::Logger::warn(e.what());
          Core::Logger::warn("Now quitting");
          m_grabber.reset();
        }
        catch(const std::exception& e){
          // init() may have thrown after the grabber was created: don't hand it out half initialized
          Core::Logger::warn(e.what());
          m_grabber.reset();
        }
      }

      return m_grabber.get();
    }


  protected:
    /**
     * @brief Factory method for grabber
     * 
     * @param config Huenicorn current configuration
     * @return UniqueGrabber New grabber
     */
    virtual UniqueGrabber _createGrabber(
      Core::Config* config
    ) = 0;

    // Attributes
    const std::string m_platformName;
    UniqueGrabber m_grabber;
  };
}
