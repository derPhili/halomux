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


    // Length of the UTF-8 sequence starting at a position, 0 if it isn't a valid one
    size_t utf8SequenceLength(
      const std::string& bytes,
      size_t position
    )
    {
      const auto byte = [&bytes](size_t i){return static_cast<unsigned char>(bytes[i]);};
      const unsigned char first = byte(position);

      size_t length = 0;
      if(first < 0x80){
        return 1;
      }
      else if((first & 0xE0) == 0xC0 && first >= 0xC2){
        length = 2;
      }
      else if((first & 0xF0) == 0xE0){
        length = 3;
      }
      else if((first & 0xF8) == 0xF0 && first <= 0xF4){
        length = 4;
      }
      else{
        return 0;
      }

      if(position + length > bytes.size()){
        return 0;
      }
      for(size_t i = 1; i < length; i++){
        if((byte(position + i) & 0xC0) != 0x80){
          return 0;
        }
      }

      return length;
    }


    // A journal field's text: journalctl writes texts that aren't valid UTF-8 as arrays of bytes.
    // Bytes that aren't UTF-8 are replaced, as the log's JSON answer would fail on them
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

      static constexpr std::string_view Replacement = "\xEF\xBF\xBD";
      std::string valid;
      valid.reserve(bytes.size());
      for(size_t i = 0; i < bytes.size();){
        const size_t length = utf8SequenceLength(bytes, i);
        if(length == 0){
          valid += Replacement;
          i++;
        }
        else{
          valid.append(bytes, i, length);
          i += length;
        }
      }

      return valid;
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
