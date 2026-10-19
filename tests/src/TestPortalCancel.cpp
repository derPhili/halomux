#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <thread>

#include <gio/gio.h>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Core/Logger.hpp>
#include <Halomux/Grabber/GnuLinux/Pipewire/PipewireGrabber.hpp>

using namespace Halomux;
using namespace std::chrono_literals;


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


  constexpr const char* PortalXml = R"(
<node>
  <interface name="org.freedesktop.portal.ScreenCast">
    <method name="CreateSession">
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <method name="SelectSources">
      <arg type="o" name="session_handle" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <method name="Start">
      <arg type="o" name="session_handle" direction="in"/>
      <arg type="s" name="parent_window" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
    <property name="AvailableSourceTypes" type="u" access="read"/>
    <property name="AvailableCursorModes" type="u" access="read"/>
    <property name="version" type="u" access="read"/>
  </interface>
</node>
)";

  constexpr const char* CloseXml = R"(
<node>
  <interface name="org.freedesktop.portal.Request">
    <method name="Close"/>
  </interface>
  <interface name="org.freedesktop.portal.Session">
    <method name="Close"/>
  </interface>
</node>
)";


  /**
   * @brief The ScreenCast interface of xdg-desktop-portal, answering Start as the test asks:
   *        cancelled by the user, refused, or never (the dialog stays open)
   *
   */
  class FakePortal
  {
  public:
    enum class StartAnswer
    {
      Cancelled,
      Refused,
      Never
    };


    FakePortal()
    {
      m_thread = std::thread([this](){_run();});
      m_ready.get_future().wait();
    }


    ~FakePortal()
    {
      g_main_loop_quit(m_loop);
      m_thread.join();
    }


    void setStartAnswer(StartAnswer answer)
    {
      std::lock_guard lock(m_mutex);
      m_startAnswer = answer;
      m_startReceived = false;
      m_closedPaths.clear();
    }


    bool startReceived() const
    {
      return m_startReceived;
    }


    // Paths whose Close was called: requests and sessions
    std::set<std::string> closedPaths() const
    {
      std::lock_guard lock(m_mutex);
      return m_closedPaths;
    }


    std::string lastSession() const
    {
      std::lock_guard lock(m_mutex);
      return m_lastSession;
    }


    // Stops answering anything, such as a portal still starting up, until thawed
    void freeze()
    {
      auto frozen = std::make_shared<std::promise<void>>();
      m_thawed = std::make_shared<std::promise<void>>();
      auto thawed = m_thawed->get_future().share();
      g_main_context_invoke(m_context, [](void* data) -> gboolean {
        auto* waits = static_cast<std::pair<std::shared_ptr<std::promise<void>>, std::shared_future<void>>*>(data);
        waits->first->set_value();
        waits->second.wait();
        delete waits;
        return G_SOURCE_REMOVE;
      }, new std::pair<std::shared_ptr<std::promise<void>>, std::shared_future<void>>(frozen, thawed));
      frozen->get_future().wait();
    }


    void thaw()
    {
      m_thawed->set_value();
    }

  private:
    void _run()
    {
      GMainContext* context = g_main_context_new();
      m_context = context;
      g_main_context_push_thread_default(context);
      m_loop = g_main_loop_new(context, FALSE);

      m_portalInfo = g_dbus_node_info_new_for_xml(PortalXml, nullptr);
      m_closeInfo = g_dbus_node_info_new_for_xml(CloseXml, nullptr);

      // A connection of its own, as the real portal has: a restarted portal gets a new name
      g_autofree char* address = g_dbus_address_get_for_bus_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
      m_connection = g_dbus_connection_new_for_address_sync(address, static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr, nullptr);

      static const GDBusInterfaceVTable portalVTable{_onPortalCall, _onGetProperty, nullptr, {}};
      g_dbus_connection_register_object(m_connection, "/org/freedesktop/portal/desktop", m_portalInfo->interfaces[0], &portalVTable, this, nullptr, nullptr);

      g_bus_own_name_on_connection(m_connection, "org.freedesktop.portal.Desktop", G_BUS_NAME_OWNER_FLAGS_NONE, [](GDBusConnection*, const char*, void* self){
        static_cast<FakePortal*>(self)->m_ready.set_value();
      }, nullptr, this, nullptr);

      g_main_loop_run(m_loop);

      // Releases its name, as a portal that ends does
      g_dbus_connection_close_sync(m_connection, nullptr, nullptr);
      g_object_unref(m_connection);
      g_dbus_node_info_unref(m_portalInfo);
      g_dbus_node_info_unref(m_closeInfo);
      g_main_loop_unref(m_loop);
      g_main_context_pop_thread_default(context);
      g_main_context_unref(context);
    }


    // Object path of a request or a session, as the portal names it after the caller's token
    static std::string _path(const char* type, const char* sender, const char* token)
    {
      std::string name(sender + 1);
      for(char& c : name){
        if(c == '.'){
          c = '_';
        }
      }

      return std::string("/org/freedesktop/portal/desktop/") + type + "/" + name + "/" + token;
    }


    void _registerClosable(const std::string& path, int interfaceIndex)
    {
      static const GDBusInterfaceVTable closeVTable{_onClose, nullptr, nullptr, {}};
      g_dbus_connection_register_object(m_connection, path.c_str(), m_closeInfo->interfaces[interfaceIndex], &closeVTable, this, nullptr, nullptr);
    }


    void _respond(const std::string& sender, const std::string& requestPath, uint32_t response, GVariant* results)
    {
      g_dbus_connection_emit_signal(m_connection, sender.c_str(), requestPath.c_str(), "org.freedesktop.portal.Request", "Response", g_variant_new("(u@a{sv})", response, results), nullptr);
    }


    static void _onPortalCall(GDBusConnection*, const char* sender, const char*, const char*, const char* method, GVariant* parameters, GDBusMethodInvocation* invocation, void* userData)
    {
      auto* self = static_cast<FakePortal*>(userData);

      g_autoptr(GVariant) options = g_variant_get_child_value(parameters, g_variant_n_children(parameters) - 1);
      const char* handleToken = "none";
      g_variant_lookup(options, "handle_token", "&s", &handleToken);
      const std::string requestPath = _path("request", sender, handleToken);
      self->_registerClosable(requestPath, 0);

      g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", requestPath.c_str()));

      GVariantBuilder results;
      g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);

      if(std::string(method) == "CreateSession"){
        const char* sessionToken = "none";
        g_variant_lookup(options, "session_handle_token", "&s", &sessionToken);
        const std::string sessionPath = _path("session", sender, sessionToken);
        self->_registerClosable(sessionPath, 1);
        {
          std::lock_guard lock(self->m_mutex);
          self->m_lastSession = sessionPath;
        }

        g_variant_builder_add(&results, "{sv}", "session_handle", g_variant_new_string(sessionPath.c_str()));
        self->_respond(sender, requestPath, 0, g_variant_builder_end(&results));
        return;
      }

      if(std::string(method) == "SelectSources"){
        self->_respond(sender, requestPath, 0, g_variant_builder_end(&results));
        return;
      }

      // Start: the dialog
      StartAnswer answer;
      {
        std::lock_guard lock(self->m_mutex);
        answer = self->m_startAnswer;
      }
      self->m_startReceived = true;

      if(answer == StartAnswer::Never){
        g_variant_builder_clear(&results);
        return;
      }

      self->_respond(sender, requestPath, answer == StartAnswer::Cancelled ? 1 : 2, g_variant_builder_end(&results));
    }


    static GVariant* _onGetProperty(GDBusConnection*, const char*, const char*, const char*, const char* property, GError**, void*)
    {
      // Monitors, embedded cursor, restore tokens
      if(std::string(property) == "version"){
        return g_variant_new_uint32(4);
      }

      return g_variant_new_uint32(std::string(property) == "AvailableCursorModes" ? 2 : 1);
    }


    static void _onClose(GDBusConnection*, const char*, const char* path, const char*, const char*, GVariant*, GDBusMethodInvocation* invocation, void* userData)
    {
      auto* self = static_cast<FakePortal*>(userData);
      {
        std::lock_guard lock(self->m_mutex);
        self->m_closedPaths.insert(path);
      }
      g_dbus_method_invocation_return_value(invocation, nullptr);
    }


    std::thread m_thread;
    std::promise<void> m_ready;
    GMainContext* m_context{nullptr};
    std::shared_ptr<std::promise<void>> m_thawed;
    GMainLoop* m_loop{nullptr};
    GDBusConnection* m_connection{nullptr};
    GDBusNodeInfo* m_portalInfo{nullptr};
    GDBusNodeInfo* m_closeInfo{nullptr};

    mutable std::mutex m_mutex;
    StartAnswer m_startAnswer{StartAnswer::Never};
    std::atomic<bool> m_startReceived{false};
    std::set<std::string> m_closedPaths;
    std::string m_lastSession;
  };


  enum class Outcome
  {
    Started,
    Cancelled,
    Unavailable,
    OtherError
  };


  Outcome select(Core::Config& config, std::stop_token stopToken = {})
  {
    try{
      Grabber::PipewireGrabber grabber(&config, true, stopToken);
      return Outcome::Started;
    }
    catch(const Grabber::GrabberCancelled&){
      return Outcome::Cancelled;
    }
    catch(const Grabber::GrabberUnavailable&){
      return Outcome::Unavailable;
    }
    catch(const std::exception& e){
      Core::Logger::error(e.what());
      return Outcome::OtherError;
    }
  }


  // Close is called without waiting for an answer: give it a moment to arrive
  bool closed(const FakePortal& portal, const std::string& path)
  {
    for(int i = 0; i < 100; i++){
      if(portal.closedPaths().contains(path)){
        return true;
      }
      std::this_thread::sleep_for(10ms);
    }

    return false;
  }
}


