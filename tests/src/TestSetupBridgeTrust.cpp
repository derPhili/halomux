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
    expect("Nothing to confirm", !trust.acceptUnverified());
  }

  {
    // Signed by the Hue CA, for its own address only
    Core::SetupBridgeTrust trust;
    trust.validated("192.168.0.42", true);
    expect("Verified bridge trusted", trust.trusted("192.168.0.42"));
    expect("Another address not trusted", !trust.trusted("192.168.0.43"));
    expect("Nothing to confirm for a verified bridge", !trust.acceptUnverified());
  }

  {
    // Its own certificate, such as an older bridge's or an impersonator's: once the user confirms
    Core::SetupBridgeTrust trust;
    trust.validated("192.168.0.42", false);
    expect("Unverified bridge not trusted yet", !trust.trusted("192.168.0.42"));
    expect("Unverified bridge confirmed", trust.acceptUnverified());
    expect("Confirmed bridge trusted", trust.trusted("192.168.0.42"));

    // Another address needs its own confirmation
    trust.validated("192.168.0.43", false);
    expect("Confirmation doesn't carry over", !trust.trusted("192.168.0.43") && !trust.trusted("192.168.0.42"));

    // An address that isn't a bridge: nothing is trusted
    trust.validated("192.168.0.42", true);
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
