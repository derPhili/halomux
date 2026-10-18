#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>
#include <sys/stat.h>

#include <Halomux/Core/AtomicFile.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
namespace fs = std::filesystem;


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


  std::string read(const fs::path& path)
  {
    std::ifstream file(path);
    std::stringstream content;
    content << file.rdbuf();
    return content.str();
  }


  // Files of the directory but the one written: temporary files left behind
  size_t strayFiles(const fs::path& directory, const fs::path& file)
  {
    size_t count = 0;
    for(const auto& entry : fs::directory_iterator(directory)){
      count += entry.path() != file;
    }

    return count;
  }


  mode_t permissions(const fs::path& path)
  {
    struct stat info{};
    stat(path.c_str(), &info);
    return info.st_mode & 0777;
  }
}


/**
 * @brief Checks that a file is only ever replaced by a complete content
 *
 */
int main()
{
  const fs::path root = fs::temp_directory_path() / "halomux-test-atomic-file";
  fs::remove_all(root);
  const fs::path file = root / "sub" / "data.json";

  expect("Writes, creating the directories", Core::writeFileAtomically(file, "first\n") && read(file) == "first\n");
  expect("Readable by its owner only", permissions(file) == 0600);
  expect("Replaces", Core::writeFileAtomically(file, "second\n") && read(file) == "second\n");
  expect("No temporary file left", strayFiles(file.parent_path(), file) == 0);

  {
    // A full disk: the write fails once the file exceeds the limit, the old content stays
    std::signal(SIGXFSZ, SIG_IGN);
    rlimit previous{};
    getrlimit(RLIMIT_FSIZE, &previous);
    rlimit limited = previous;
    limited.rlim_cur = 64;
    setrlimit(RLIMIT_FSIZE, &limited);

    const bool written = Core::writeFileAtomically(file, std::string(4096, 'x'));

    setrlimit(RLIMIT_FSIZE, &previous);

    expect("Failed write reported", !written);
    expect("Failed write keeps the old content", read(file) == "second\n");
    expect("Failed write leaves no temporary file", strayFiles(file.parent_path(), file) == 0);
  }

  {
    // Concurrent writers: the file always ends up with one of the contents, whole
    std::vector<std::string> contents;
    for(int i = 0; i < 8; i++){
      contents.push_back(std::string(20000, static_cast<char>('a' + i)));
    }

    std::vector<std::thread> writers;
    bool allWritten = true;
    std::vector<char> results(contents.size(), 0);
    for(size_t i = 0; i < contents.size(); i++){
      writers.emplace_back([&, i](){
        bool ok = true;
        for(int round = 0; round < 20; round++){
          ok = Core::writeFileAtomically(file, contents[i]) && ok;
        }
        results[i] = ok;
      });
    }
    for(auto& writer : writers){
      writer.join();
    }
    for(char result : results){
      allWritten = allWritten && result;
    }

    const std::string final = read(file);
    bool whole = false;
    for(const auto& content : contents){
      whole = whole || final == content;
    }

    expect("Concurrent writes succeed", allWritten);
    expect("Concurrent writes leave a whole content", whole);
    expect("Concurrent writes leave no temporary file", strayFiles(file.parent_path(), file) == 0);
  }

  {
    // A file where the directory should be
    std::ofstream(root / "blocker") << "x";
    expect("Impossible directory reported, not thrown", !Core::writeFileAtomically(root / "blocker" / "data.json", "x"));
  }

  fs::remove_all(root);

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Atomic file: OK");
  return 0;
}
