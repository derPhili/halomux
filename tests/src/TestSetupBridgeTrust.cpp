#include <Halomux/Core/Logger.hpp>
#include <Halomux/Core/SetupBridgeTrust.hpp>

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
}


/**
 * @brief Which bridges the setup pairs with: any device of the network can answer its search
 */
int main()
{
  {
    Core::SetupBridgeTrust trust;
    expect("Nothing validated, nothing trusted", !trust.trusted("192.168.0.42"));
    expect("Nothing to confirm", !trust.acceptUnverified("192.168.0.42").has_value());
  }

  {
    // Signed by the Hue CA, for its own address only
    Core::SetupBridgeTrust trust;
    trust.validated({.address = "192.168.0.42", .bridgeId = "aabbccddeeff0011", .verified = true});
    expect("Verified bridge trusted", trust.trusted("192.168.0.42"));
    expect("Another address not trusted", !trust.trusted("192.168.0.43"));
  }

  {
    // Its own certificate, such as an older bridge's or an impostor's: once the user confirms
    Core::SetupBridgeTrust trust;
    trust.validated({.address = "192.168.0.42", .bridgeId = "aabbccddeeff0011", .verified = false});
    expect("Unverified bridge not trusted yet", !trust.trusted("192.168.0.42"));

    // The address the user saw, not the one validated last
    expect("Another address can't be confirmed", !trust.acceptUnverified("192.168.0.43").has_value());
    expect("Still not trusted", !trust.trusted("192.168.0.42"));

    const auto accepted = trust.acceptUnverified("192.168.0.42");
    expect("Unverified bridge confirmed", accepted.has_value() && accepted->bridgeId == "aabbccddeeff0011" && !accepted->verified);
    expect("Confirmed bridge trusted", trust.trusted("192.168.0.42"));

    // Confirmed twice, such as a double click: still confirmed
    expect("Confirmed again", trust.acceptUnverified("192.168.0.42").has_value() && trust.trusted("192.168.0.42"));

    // Another address needs its own confirmation
    trust.validated({.address = "192.168.0.43", .bridgeId = "0011aabbccddeeff", .verified = false});
    expect("Confirmation doesn't carry over", !trust.trusted("192.168.0.43") && !trust.trusted("192.168.0.42"));

    // An address that isn't a bridge: nothing is trusted
    trust.validated({.address = "192.168.0.42", .bridgeId = "aabbccddeeff0011", .verified = true});
    trust.invalidate();
    expect("Nothing trusted after a failed validation", !trust.trusted("192.168.0.42"));
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Setup bridge trust: OK");
  return 0;
}