/**
 * @brief Checks how the screen selection ends against a portal: run in a private session bus
 *        (dbus-run-session), whose portal the test provides
 */
int main()
{
  if(!std::getenv("DBUS_SESSION_BUS_ADDRESS")){
    Core::Logger::error("Run through dbus-run-session: the test needs a session bus of its own");
    return 1;
  }

  std::optional<FakePortal> runningPortal;
  FakePortal& portal = runningPortal.emplace();
  Core::Config config(std::filesystem::temp_directory_path() / "halomux-test-portal");

  // The user cancels the dialog: a choice, not a failure
  portal.setStartAnswer(FakePortal::StartAnswer::Cancelled);
  expect("Cancelled by the user", select(config) == Outcome::Cancelled);
  expect("Cancelled selection closes its session", closed(portal, portal.lastSession()));

  // The portal refuses: a failure worth trying again
  portal.setStartAnswer(FakePortal::StartAnswer::Refused);
  expect("Refused by the portal", select(config) == Outcome::Unavailable);

  // A stop while the dialog is open, such as SIGTERM at login: aborted at once
  {
    portal.setStartAnswer(FakePortal::StartAnswer::Never);
    std::stop_source stop;
    auto outcome = std::async(std::launch::async, [&](){return select(config, stop.get_token());});

    while(!portal.startReceived()){
      std::this_thread::sleep_for(10ms);
    }

    const auto stoppedAt = std::chrono::steady_clock::now();
    stop.request_stop();
    const bool ended = outcome.wait_for(5s) == std::future_status::ready;
    expect("A stop ends the open dialog's wait", ended);
    if(ended){
      expect("A stop counts as cancelled", outcome.get() == Outcome::Cancelled);
      expect("A stop ends it within a second", std::chrono::steady_clock::now() - stoppedAt < 1s);
    }
    expect("A stop closes the session", closed(portal, portal.lastSession()));
  }

  // A stop while the portal doesn't answer yet, such as one still starting at login: aborted at
  // once too, not after the D-Bus timeout
  {
    portal.freeze();
    std::stop_source stop;
    auto outcome = std::async(std::launch::async, [&](){return select(config, stop.get_token());});
    std::this_thread::sleep_for(200ms);

    const auto stoppedAt = std::chrono::steady_clock::now();
    stop.request_stop();
    const bool ended = outcome.wait_for(5s) == std::future_status::ready;
    portal.thaw();
    expect("A stop ends the wait for a portal that doesn't answer", ended);
    if(ended){
      expect("A stop before the portal answered counts as cancelled", outcome.get() == Outcome::Cancelled);
      expect("A stop ends the wait for the portal within a second", std::chrono::steady_clock::now() - stoppedAt < 1s);
    }
  }

  // The portal restarts, such as after an update, between two selections: the next one reaches
  // the new portal, not the name the former one had
  {
    runningPortal.reset();
    FakePortal& restarted = runningPortal.emplace();
    restarted.setStartAnswer(FakePortal::StartAnswer::Cancelled);
    expect("Selection after a portal restart reaches the new portal", select(config) == Outcome::Cancelled);
  }

  if(failures > 0){
    Core::Logger::error(failures, " check(s) failed");
    return 1;
  }

  Core::Logger::log("Portal cancel: OK");
  return 0;
}
