#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <Halomux/Grabber/Monitors.hpp>
#include <Halomux/Hue/Api/Channel.hpp>
#include <Halomux/Imaging/UV.hpp>


namespace Halomux::Core
{
  /**
   * @brief The lamps of the selected entertainment area, their streams and the captured monitors
   *
   * Shared between the streaming loop, which computes the colors under access(), and the web UI
   * threads, which edit the lamps through the other methods.
   */
  class ChannelStore
  {
  public:
    /**
     * @brief Exclusive access to the whole state, as long as it lives
     *
     */
    class Access
    {
    public:
      /**
       * @brief Finds the monitor of each channel among newly captured monitors
       *
       * Active channels of profiles from before monitors per lamp follow the first monitor.
       *
       * @param monitors Captured monitors, in the order channels refer to them
       */
      void resolveMonitors(
        Grabber::Monitors monitors
      );


      /**
       * @brief Returns the horizontal center of a channel's region on the whole desktop
       *
       * @param channel Channel
       * @return float 0 at the left edge of the desktop, 1 at the right one, 0.5 if its monitor isn't captured
       */
      float desktopCenterX(
        const Hue::Api::Channel& channel
      ) const;


      Hue::Api::Channels& channels;
      Hue::Api::ChannelStreams& streams;
      const Grabber::Monitors& monitors;

    private:
      friend ChannelStore;

      Access(
        ChannelStore& store
      );

      std::unique_lock<std::mutex> m_lock;
      ChannelStore& m_store;
    };


    /**
     * @brief Locks the whole state
     *
     * @return Access Access, holding the lock
     */
    Access access();


    /**
     * @brief Replaces the lamps, such as when another entertainment area is selected
     *
     * The former streams are dropped.
     *
     * @param channels Lamps of the area
     * @param monitors Captured monitors
     */
    void replace(
      Hue::Api::Channels channels,
      Grabber::Monitors monitors
    );


    /**
     * @brief Tells whether monitors were lost or recovered since they were last resolved
     *
     * @param monitors Captured monitors now
     * @return true The lost state of a monitor changed, or monitors were added or removed
     */
    bool monitorsLostChanged(
      const Grabber::Monitors& monitors
    ) const;


    /**
     * @brief Activates or deactivates a lamp
     *
     * A lamp set up for the first time follows the monitor most other lamps follow. A deactivated
     * lamp is streamed black once before it's released.
     *
     * @param channelId Lamp
     * @param active Whether it follows the screen
     * @return true The lamp exists
     */
    bool setActivity(
      uint8_t channelId,
      bool active
    );


    /**
     * @brief Moves a corner of a lamp's region, inside the screen
     *
     * @param channelId Lamp
     * @param uv New position of the corner
     * @param corner Corner
     * @return std::optional<Imaging::UVs> Resulting region, nothing if the lamp doesn't exist
     */
    std::optional<Imaging::UVs> setUV(
      uint8_t channelId,
      const Imaging::UV& uv,
      Imaging::UVCorner corner
    );


    /**
     * @brief Moves a lamp's region onto a captured monitor
     *
     * @param channelId Lamp
     * @param monitor Index of the monitor
     * @param uvs Region on that monitor, clamped and kept at least one subsample cell large
     * @param configuredSubsampleWidth Configured subsample width, for the widest monitor
     * @return std::optional<Imaging::UVs> Resulting region, nothing if the lamp or the monitor doesn't exist
     */
    std::optional<Imaging::UVs> setMonitor(
      uint8_t channelId,
      size_t monitor,
      const Imaging::UVs& uvs,
      unsigned configuredSubsampleWidth
    );


    /**
     * @brief Sets the gamma correction of a lamp
     *
     * @param channelId Lamp
     * @param gammaFactor Gamma factor
     * @return true The lamp exists and the factor is a number
     */
    bool setGammaFactor(
      uint8_t channelId,
      float gammaFactor
    );


    /**
     * @brief Returns a copy of the lamps
     *
     * @return Hue::Api::Channels Lamps
     */
    Hue::Api::Channels snapshot() const;


    /**
     * @brief Returns the horizontal center of each active lamp's region on the whole desktop
     *
     * @return std::vector<std::pair<uint8_t, float>> Lamp and center, 0 at the left, 1 at the right
     */
    std::vector<std::pair<uint8_t, float>> activeLampCenters() const;


  private:
    // With the lock held
    void _resolveMonitors(
      Grabber::Monitors monitors
    );

    static float _desktopCenterX(
      const Hue::Api::Channel& channel,
      const Grabber::Monitors& monitors
    );


    mutable std::mutex m_mutex;
    Hue::Api::Channels m_channels;
    Hue::Api::ChannelStreams m_streams;
    Grabber::Monitors m_monitors;
  };
}
