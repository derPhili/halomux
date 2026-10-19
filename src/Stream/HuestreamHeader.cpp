#include <Halomux/Stream/HuestreamHeader.hpp>

#include <cstring>
#include <algorithm>

#include <Halomux/Stream/HuestreamPayload.hpp>


namespace Halomux::Stream
{
  namespace
  {
    // A normalized value on 16 bits, clamped to [0, 1]
    uint16_t toUint16(
      float value
    )
    {
      // Converting an out of range (or NaN) float to an integer is undefined behavior
      if(!(value > 0.f)){
        return 0;
      }

      return static_cast<uint16_t>(std::min(value, 1.f) * 0xffff);
    }
  }


  void HuestreamHeader::setColorSpace(
    char _colorSpace
  )
  {
    this->colorSpace = _colorSpace;
  }


  void HuestreamHeader::setEntertainmentConfigurationId(
    const std::string& _entertainmentConfigurationId
  )
  {
    std::memset(entertainmentConfigurationId, 0, sizeof(entertainmentConfigurationId));
    std::memcpy(
      entertainmentConfigurationId,
      _entertainmentConfigurationId.data(),
      std::min(_entertainmentConfigurationId.size(), sizeof(entertainmentConfigurationId))
    );
  }


  void appendHuestreamMessage(
    std::vector<std::byte>& buffer,
    const HuestreamHeader& header,
    const Hue::Api::ChannelStreams& channels
  )
  {
    const auto* headerBytes = reinterpret_cast<const std::byte*>(&header);
    buffer.insert(buffer.end(), headerBytes, headerBytes + sizeof(HuestreamHeader));

    for(const auto& channel : channels){
      HuestreamPayload payload;
      payload.setChannelId(static_cast<char>(channel.id));
      payload.setR(toUint16(channel.r));
      payload.setG(toUint16(channel.g));
      payload.setB(toUint16(channel.b));

      const auto* payloadBytes = reinterpret_cast<const std::byte*>(&payload);
      buffer.insert(buffer.end(), payloadBytes, payloadBytes + sizeof(HuestreamPayload));
    }
  }
}
