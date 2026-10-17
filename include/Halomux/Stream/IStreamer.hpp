#pragma once

#include <Halomux/Hue/Api/Channel.hpp>


namespace Halomux::Stream
{
  /**
   * @brief Streams lamp colors to an entertainment area of the bridge
   *
   */
  class IStreamer
  {
  public:
    virtual ~IStreamer() = default;


    /**
     * @brief Sends the colors of a frame, without waiting for the connection
     *
     * @param channels Colors per channel
     */
    virtual void streamChannels(
      const Hue::Api::ChannelStreams& channels
    ) = 0;


    /**
     * @brief Drops the connection and connects again, such as when the bridge stopped the stream
     *
     * Safe to call from any thread: the streaming thread hands the connection over on its next frame.
     */
    virtual void reconnect() = 0;
  };
}
