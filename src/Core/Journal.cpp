#include <Halomux/Core/Journal.hpp>

#include <array>
#include <charconv>
#include <stdexcept>
#include <utility>

#include <Halomux/Serialization/Json.hpp>


namespace Halomux::Core
{
  namespace
  {
    using Serialization::Json;

    // The Logger's prefixes of warnings and errors (see Logger.hpp)
    constexpr std::array<std::pair<std::string_view, JournalLine::Level>, 2> Prefixes{{
      {"[Warning] : ", JournalLine::Level::Warning},
      {"[Error] : ", JournalLine::Level::Error}
    }};


    // A journal field's text: journalctl writes texts that aren't valid UTF-8 as arrays of bytes
    std::string text(
      const Json& field
    )
    {
      if(field.is_string()){
        return field.get<std::string>();
      }

      std::string bytes;
      if(field.is_array()){
        for(const Json& byte : field){
          if(byte.is_number_unsigned() && byte.get<unsigned>() <= 255){
            bytes.push_back(static_cast<char>(byte.get<unsigned>()));
          }
        }
      }

      return bytes;
    }


    int64_t number(
      const Json& field
    )
    {
      const std::string value = text(field);
      int64_t parsed = 0;
      const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
      if(error != std::errc() || end != value.data() + value.size()){
        throw std::invalid_argument("not a number: " + value);
      }

      return parsed;
    }
  }


  std::vector<JournalLine> parseJournal(
    std::string_view journal
  )
  {
    std::vector<JournalLine> lines;

    size_t start = 0;
    while(start < journal.size()){
      size_t end = journal.find('\n', start);
      if(end == std::string_view::npos){
        end = journal.size();
      }
      const std::string_view entry = journal.substr(start, end - start);
      start = end + 1;

      try{
        const Json fields = Json::parse(entry);
        JournalLine line{
          .time = number(fields.at("__REALTIME_TIMESTAMP")) / 1000,
          .level = JournalLine::Level::Info,
          .message = text(fields.at("MESSAGE"))
        };

        // syslog priorities: 3 and below errors, 4 warnings
        if(fields.contains("PRIORITY")){
          const int64_t priority = number(fields.at("PRIORITY"));
          line.level = priority <= 3 ? JournalLine::Level::Error : priority == 4 ? JournalLine::Level::Warning : JournalLine::Level::Info;
        }

        for(const auto& [prefix, level] : Prefixes){
          if(line.message.starts_with(prefix)){
            line.message.erase(0, prefix.size());
            line.level = level;
          }
        }

        lines.push_back(std::move(line));
      }
      catch(const std::exception&){
        // Not a journal entry of the expected shape
      }
    }

    return lines;
  }
}
