#include <Halomux/Stream/HuestreamHeader.hpp>

#include <cstring>
#include <algorithm>


namespace Halomux::Stream
{
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
}