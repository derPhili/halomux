#include <Halomux/Core/AtomicFile.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <Halomux/Core/Logger.hpp>


namespace
{
  // Writes all of the content, through partial writes and interruptions
  bool writeAll(
    int fd,
    std::string_view content
  )
  {
    while(!content.empty()){
      const ssize_t written = ::write(fd, content.data(), content.size());
      if(written < 0){
        if(errno == EINTR){
          continue;
        }

        return false;
      }

      content.remove_prefix(static_cast<size_t>(written));
    }

    return true;
  }
}


namespace Halomux::Core
{
  bool writeFileAtomically(
    const std::filesystem::path& path,
    std::string_view content
  )
  {
    std::error_code directoryError;
    std::filesystem::create_directories(path.parent_path(), directoryError);
    if(directoryError){
      Logger::error("Failed to write ", path.string(), ": ", directoryError.message());
      return false;
    }

    // A temporary file of its own, created readable by its owner only
    std::string tmpTemplate = path.string() + ".tmp-XXXXXX";
    std::vector<char> tmpPath(tmpTemplate.begin(), tmpTemplate.end());
    tmpPath.push_back('\0');

    const int fd = ::mkostemp(tmpPath.data(), O_CLOEXEC);
    if(fd < 0){
      Logger::error("Failed to write ", path.string(), ": ", std::strerror(errno));
      return false;
    }

    // The data must be on disk before the rename makes it the file's content
    bool written = writeAll(fd, content) && ::fsync(fd) == 0;
    int writeErrno = errno;

    // Errors of delayed writes may only show when closing
    if(::close(fd) != 0 && written){
      written = false;
      writeErrno = errno;
    }

    if(!written || ::rename(tmpPath.data(), path.c_str()) != 0){
      if(written){
        writeErrno = errno;
      }

      ::unlink(tmpPath.data());
      Logger::error("Failed to write ", path.string(), ": ", std::strerror(writeErrno));
      return false;
    }

    // The rename itself, durable once the directory is synced
    const int directoryFd = ::open(path.parent_path().empty() ? "." : path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if(directoryFd >= 0){
      ::fsync(directoryFd);
      ::close(directoryFd);
    }

    return true;
  }
}
