#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include <glm/vec2.hpp>

#include <Huenicorn/Imaging/ImageData.hpp>
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
     * @brief Takes a screen capture and returns a subsample of it as bitmap
     * 
     * @param imageData Subsample of screen capture
     */
    virtual void grabFrameSubsample(
      Imaging::ImageData& imageData
    ) = 0;


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
