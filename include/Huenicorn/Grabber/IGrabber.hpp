#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include <Huenicorn/Imaging/Color.hpp>
#include <Huenicorn/Imaging/ImageData.hpp>
#include <Huenicorn/Imaging/UV.hpp>
#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Core
{
  class Config;
}

namespace Huenicorn::Grabber
{
  class GrabberUnavailable : public std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };


  class GrabberCancelled : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };


  /**
   * @brief Abstract class to implement for screen capture
   * 
   */
  class IGrabber
  {
  public:
    // Type definitions
    using Divisors = std::vector<int>;
    using Resolution = glm::ivec2;
    using Resolutions = std::vector<Resolution>;
    using RefreshRate = unsigned;

    // Normalized screen region of a channel, keyed by channel ID
    using ChannelRegions = std::vector<std::pair<uint8_t, Imaging::UVs>>;
    // Average color of a channel's region, keyed by channel ID
    using ChannelColors = std::vector<std::pair<uint8_t, Imaging::Color>>;


    /**
     * @brief Where captured frames are processed
     *
     */
    struct ProcessingInfo
    {
      bool gpu{false};
      std::string details; // GPU name and buffer type, or why the CPU is used
    };

    // Constructor / Destructor
    /**
     * @brief IGrabber constructor
     * 
     * @param config Huenicorn configuration
     */
    IGrabber(
      Core::Config* config
    ):
    m_config(config)
    {}


    /**
     * @brief IGrabber destructor
     * 
     */
    virtual ~IGrabber(){}


    // Getters
    /**
     * @brief Returns the identifier of the grabber implementation
     * 
     * @return const std::string& Grabber identifier
     */
    virtual const std::string& name() const = 0;


    /**
     * @brief Returns the resolution of the selected display
     * 
     * @return Resolution Resolution of the selected display
     */
    virtual Resolution displayResolution() const = 0;


    /**
     * @brief Returns the refresh rate of the display
     * 
     * @return RefreshRate Refresh rate of the display
     */
    virtual RefreshRate displayRefreshRate() const = 0;


    // Methods
    /**
     * @brief Sets the screen regions whose average colors grabChannelColors() returns
     *
     * @param regions Regions of the active channels
     */
    virtual void setChannelRegions(
      const ChannelRegions& regions
    ) = 0;


    /**
     * @brief Returns the average color of each channel region in the last captured frame
     *
     * Frames are captured asynchronously: right after setChannelRegions(), colors may still
     * come from the previous regions, or miss newly added channels.
     *
     * @param colors Average color per channel
     * @return true At least one frame was processed
     * @return false No frame was processed yet
     */
    virtual bool grabChannelColors(
      ChannelColors& colors
    ) = 0;


    /**
     * @brief Returns where the last captured frame was processed
     *
     * @return ProcessingInfo GPU or CPU, with details
     */
    virtual ProcessingInfo processingInfo() const = 0;


    /**
     * @brief Hints the grabber that frames aren't needed faster than frameRate
     *
     * @param frameRate Frames per second actually consumed
     */
    virtual void setFrameRateLimit(
      RefreshRate /*frameRate*/
    )
    {}


    /**
     * @brief Stops or resumes capturing, while the lamps don't follow the screen
     *
     * Until a frame is captured after resuming, grabChannelColors() returns false.
     *
     * @param paused Whether to stop capturing
     */
    virtual void setPaused(
      bool /*paused*/
    )
    {}


    /**
     * @brief Returns a list of available subsample resolutions
     * 
     * @return Resolutions List of available subsample resolutions
     */
    Resolutions subsampleResolutionCandidates()
    {
      auto resolution = displayResolution();
      auto wDivisors = _divisors(resolution.x);
      auto hDivisors = _divisors(resolution.y);

      auto validDivisors = _selectValidDivisors(wDivisors, hDivisors);

      Resolutions subsampleResolutionCandidates;

      for(const auto& validDivisor : validDivisors){
        int width = resolution.x / validDivisor;
        int height = (resolution.y * width) / resolution.x;
        subsampleResolutionCandidates.emplace_back(width, height);
      }

      return subsampleResolutionCandidates;
    }


  protected:
    // Protected static methods
    /**
     * @brief Computes a list of integer divisors for a given number
     * 
     * @param number 
     * @return Divisors 
     */
    inline static Divisors _divisors(
      int number
    )
    {
      Divisors divisors;
      for(int i = 1; i < number / 2; i++){
        if(number % i == 0){
          divisors.push_back(i);
        }
      }

      divisors.push_back(number);

      return divisors;
    }


    /**
     * @brief Outputs a list of integer divisors for given width and height out of divisor list
     * 
     * @param widthDivisors List of width divisors
     * @param heightDivisors List of height divisors
     * @return Divisors
     */
    inline static Divisors _selectValidDivisors(
      const Divisors& widthDivisors,
      const Divisors& heightDivisors
    )
    {
      Divisors validDivisors(std::max(widthDivisors.size(), heightDivisors.size()));
      auto validDivisorsIt = std::set_intersection(widthDivisors.begin(), widthDivisors.end(), heightDivisors.begin(), heightDivisors.end(), validDivisors.begin());

      validDivisors.resize(validDivisorsIt - validDivisors.begin());

      return validDivisors;
    }


    //Attributes
    Core::Config* m_config;
  };
}
