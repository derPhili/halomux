#include <memory>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Api/BridgeDiscovery.hpp>

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


  // Builds DNS messages for the test
  struct Message
  {
    std::vector<uint8_t> bytes;

    void u16(uint16_t value)
    {
      bytes.push_back(static_cast<uint8_t>(value >> 8));
      bytes.push_back(static_cast<uint8_t>(value & 0xff));
    }

    void name(const std::vector<std::string>& labels)
    {
      for(const auto& label : labels){
        bytes.push_back(static_cast<uint8_t>(label.size()));
        bytes.insert(bytes.end(), label.begin(), label.end());
      }
      bytes.push_back(0);
    }

    void pointer(uint16_t offset)
    {
      u16(static_cast<uint16_t>(0xc000 | offset));
    }

    // Record header after its name: type, class, ttl, data length
    void record(uint16_t type, uint16_t length)
    {
      u16(type);
      u16(1);
      u16(0);
      u16(120);
      u16(length);
    }
  };


  // An answer as a Hue bridge sends it: PTR to its instance, TXT with its id
  Message bridgeAnswer(const std::string& bridgeId = "aabbccddeeff0011")
  {
    Message message;
    message.u16(0); message.u16(0x8400); // id, flags
    message.u16(0); message.u16(2); message.u16(0); message.u16(0); // counts

    const uint16_t serviceOffset = static_cast<uint16_t>(message.bytes.size());
    message.name({"_hue", "_tcp", "local"});

    const std::string instance = "Hue Bridge - 0A9691";
    message.record(12, static_cast<uint16_t>(1 + instance.size() + 2));
    const uint16_t instanceOffset = static_cast<uint16_t>(message.bytes.size());
    message.bytes.push_back(static_cast<uint8_t>(instance.size()));
    message.bytes.insert(message.bytes.end(), instance.begin(), instance.end());
    message.pointer(serviceOffset);

    const std::string id = "bridgeid=" + bridgeId;
    const std::string model = "modelid=BSB002";
    message.pointer(instanceOffset);
    message.record(16, static_cast<uint16_t>(2 + id.size() + model.size()));
    message.bytes.push_back(static_cast<uint8_t>(id.size()));
    message.bytes.insert(message.bytes.end(), id.begin(), id.end());
    message.bytes.push_back(static_cast<uint8_t>(model.size()));
    message.bytes.insert(message.bytes.end(), model.begin(), model.end());

    return message;
  }
}


/**
 * @brief Checks the reading of mDNS answers, which come from the network
 *
 */
int main()
{
  const Message valid = bridgeAnswer();
  const auto id = Hue::Api::parseDiscoveryAnswer(valid.bytes.data(), valid.bytes.size());
  expect("Bridge id from a compressed answer", id.has_value() && *id == "aabbccddeeff0011");

  // Anyone in the network can answer: an id that isn't one is dropped, the answer still counts
  for(const std::string bad : {std::string("\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0"), std::string("aabbccdd\"eff0011"), std::string("x")}){
    const Message odd = bridgeAnswer(bad);
    const auto oddId = Hue::Api::parseDiscoveryAnswer(odd.bytes.data(), odd.bytes.size());
    expect("Invalid bridge id dropped", oddId.has_value() && oddId->empty());
  }

  // Every truncation is rejected or read safely, never read beyond the message
  for(size_t size = 0; size < valid.bytes.size(); size++){
    // Each in an allocation of its exact size, so AddressSanitizer catches any read beyond it
    const auto truncated = std::make_unique<uint8_t[]>(size);
    std::copy_n(valid.bytes.data(), size, truncated.get());
    Hue::Api::parseDiscoveryAnswer(truncated.get(), size);
  }

  // Another service
  Message other;
  other.u16(0); other.u16(0x8400);
  other.u16(0); other.u16(1); other.u16(0); other.u16(0);
  other.name({"_printer", "_tcp", "local"});
  other.record(12, 2);
  other.pointer(12);
  expect("Other service ignored", !Hue::Api::parseDiscoveryAnswer(other.bytes.data(), other.bytes.size()).has_value());

  // A name pointing to itself
  Message loop;
  loop.u16(0); loop.u16(0x8400);
  loop.u16(0); loop.u16(1); loop.u16(0); loop.u16(0);
  loop.pointer(12);
  loop.record(12, 0);
  expect("Pointer loop rejected", !Hue::Api::parseDiscoveryAnswer(loop.bytes.data(), loop.bytes.size()).has_value());

  // A record claiming more data than the message holds
  Message overflow = bridgeAnswer();
  overflow.bytes.resize(overflow.bytes.size() - 5);
  expect("Overlong record rejected", !Hue::Api::parseDiscoveryAnswer(overflow.bytes.data(), overflow.bytes.size()).has_value());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Bridge discovery: OK");
  return 0;
}
