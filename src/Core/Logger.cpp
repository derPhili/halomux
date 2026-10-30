#include <Halomux/Core/Logger.hpp>

#include <iostream>


namespace Halomux::Core
{
  namespace Logger
  {
    void write(
      LogLevel logLevel,
      const std::string& message
    )
    {
      // One line, written in one go: threads log at the same time, and a line put together from
      // several writes could be interleaved with another's. Line breaks in a message, such as
      // from a request's data, would forge lines of their own, other control characters would be
      // the terminal's: replaced.
      std::string line;
      switch(logLevel)
      {
        case LogLevel::Message:
          break;

        case LogLevel::Warning:
          line = "[Warning] : ";
          break;

        case LogLevel::Error:
          line = "[Error] : ";
          break;

  #ifndef NDEBUG
        case LogLevel::Debug:
          line = "[Debug] : ";
          break;
  #endif // NDEBUG

        default:
          return;
      }

      for(const char c : message){
        const auto byte = static_cast<unsigned char>(c);
        if(c == '\n' || c == '\r' || c == '\t'){
          line += ' ';
        }
        else if(byte < 0x20 || byte == 0x7f){
          line += '?';
        }
        else{
          line += c;
        }
      }
      line += '\n';

      // Flushed on every line: stdout is block buffered when not attached to a terminal
      // (e.g. under systemd), which held messages back from the journal
      std::ostream& out = logLevel == LogLevel::Error ? std::cerr : std::cout;
      out << line << std::flush;
    }
  }
}
