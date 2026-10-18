#pragma once

#include <cstdint>


namespace Halomux::Grabber
{
  /**
   * @brief A plane of a DMA-BUF: compressed formats (such as Intel CCS or AMD DCC modifiers) add
   *        planes holding the compression metadata
   *
   */
  struct DmaBufPlane
  {
    int fd;
    uint32_t offset;
    uint32_t stride;
  };


  // As many planes as EGL imports
  inline constexpr uint32_t MaxDmaBufPlanes = 4;
}
