#pragma once

#include <sstream>


namespace Halomux::Core
{
  namespace Logger
  {
    enum class LogLevel
    {
      Message,
      Warning,
      Error,
      Debug,
    };


    void write(
      LogLevel level,
      const std::string& message
    );

    template<typename... Args>
    void log(
      LogLevel logLevel,
      Args&&... args
    )
    {
      std::ostringstream oss;
      (oss << ... << std::forward<Args>(args));
      write(logLevel, oss.str());
    }


    template<typename... Args>
    void log(
      Args&&... args
    )
    {
      log(LogLevel::Message, std::forward<Args>(args)...);
    }


    template<typename... Args>
    void warn(
      Args&&... args
    )
    {
      log(LogLevel::Warning, std::forward<Args>(args)...);
    }


    template<typename... Args>
    void error(
      Args&&... args
    )
    {
      log(LogLevel::Error, std::forward<Args>(args)...);
    }


    template<typename... Args>
    void debug(
      Args&&... args
    )
    {
      log(LogLevel::Debug, std::forward<Args>(args)...);
    }
  }
}
