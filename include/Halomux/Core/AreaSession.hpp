#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include <Halomux/Hue/Api/BridgeApi.hpp>
#include <Halomux/Stream/IStreamer.hpp>


namespace Halomux::Core
{
  /**
   * @brief The entertainment area streamed to, and the stream to it
   *
   * Selects an area atomically: its lamps are loaded before anything changes on the bridge.
   * Recovers on its own: while no area could be selected (bridge offline at startup, no area
   * yet) it keeps trying, and when the bridge stopped the stream (reboot, another application
   * done) it starts it again and renews the connection.
   */
  class AreaSession
  {
  public:
    // Creates the streamer of an area
    using StreamerFactory = std::function<std::unique_ptr<Stream::IStreamer>(const std::string& areaId)>;
    // Called once an area is selected, with its lamps, while no other selection runs
    using SelectedCallback = std::function<void(const std::string& areaId, const Hue::Api::ChannelMembers& members)>;
    // Called once the selected area is gone, such as deleted in the Hue app, before another one
    // is selected
    using LostCallback = std::function<void()>;


    /**
     * @brief AreaSession constructor
     *
     * @param bridge The bridge
     * @param streamerFactory Creates the streamer of an area
     * @param onSelected Called once an area is selected
     * @param onLost Called once the selected area is gone
     * @param watchInterval Interval of the checks (tick()), 0 for none
     */
    AreaSession(
      std::unique_ptr<Hue::Api::BridgeApi> bridge,
      StreamerFactory streamerFactory,
      SelectedCallback onSelected,
      LostCallback onLost,
      std::chrono::milliseconds watchInterval
    );


    /**
     * @brief Stops the checks and the streaming
     *
     */
    ~AreaSession();


    /**
     * @brief Selects the area saved in the profile, the first one if it's gone
     *
     * If the bridge is unreachable or has no area, the checks keep trying.
     *
     * @param savedAreaId Area of the profile, possibly empty
     */
    void start(
      const std::string& savedAreaId
    );


    /**
     * @brief Switches to another area
     *
     * @param areaId Entertainment area
     * @return true Switched
     * @return false Unknown area, or its lamps couldn't be loaded: nothing changed
     */
    bool select(
      const std::string& areaId
    );


    /**
     * @brief One check: selects an area if none is, restarts the stream if the bridge stopped it
     *
     */
    void tick();


    /**
     * @brief Streams the colors of a frame, if an area is selected
     *
     * @param channels Colors per channel
     */
    void stream(
      const Hue::Api::ChannelStreams& channels
    );


    /**
     * @brief Stops the checks, then the streaming on the bridge
     *
     */
    void stop();


    /**
     * @brief Returns the entertainment areas of the bridge
     *
     * @return Hue::Api::EntertainmentConfigurations Areas, empty until the bridge answered
     */
    Hue::Api::EntertainmentConfigurations areas() const;


    /**
     * @brief Returns the selected area
     *
     * @return std::optional<std::string> Area id, nothing if none is selected
     */
    std::optional<std::string> currentAreaId() const;


    /**
     * @brief Loads the areas from the bridge again, such as ones created in the Hue app since
     *
     * @return Hue::Api::EntertainmentConfigurations Areas, the known ones if the bridge is unreachable
     */
    Hue::Api::EntertainmentConfigurations refreshAreas();


  private:
    // With m_operationMutex held
    bool _loadAreas();
    // Removes the streamer, for the caller to end it outside the lock
    std::unique_ptr<Stream::IStreamer> _takeStreamer();
    // Selects the saved area, or another one if it no longer exists
    void _ensureSelection(bool loadAreas = true);
    bool _select(const std::string& areaId);

    void _watch(
      std::stop_token stopToken
    );


    std::unique_ptr<Hue::Api::BridgeApi> m_bridge;
    StreamerFactory m_streamerFactory;
    SelectedCallback m_onSelected;
    LostCallback m_onLost;
    std::chrono::milliseconds m_watchInterval;

    // Serializes the operations talking to the bridge: selections and checks
    std::mutex m_operationMutex;
    std::string m_savedAreaId;
    bool m_unreachableReported{false};
    bool m_noAreaReported{false};

    // The state the light manager reads, quick to lock
    mutable std::mutex m_stateMutex;
    Hue::Api::EntertainmentConfigurations m_areas;
    std::optional<std::string> m_currentAreaId;

    // The streaming loop only waits for this one
    std::mutex m_streamerMutex;
    std::unique_ptr<Stream::IStreamer> m_streamer;

    std::mutex m_wakeMutex;
    std::condition_variable_any m_wake;
    // Declared last: stopped first
    std::jthread m_watchdog;
  };
}
