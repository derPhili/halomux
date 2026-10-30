#include <stdexcept>

#include <Halomux/Core/Logger.hpp>
#include <Halomux/Hue/Auth/Credentials.hpp>

using namespace Halomux;
using Hue::Auth::Credentials;


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


  // Whether reading the clientkey as bytes refuses it
  bool refused(const std::string& clientkey)
  {
    try{
      Credentials("user", clientkey).clientkeyBytes();
      return false;
    }
    catch(const std::runtime_error&){
      return true;
    }
  }
}


/**
 * @brief Checks reading the clientkey, the pre-shared key of the streaming connection
 *
 */
int main()
{
  expect("Hex digits of both cases", Credentials("user", "00ff10Ab").clientkeyBytes() == std::vector<unsigned char>{0x00, 0xff, 0x10, 0xab});
  expect("Username bytes", Credentials("ab", "00").usernameBytes() == std::vector<unsigned char>{'a', 'b'});

  expect("Odd length refused", refused("abc"));
  expect("Non-hex refused", refused("zz"));
  expect("Partly hex refused", refused("1z"));
  expect("Sign refused", refused("+1"));
  expect("Space refused", refused(" 1"));

  expect("Valid clientkey", Credentials("user", "00112233445566778899aabbccddeeff").hasValidClientkey());
  expect("Empty clientkey", !Credentials("user", "").hasValidClientkey());
  expect("Odd clientkey", !Credentials("user", "abc").hasValidClientkey());
  expect("Non-hex clientkey", !Credentials("user", "00112233445566778899aabbccddeefg").hasValidClientkey());
  // Always 16 bytes: a shorter or longer one, such as a mistyped one, would fail every connection
  expect("Short clientkey", !Credentials("user", "00").hasValidClientkey());
  expect("Long clientkey", !Credentials("user", "00112233445566778899aabbccddeeff00").hasValidClientkey());

  // The username goes into an HTTP header: the characters of the bridge's usernames only
  expect("Valid username", Credentials("AbCdEf0123456789-_abcdefghijklmnopqrstuv", "00112233445566778899aabbccddeeff").hasValidUsername());
  expect("Empty username", !Credentials("", "00112233445566778899aabbccddeeff").hasValidUsername());
  expect("Username with a line break", !Credentials("user\r\nInjected: yes", "00112233445566778899aabbccddeeff").hasValidUsername());
  expect("Username with a space", !Credentials("user name", "00112233445566778899aabbccddeeff").hasValidUsername());
  expect("Overlong username", !Credentials(std::string(129, 'a'), "00112233445566778899aabbccddeeff").hasValidUsername());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Credentials: OK");
  return 0;
}
