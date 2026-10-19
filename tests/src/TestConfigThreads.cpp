#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>

using namespace Halomux;
namespace fs = std::filesystem;


/**
 * @brief The setup's web UI threads write the bridge address and credentials while others read
 *        them, and settings are changed from several threads. Run with ThreadSanitizer.
 */
int main()
{
  const fs::path root = fs::temp_directory_path() / "halomux-test-config-threads";
  fs::remove_all(root);
  fs::create_directories(root);

  {
    Core::Config config(root);
    std::atomic<bool> stop{false};

    std::thread writer([&](){
      for(int i = 0; i < 200 && !stop; i++){
        config.setBridge("192.168.0." + std::to_string(i % 200 + 10), "aabbccddeeff0011", false);
        config.setCredentials({"user" + std::to_string(i), "00112233445566778899aabbccddeeff"});
      }
      stop = true;
    });

    std::vector<std::thread> readers;
    for(int r = 0; r < 3; r++){
      readers.emplace_back([&](){
        size_t length = 0;
        while(!stop){
          const auto address = config.bridgeAddress();
          const auto credentials = config.credentials();
          length += address.value_or("").size() + (credentials.has_value() ? credentials->username().size() : 0);
          (void)config.initialSetupOk();
          (void)config.profileName();
          (void)config.boundBackendIP();
          (void)config.restoreToken();
        }
        (void)length;
      });
    }

    writer.join();
    for(auto& reader : readers){
      reader.join();
    }
  }

  // Settings changed from several threads at once: the file ends up with the last change, as a
  // slower write of an older one doesn't replace a newer one
  bool lastChangeSaved = true;
  {
    Core::Config config(root);
    std::vector<std::thread> writers;
    for(int w = 0; w < 4; w++){
      writers.emplace_back([&config, w](){
        for(int i = 0; i < 50; i++){
          config.setBrightness(static_cast<float>(w * 50 + i) / 200.f);
        }
      });
    }
    for(auto& writer : writers){
      writer.join();
    }

    const float inMemory = config.brightness();
    Core::Config reloaded(root);
    lastChangeSaved = reloaded.brightness() == inMemory;
  }

  fs::remove_all(root);
  if(!lastChangeSaved){
    Core::Logger::error("The file doesn't hold the last change");
    return 1;
  }

  Core::Logger::log("Config threads: OK");
  return 0;
}
