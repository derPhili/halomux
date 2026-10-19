#pragma once

#include <mutex>
#include <optional>
#include <string>


namespace Halomux::Core
{
  /**
   * @brief Whether the setup may pair with the bridge whose address it validated
   *
   * Any device of the network can answer the bridge search. One with a certificate of its own
   * could pass the pairing on to the real bridge and keep the credentials, which control the
   * whole Hue system. A bridge whose certificate the Hue CA signed is trusted; older bridges have
   * certificates of their own too, so for those the user confirms that the address is their
   * bridge first. Used from the setup's request threads.
   */
  class SetupBridgeTrust
  {
  public:
    /**
     * @brief Records the bridge the setup validated, replacing the former one
     *
     * @param address Its address
     * @param certificateVerified Whether the Hue CA signed its certificate
     */
    void validated(
      const std::string& address,
      bool certificateVerified
    )
    {
      std::lock_guard lock(m_mutex);
      m_address = address;
      m_trusted = certificateVerified;
    }


    /**
     * @brief Forgets the validated bridge, such as after an address that isn't one
     */
    void invalidate()
    {
      std::lock_guard lock(m_mutex);
      m_address.reset();
      m_trusted = false;
    }


    /**
     * @brief Trusts the validated bridge as the user confirmed it, though its certificate isn't
     *        signed by the Hue CA
     *
     * @return true A bridge awaited the confirmation
     * @return false None was validated, or it was already trusted
     */
    bool acceptUnverified()
    {
      std::lock_guard lock(m_mutex);
      if(!m_address.has_value() || m_trusted){
        return false;
      }

      m_trusted = true;
      return true;
    }


    /**
     * @brief Tells whether the setup may pair with a bridge
     *
     * @param address Its address
     * @return true It's the validated bridge, verified or confirmed by the user
     */
    bool trusted(
      const std::string& address
    ) const
    {
      std::lock_guard lock(m_mutex);
      return m_trusted && m_address == address;
    }

  private:
    mutable std::mutex m_mutex;
    std::optional<std::string> m_address;
    bool m_trusted{false};
  };
}
