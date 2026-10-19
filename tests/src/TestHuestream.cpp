#include <cstddef>
#include <cstring>
#include <vector>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Stream/HuestreamHeader.hpp>
#include <Halomux/Stream/HuestreamPayload.hpp>

using namespace Halomux;


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


  unsigned byteAt(const std::vector<std::byte>& message, size_t index)
  {
    return std::to_integer<unsigned>(message.at(index));
  }
}


/**
 * @brief Messages of the Hue Entertainment API v2, as the bridge reads them
 */
int main()
{
  // Packed as the protocol lays them out
  expect("Header of 52 bytes", sizeof(Stream::HuestreamHeader) == 52);
  expect("Channel of 7 bytes", sizeof(Stream::HuestreamPayload) == 7);

  Stream::HuestreamHeader header;
  header.setEntertainmentConfigurationId("a0000000-0000-4000-8000-000000000001");
  header.sequenceId = 7;

  std::vector<std::byte> message;
  Stream::appendHuestreamMessage(message, header, {{.id = 3, .r = 1.f, .g = 0.5f, .b = 0.f}, {.id = 9, .r = -1.f, .g = 2.f, .b = 0.25f}});

  expect("Header and two channels", message.size() == 52 + 2 * 7);
  expect("Protocol name", message.size() >= 9 && std::memcmp(message.data(), "HueStream", 9) == 0);
  expect("Version 2.0", byteAt(message, 9) == 2 && byteAt(message, 10) == 0);
  expect("Sequence id", byteAt(message, 11) == 7);
  expect("XY and brightness", byteAt(message, 14) == 1);
  expect("Area id", std::memcmp(message.data() + 16, "a0000000-0000-4000-8000-000000000001", 36) == 0);

  // Channel id, then each value on 16 bits, most significant byte first
  expect("First channel id", byteAt(message, 52) == 3);
  expect("Full value", byteAt(message, 53) == 0xff && byteAt(message, 54) == 0xff);
  expect("Half value", byteAt(message, 55) == 0x7f && byteAt(message, 56) == 0xff);
  expect("Zero value", byteAt(message, 57) == 0 && byteAt(message, 58) == 0);
  expect("Second channel id", byteAt(message, 59) == 9);
  expect("Negative value clamped", byteAt(message, 60) == 0 && byteAt(message, 61) == 0);
  expect("Value above one clamped", byteAt(message, 62) == 0xff && byteAt(message, 63) == 0xff);
  expect("Quarter value", byteAt(message, 64) == 0x3f && byteAt(message, 65) == 0xff);

  // Appended: the buffer is reused across frames by clearing it
  Stream::appendHuestreamMessage(message, header, {});
  expect("Appended to the buffer", message.size() == 52 + 2 * 7 + 52);

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Huestream: OK");
  return 0;
}
