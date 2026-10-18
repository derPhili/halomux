#pragma once

#include <optional>
#include <vector>

#include <Halomux/Grabber/GnuLinux/Pipewire/DmaBufPlane.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <spa/buffer/buffer.h>
#pragma GCC diagnostic pop


namespace Halomux::Grabber
{
  /**
   * @brief Reads the planes of a DMA-BUF buffer of Pipewire, one per data
   *
   * @param buffer Buffer whose datas are of type SPA_DATA_DmaBuf
   * @return std::optional<std::vector<DmaBufPlane>> Planes, nothing if a plane misses its fd or
   *         chunk, or there are none or more than MaxDmaBufPlanes
   */
  std::optional<std::vector<DmaBufPlane>> dmaBufPlanes(
    const spa_buffer& buffer
  );
}
