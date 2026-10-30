#include <iostream>
#include <sstream>
#include <string>

#include <Halomux/Core/Logger.hpp>

using namespace Halomux;


namespace
{
  int failures = 0;

  void expect(const char* name, bool condition)
  {
    if(!condition){
      std::cerr << "[Error] : " << name << "\n";
      failures++;
    }
  }


  // What a log call writes to stdout
  std::string written(auto&& logCall)
  {
    std::ostringstream captured;
    std::streambuf* original = std::cout.rdbuf(captured.rdbuf());
    logCall();
    std::cout.rdbuf(original);
    return captured.str();
  }
}


/**
 * @brief Log lines as the journal gets them: one line per message, whatever the message holds
 */
int main()
{
  expect("Message", written([](){Core::Logger::log("Started ", 3, " streams");}) == "Started 3 streams\n");
  expect("Warning prefixed", written([](){Core::Logger::warn("Careful");}) == "[Warning] : Careful\n");

  // Line breaks in a message, such as from a request's data, would forge lines of their own,
  // and other control characters would be the terminal's
  expect("Line break replaced", written([](){Core::Logger::log("Unknown area x\n[Error] : forged");}) == "Unknown area x [Error] : forged\n");
  expect("Control character replaced", written([](){Core::Logger::log("a\x1b[31mb\tc");}) == "a?[31mb c\n");
  expect("UTF-8 kept", written([](){Core::Logger::log("Küche");}) == "Küche\n");

  if(failures > 0){
    std::cerr << "[Error] : " << failures << " check(s) failed\n";
    return 1;
  }

  std::cout << "Logger: OK\n";
  return 0;
}
