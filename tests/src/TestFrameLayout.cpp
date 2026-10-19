#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/FrameLayout.hpp>

using namespace Halomux;
using Grabber::sharedMemoryFrame;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      Core::Logger::error(name);
      failures++;
    }
  }
}


/**
 * @brief Frames in shared memory buffers, as producers describe them: read only inside the buffer
 */
int main()
{
  // 4 x 2 pixels of 4 bytes
  const auto packed = sharedMemoryFrame(0, 16, 32, 4, 2);
  expect("Packed frame", packed.has_value() && packed->offset == 0 && packed->stride == 16);

  const auto padded = sharedMemoryFrame(8, 24, 64, 4, 2);
  expect("Padded rows after an offset", padded.has_value() && padded->offset == 8 && padded->stride == 24);

  const auto unknownStride = sharedMemoryFrame(0, 0, 32, 4, 2);
  expect("Stride of a packed frame when not told", unknownStride.has_value() && unknownStride->stride == 16);

  // The offset is taken modulo the buffer's size, as SPA says
  const auto wrapped = sharedMemoryFrame(64 + 4, 16, 64, 4, 2);
  expect("Offset modulo the buffer size", wrapped.has_value() && wrapped->offset == 4);

  // Beyond the buffer: refused instead of read past its end
  expect("Last row past the end", !sharedMemoryFrame(4, 16, 32, 4, 2).has_value());
  expect("Rows longer than the buffer", !sharedMemoryFrame(0, 16, 16, 4, 2).has_value());
  expect("Stride shorter than a row", !sharedMemoryFrame(0, 8, 64, 4, 2).has_value());
  expect("Empty buffer", !sharedMemoryFrame(0, 16, 0, 4, 2).has_value());
  expect("Empty frame", !sharedMemoryFrame(0, 16, 32, 0, 2).has_value());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Frame layout: OK");
  return 0;
}
