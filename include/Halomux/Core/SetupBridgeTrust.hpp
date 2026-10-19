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
    // A bridge the setup validated
    struct Bridge
    {
      std::string address;
      // The name its certificate carries
      std::string bridgeId;
      // Whether its certificate passed the check of the Hue CA, now or before
      bool verified{false};
    };


    /**
     * @brief Records the bridge the setup validated, replacing the former one
     *
     * @param bridge The bridge, trusted at once if verified
     */
    void validated(
      const Bridge& bridge
    )
    {
      std::lock_guard lock(m_mutex);
      m_bridge = bridge;
      m_trusted = bridge.verified;
    }


    /**
     * @brief Forgets the validated bridge, such as after an address that isn't one
     */
    void invalidate()
    {
      std::lock_guard lock(m_mutex);
      m_bridge.reset();
      m_trusted = false;
    }


    /**
     * @brief Trusts the validated bridge as the user confirmed it, though its certificate isn't
     *        signed by the Hue CA
     *
     * @param address Address the user confirmed: another validated meanwhile isn't confirmed
     * @return std::optional<Bridge> The bridge now trusted, also if it already was; nothing if
     *         none was validated at that address
     */
    std::optional<Bridge> acceptUnverified(
      const std::string& address
    )
    {
      std::lock_guard lock(m_mutex);
      if(!m_bridge.has_value() || m_bridge->address != address){
        return std::nullopt;
      }

      m_trusted = true;
      return m_bridge;
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
      return m_trusted && m_bridge.has_value() && m_bridge->address == address;
    }

  private:
    mutable std::mutex m_mutex;
    std::optional<Bridge> m_bridge;
    bool m_trusted{false};
  };
}
