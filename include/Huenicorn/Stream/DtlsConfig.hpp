#pragma once

#include <stop_token>
#include <string>

#include <Huenicorn/Hue/Auth/Credentials.hpp>


namespace Huenicorn::Stream
{
  struct DtlsConfig
  {
    const Hue::Auth::Credentials credentials;
    const std::string address;
    const std::string port;
    const std::string hostname;
    const unsigned handshakeAttempts;
    // Stops the handshake between attempts
    std::stop_token stopToken{};
  };
}
