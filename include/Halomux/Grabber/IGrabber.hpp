#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include <Halomux/Grabber/Monitors.hpp>
#include <Halomux/Grabber/SubsampleResolutions.hpp>
#include <Halomux/Imaging/Color.hpp>
#include <Halomux/Imaging/ImageData.hpp>
#include <Halomux/Imaging/UV.hpp>
#include <Halomux/Core/Logger.hpp>


namespace Halomux::Core
{
  class Config;
}

namespace Halomux::Grabber
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
    using Resolution = glm::ivec2;
    using Resolutions = std::vector<Resolution>;
    using RefreshRate = unsigned;

    /**
     * @brief Normalized region of a channel on one of the captured monitors
     *
     */
    struct ChannelRegion
    {
      uint8_t channelId;
      size_t monitor; // Index in monitors()
      Imaging::UVs uvs;

      bool operator==(const ChannelRegion& other) const = default;
    };

    using ChannelRegions = std::vector<ChannelRegion>;
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
     * @param config Halomux configuration
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
     * @brief Returns the pixel resolution of the widest captured monitor
     * 
     * @return Resolution Resolution of the widest captured monitor
     */
    virtual Resolution displayResolution() const = 0;


    /**
     * @brief Returns the highest refresh rate of the captured monitors
     * 
     * @return RefreshRate Refresh rate of the display
     */
    virtual RefreshRate displayRefreshRate() const = 0;


    /**
     * @brief Returns the captured monitors, in a stable order
     *
     * @return Monitors Captured monitors
     */
    virtual Monitors monitors() const = 0;


    // Methods
    /**
     * @brief Sets the screen regions whose average colors grabChannelColors() returns
     *
     * Monitors without a region aren't captured. Regions on a monitor index out of range are ignored.
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
     * Each monitor sends frames only when its content changes: a channel keeps the color of the
     * last frame of its monitor.
     *
     * @param colors Average color per channel
     * @return true A frame of a monitor with regions was processed, or there are no regions
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
      return subsampleResolutions(displayResolution());
    }


  protected:
    //Attributes
    Core::Config* m_config;
  };
}
