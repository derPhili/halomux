#include <Halomux/Hue/Auth/Credentials.hpp>

#include <optional>


namespace
{
  // Value of a hexadecimal digit, nothing for another character
  std::optional<unsigned char> hexDigit(
    char c
  )
  {
    if(c >= '0' && c <= '9'){
      return static_cast<unsigned char>(c - '0');
    }
    if(c >= 'a' && c <= 'f'){
      return static_cast<unsigned char>(c - 'a' + 10);
    }
    if(c >= 'A' && c <= 'F'){
      return static_cast<unsigned char>(c - 'A' + 10);
    }

    return std::nullopt;
  }


  // Bytes of a hexadecimal string, nothing if it isn't one
  std::optional<std::vector<unsigned char>> hexStringToBytes(
    const std::string& hexString
  )
  {
    if(hexString.empty() || hexString.size() % 2 != 0){
      return std::nullopt;
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(hexString.size() / 2);

    for(size_t i = 0; i < hexString.size(); i += 2){
      const auto high = hexDigit(hexString[i]);
      const auto low = hexDigit(hexString[i + 1]);
      if(!high || !low){
        return std::nullopt;
      }

      bytes.push_back(static_cast<unsigned char>(*high << 4 | *low));
    }

    return bytes;
  }


  std::vector<unsigned char> stringToBytes(
    const std::string& string
  )
  {
    std::vector<unsigned char> bytes;
    for(const auto& c : string){
      bytes.push_back(static_cast<unsigned char>(c));
    }

    return bytes;
  }
}


namespace Halomux::Hue::Auth
{
  Credentials::Credentials(
    const std::string& username,
    const std::string& clientkey
  ):
  m_username(username),
  m_clientkey(clientkey)
  {}


  const std::string& Credentials::username() const
  {
    return m_username;
  }


  const std::string& Credentials::clientkey() const
  {
    return m_clientkey;
  }


  std::vector<unsigned char> Credentials::usernameBytes() const
  {
    return stringToBytes(m_username);
  }


  std::vector<unsigned char> Credentials::clientkeyBytes() const
  {
    auto bytes = hexStringToBytes(m_clientkey);
    if(!bytes){
      throw std::runtime_error("The clientkey isn't hexadecimal");
    }

    return std::move(*bytes);
  }


  bool Credentials::hasValidClientkey() const
  {
    // The bridge's clientkeys are 16 bytes: another length, such as a mistyped key, would fail
    // every streaming connection
    static constexpr size_t ClientkeySize = 16;
    const auto bytes = hexStringToBytes(m_clientkey);
    return bytes.has_value() && bytes->size() == ClientkeySize;
  }
}
