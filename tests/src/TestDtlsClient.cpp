#include <Halomux/Core/Logger.hpp>
#include <Halomux/Stream/DtlsClient.hpp>

using namespace Halomux;


/**
 * @brief Connection attempts to a bridge that doesn't answer, as the streamer retries every 5 s
 *
 * Run with AddressSanitizer: LeakSanitizer reports the TLS contexts of every attempt if they
 * aren't released.
 */
int main()
{
  const Hue::Auth::Credentials credentials{"test-user", "00112233445566778899aabbccddeeff"};

  for(int i = 0; i < 5; i++){
    // A closed local port refuses at once
    Stream::DtlsClient client(Stream::DtlsConfig{
      .credentials = credentials,
      .address = "127.0.0.1",
      .port = "9",
      .hostname = "Hue",
      .handshakeAttempts = 1
    });

    try{
      client.init();
    }
    catch(const std::exception&){
      // Expected: nothing listens
    }
  }

  Core::Logger::log("DTLS client: OK");
  return 0;
}
