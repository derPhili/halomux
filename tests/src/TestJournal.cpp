#include <Halomux/Core/Journal.hpp>
#include <Halomux/Core/Logger.hpp>

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
 * @brief The service's journal as journalctl --output=json gives it, for the light manager's log page
 */
int main()
{
  const std::string journal =
    R"({"__REALTIME_TIMESTAMP":"1792400000123456","PRIORITY":"6","MESSAGE":"Started Halomux."})" "\n"
    R"({"__REALTIME_TIMESTAMP":"1792400001000000","PRIORITY":"6","MESSAGE":"[Warning] : The bridge is unreachable"})" "\n"
    R"({"__REALTIME_TIMESTAMP":"1792400002000000","PRIORITY":"6","MESSAGE":"[Error] : HTTP request failed"})" "\n"
    R"({"__REALTIME_TIMESTAMP":"1792400003000000","PRIORITY":"3","MESSAGE":"halomux.service: Main process exited, status=1/FAILURE"})" "\n"
    R"({"__REALTIME_TIMESTAMP":"1792400004000000","PRIORITY":"4","MESSAGE":"Some warning of systemd"})" "\n"
    R"({"__REALTIME_TIMESTAMP":"1792400005000000","PRIORITY":"6","MESSAGE":[104,105]})" "\n"
    "not json\n"
    R"({"PRIORITY":"6","MESSAGE":"No time"})" "\n";

  const auto lines = Core::parseJournal(journal);
  expect("Readable lines kept", lines.size() == 6);

  if(lines.size() == 6){
    expect("Time in milliseconds", lines[0].time == 1792400000123);
    expect("Message", lines[0].message == "Started Halomux." && lines[0].level == Core::JournalLine::Level::Info);
    expect("Warning of the service, its prefix dropped", lines[1].level == Core::JournalLine::Level::Warning && lines[1].message == "The bridge is unreachable");
    expect("Error of the service, its prefix dropped", lines[2].level == Core::JournalLine::Level::Error && lines[2].message == "HTTP request failed");
    expect("Error by priority", lines[3].level == Core::JournalLine::Level::Error);
    expect("Warning by priority", lines[4].level == Core::JournalLine::Level::Warning);
    expect("Binary message read as text", lines[5].message == "hi");
  }

  expect("Empty journal", Core::parseJournal("").empty());

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Journal: OK");
  return 0;
}
