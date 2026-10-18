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
 *        them. Run with ThreadSanitizer.
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
        config.setBridgeAddress("192.168.1." + std::to_string(i % 200 + 10));
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

  fs::remove_all(root);
  Core::Logger::log("Config threads: OK");
  return 0;
}
