#pragma once

#include <cstdint>


namespace Halomux::Stream
{
  struct HuestreamPayload
  {
    char channelId;
    char colorData0[2];
    char colorData1[2];
    char colorData2[2];


    /**
     * @brief Sets the channel ID
     * 
     * @param _channelId ID of the channel
     */
    inline void setChannelId(
      char _channelId
    )
    {
      channelId = _channelId;
    }


    /**
     * @brief Sets the Red field value
     * 
     * @param red Red value
     */
    inline void setR(
      uint16_t red
    )
    {
      uint8_t a = static_cast<uint8_t>((red >> 8) & 0xff);
      uint8_t b = static_cast<uint8_t>(red & 0xff);
      colorData0[0] = a;
      colorData0[1] = b;
    }


    /**
     * @brief Sets the Green field value
     * 
     * @param green Green value
     */
    inline void setG(
      uint16_t green
    )
    {
      uint8_t a = static_cast<uint8_t>((green >> 8) & 0xff);
      uint8_t b = static_cast<uint8_t>(green & 0xff);
      colorData1[0] = a;
      colorData1[1] = b;
    }


    /**
     * @brief Sets the blue field value
     * 
     * @param blue Blue value
     */
    inline void setB(
      uint16_t blue
    )
    {
      uint8_t a = static_cast<uint8_t>((blue >> 8) & 0xff);
      uint8_t b = static_cast<uint8_t>(blue & 0xff);
      colorData2[0] = a;
      colorData2[1] = b;
    }
  };
}
