#include <Huenicorn/Core/Logger.hpp>

#include <iostream>


namespace Huenicorn::Core
{
  namespace Logger
  {
    void write(
      LogLevel logLevel,
      const std::string& message
    )
    {
      switch(logLevel)
      {
        // Flushed on every line: stdout is block buffered when not attached to a terminal
        // (e.g. under systemd), which held messages back from the journal
        case LogLevel::Message:
          std::cout << message << std::endl;
          break;

        case LogLevel::Warning:
          std::cout << "[Warning] : " << message << std::endl;
          break;

        case LogLevel::Error:
          std::cerr << "[Error] : " << message << "\n";
          break;

  #ifndef NDEBUG
        case LogLevel::Debug:
          std::cout << "[Debug] : " << message << std::endl;
          break;
  #endif // NDEBUG

        default:
          break;
      }
    }
  }
}
