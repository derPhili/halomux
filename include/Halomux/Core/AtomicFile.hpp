#pragma once

#include <filesystem>
#include <string_view>


namespace Halomux::Core
{
  /**
   * @brief Replaces the content of a file in one step, readable by its owner only
   *
   * The new content goes to a temporary file of its own next to it, which replaces the file only
   * once it's completely written and synced: after a failed write (full disk, quota, I/O error)
   * or a crash the file still holds its previous content. Concurrent writers each replace it
   * with a complete content. Missing parent directories are created.
   *
   * @param path File to write
   * @param content Its new content
   * @return true The file holds the new content
   * @return false It couldn't be written, and holds its previous content (the error is logged)
   */
  bool writeFileAtomically(
    const std::filesystem::path& path,
    std::string_view content
  );
}
