#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>


namespace Halomux::Grabber
{
  /**
   * @brief Where a frame lies in a shared memory buffer
   *
   */
  struct FrameLayout
  {
    // Of its first row, from the start of the buffer's data
    size_t offset;
    // Between the starts of two rows
    size_t stride;
  };


  /**
   * @brief Locates a frame of 4 byte pixels in a shared memory buffer, as its chunk describes it
   *
   * The producer describes the frame: one that doesn't fit the buffer, such as from a faulty
   * producer or a chunk not matching the format, would be read past the buffer's end.
   *
   * @param chunkOffset Offset of the chunk, taken modulo the buffer's size as SPA says
   * @param chunkStride Stride of the chunk, that of packed rows if not positive
   * @param bufferSize Size of the buffer's data
   * @param width Frame width in pixels
   * @param height Frame height in pixels
   * @return std::optional<FrameLayout> Its layout, nothing if it doesn't fit in the buffer
   */
  inline std::optional<FrameLayout> sharedMemoryFrame(
    uint32_t chunkOffset,
    int32_t chunkStride,
    size_t bufferSize,
    int width,
    int height
  )
  {
    if(width <= 0 || height <= 0 || bufferSize == 0){
      return std::nullopt;
    }

    const size_t rowSize = static_cast<size_t>(width) * 4;
    const size_t stride = chunkStride > 0 ? static_cast<size_t>(chunkStride) : rowSize;
    if(stride < rowSize){
      return std::nullopt;
    }

    const size_t offset = chunkOffset % bufferSize;
    if(offset + (static_cast<size_t>(height) - 1) * stride + rowSize > bufferSize){
      return std::nullopt;
    }

    return FrameLayout{.offset = offset, .stride = stride};
  }
}
