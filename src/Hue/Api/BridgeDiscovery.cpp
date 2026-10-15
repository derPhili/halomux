#include <Huenicorn/Hue/Api/BridgeDiscovery.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <Huenicorn/Core/Logger.hpp>


namespace Huenicorn::Hue::Api
{
  namespace
  {
    constexpr const char* ServiceName = "_hue._tcp.local";
    constexpr uint16_t TypePtr = 12;
    constexpr uint16_t TypeTxt = 16;
    // Asks for unicast answers: they come back to this socket's port
    constexpr uint16_t ClassInUnicastResponse = 0x8001;


    /**
     * @brief Reads DNS messages, bounds checked
     *
     */
    class Reader
    {
    public:
      Reader(const uint8_t* data, size_t size):
      m_data(data),
      m_size(size)
      {}

      std::optional<uint16_t> u16(size_t& offset) const
      {
        if(offset + 2 > m_size){
          return std::nullopt;
        }

        const uint16_t value = static_cast<uint16_t>((m_data[offset] << 8) | m_data[offset + 1]);
        offset += 2;
        return value;
      }

      // Reads a possibly compressed name, lowercase, labels joined by dots
      std::optional<std::string> name(size_t& offset) const
      {
        std::string result;
        size_t position = offset;
        bool jumped = false;

        // Bounds the pointers followed, against loops
        for(int steps = 0; steps < 128; steps++){
          if(position >= m_size){
            return std::nullopt;
          }

          const uint8_t length = m_data[position];
          if(length == 0){
            if(!jumped){
              offset = position + 1;
            }
            return result;
          }

          if((length & 0xc0) == 0xc0){
            if(position + 1 >= m_size){
              return std::nullopt;
            }

            if(!jumped){
              offset = position + 2;
            }
            position = ((length & 0x3f) << 8) | m_data[position + 1];
            jumped = true;
            continue;
          }

          if(position + 1 + length > m_size){
            return std::nullopt;
          }

          if(!result.empty()){
            result += '.';
          }

          for(size_t i = 0; i < length; i++){
            result += static_cast<char>(std::tolower(m_data[position + 1 + i]));
          }
          position += 1 + length;
        }

        return std::nullopt;
      }

      const uint8_t* data() const
      {
        return m_data;
      }

      size_t size() const
      {
        return m_size;
      }

    private:
      const uint8_t* m_data;
      size_t m_size;
    };


    std::vector<uint8_t> query()
    {
      std::vector<uint8_t> message = {
        0, 0, // id
        0, 0, // flags
        0, 1, // questions
        0, 0, 0, 0, 0, 0 // answers, authorities, additionals
      };

      std::string name = ServiceName;
      size_t start = 0;
      while(start <= name.size()){
        const size_t end = std::min(name.find('.', start), name.size());
        message.push_back(static_cast<uint8_t>(end - start));
        message.insert(message.end(), name.begin() + static_cast<long>(start), name.begin() + static_cast<long>(end));
        start = end + 1;
      }
      message.push_back(0);

      for(uint16_t value : {TypePtr, ClassInUnicastResponse}){
        message.push_back(static_cast<uint8_t>(value >> 8));
        message.push_back(static_cast<uint8_t>(value & 0xff));
      }

      return message;
    }


  }


  std::optional<std::string> parseDiscoveryAnswer(
    const uint8_t* data,
    size_t size
  )
  {
    const Reader reader(data, size);

    size_t offset = 4;
    const auto questions = reader.u16(offset);
    const auto answers = reader.u16(offset);
    const auto authorities = reader.u16(offset);
    const auto additionals = reader.u16(offset);
    if(!questions || !answers || !authorities || !additionals){
      return std::nullopt;
    }

    for(unsigned i = 0; i < *questions; i++){
      if(!reader.name(offset)){
        return std::nullopt;
      }
      offset += 4;
    }

    bool aboutHue = false;
    std::string bridgeId;

    const unsigned records = *answers + *authorities + *additionals;
    for(unsigned i = 0; i < records; i++){
      const auto name = reader.name(offset);
      const auto type = reader.u16(offset);
      if(!name || !type){
        return std::nullopt;
      }

      offset += 6; // class, ttl
      const auto length = reader.u16(offset);
      if(!length || offset + *length > reader.size()){
        return std::nullopt;
      }

      if(*type == TypePtr && *name == ServiceName){
        aboutHue = true;
      }

      if(*type == TypeTxt && name->ends_with(ServiceName)){
        // Strings prefixed with their length, such as "bridgeid=001788fffe4a2b3c"
        size_t position = offset;
        while(position < offset + *length){
          const uint8_t entryLength = reader.data()[position];
          const std::string entry(reinterpret_cast<const char*>(reader.data() + position + 1), std::min<size_t>(entryLength, offset + *length - position - 1));
          if(entry.starts_with("bridgeid=")){
            bridgeId = entry.substr(9);
          }
          position += 1 + entryLength;
        }
      }

      offset += *length;
    }

    if(!aboutHue){
      return std::nullopt;
    }

    return bridgeId;
  }


  std::vector<DiscoveredBridge> discoverBridges(
    std::chrono::milliseconds timeout
  )
  {
    std::vector<DiscoveredBridge> bridges;

    const int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(socketFd < 0){
      Core::Logger::warn("mDNS discovery: could not open a socket");
      return bridges;
    }

    sockaddr_in multicast{};
    multicast.sin_family = AF_INET;
    multicast.sin_port = htons(5353);
    inet_pton(AF_INET, "224.0.0.251", &multicast.sin_addr);

    const auto message = query();
    if(sendto(socketFd, message.data(), message.size(), 0, reinterpret_cast<sockaddr*>(&multicast), sizeof(multicast)) < 0){
      Core::Logger::warn("mDNS discovery: could not send the query");
      close(socketFd);
      return bridges;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::array<uint8_t, 9000> buffer;

    while(true){
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
      if(remaining.count() <= 0){
        break;
      }

      pollfd pollFd{socketFd, POLLIN, 0};
      if(poll(&pollFd, 1, static_cast<int>(remaining.count())) <= 0){
        break;
      }

      sockaddr_in sender{};
      socklen_t senderSize = sizeof(sender);
      const ssize_t received = recvfrom(socketFd, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize);
      if(received <= 0){
        continue;
      }

      const auto bridgeId = parseDiscoveryAnswer(buffer.data(), static_cast<size_t>(received));
      if(!bridgeId){
        continue;
      }

      // The bridge answers from its own address
      std::array<char, INET_ADDRSTRLEN> address{};
      inet_ntop(AF_INET, &sender.sin_addr, address.data(), address.size());

      const bool known = std::any_of(bridges.begin(), bridges.end(), [&](const DiscoveredBridge& bridge){return bridge.address == address.data();});
      if(!known){
        bridges.push_back({*bridgeId, address.data()});
      }
    }

    close(socketFd);
    return bridges;
  }
}
