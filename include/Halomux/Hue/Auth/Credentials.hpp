#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <Halomux/Serialization/JsonSerializer.hpp>

namespace Halomux::Hue::Auth
{
  /**
   * @brief Wrapper around Hue bridge user authentication data
   * 
   */
  class Credentials
  {
    public:
    // Constructor
    Credentials()
    {}


    /**
     * @brief Credentials constructor
     * 
     * @param username Hue bridge username
     * @param clientkey Hue bridge clientkey
     */
    Credentials(
      const std::string& username,
      const std::string& clientkey
    );


    // Getters
    /**
     * @brief Returns the registered Hue bridge username
     * 
     * @return const std::string& Hue bridge username
     */
    const std::string& username() const;


    /**
     * @brief Returns the registered Hue bridge clientkey
     * 
     * @return const std::string&  Hue bridge clientkey
     */
    const std::string& clientkey() const;


    /**
     * @brief Helper to get byte representation of username
     * 
     * @return std::vector<unsigned char> username as bytes
     */
    std::vector<unsigned char> usernameBytes() const;


    /**
     * @brief Helper to get byte representation of clientkey
     *
     * @return std::vector<unsigned char> clientkey as bytes
     * @throw std::runtime_error The clientkey isn't hexadecimal
     */
    std::vector<unsigned char> clientkeyBytes() const;


    /**
     * @brief Tells whether the clientkey is hexadecimal, as the pre-shared key of the streaming connection
     *
     * @return true Non-empty, of even length, only hexadecimal digits
     */
    bool hasValidClientkey() const;

  private:
    // Attributes
    std::string m_username;
    std::string m_clientkey;

    template<typename T>
    friend struct Halomux::Serialization::JsonSerializer;
  };
}
