#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>


namespace Halomux::Core
{
  /**
   * @brief A line of the service's journal
   *
   */
  struct JournalLine
  {
    enum class Level
    {
      Info,
      Warning,
      Error
    };

    // Milliseconds since the epoch
    int64_t time{0};
    Level level{Level::Info};
    std::string message;
  };


  /**
   * @brief Reads the service's journal as journalctl --output=json gives it
   *
   * The service's warnings and errors are told by their Logger prefix, which is dropped, those of
   * systemd by their priority. Lines that can't be read are left out.
   *
   * @param journal One JSON object per line
   * @return std::vector<JournalLine> Lines, oldest first
   */
  std::vector<JournalLine> parseJournal(
    std::string_view journal
  );
}
