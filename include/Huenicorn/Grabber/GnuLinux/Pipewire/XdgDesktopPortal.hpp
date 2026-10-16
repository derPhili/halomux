#pragma once

#include <atomic>

/*
  This code is an adapation from https://codeberg.org/metamuffin/xdg-dp-start/src/branch/master/xdg-dp-start.c
  It consists on an extraction from OBS Source code achieving the call and management of a xdg-portal capture query
  As I spent days to do try to achieve the same result in a less concise way, I feel more confident using this one and give it some personnal changes
*/


#include <future>
#include <optional>
#include <string>
#include <vector>

#include <gio/gunixfdlist.h>

#include <Huenicorn/Core/Config.hpp>
#include <Huenicorn/Grabber/Monitors.hpp>


namespace Huenicorn::Grabber
{
  class XdgDesktopPortal
  {
  public:
    // Type definitions
    using StringPair = std::pair<std::string, std::string>;

    // Enums
    enum CaptureType
    {
      Monitor = 1 << 0,
      //Window = 1 << 1,
      //Virtual = 1 << 2,
    };


    enum CursorMode
    {
      Hidden = 1 << 0,
      Embedded = 1 << 1,
      Metadata = 1 << 2,
    };


    enum class CreatePathTokenType
    {
      Request = 0,
      Session = 1
    };


    // Structures
    /**
     * @brief Pipewire node of a captured monitor
     *
     */
    struct PortalStream
    {
      uint32_t node{0};
      // Logical position and size, if the portal tells them
      std::optional<MonitorGeometry> geometry;
    };


    struct Capture
    {
      CaptureType captureType;
      GCancellable* cancellable{nullptr};
      char* sessionHandle{nullptr};
      std::vector<PortalStream> streams;
      // Ignores the saved restore token, so the desktop asks for the monitors again
      bool askForMonitors{false};
      // Token received from the portal, saved by the grabber once capture works
      std::optional<std::string> restoreToken;
      uint32_t pwFd{0};
      char cursorVisible;
      std::promise<bool> fdReadyPromise;
      bool fdReadyResolved{false};
      std::atomic<bool> updateXdgContext{true};
      Core::Config* config;
    };


    struct DbusCallData
    {
      Capture* capture;
      std::string requestPath;
      guint signalId;
      gulong cancelledId;
    };

    // Public methods
    static void createSession(
      Capture* capture
    );

    static bool initScreencastCapture(
      Capture* capture
    );

    static void screencastPortalDesktopCaptureCreate(
      Capture* capture,
      CaptureType captureType,
      bool cursorVisible
    );

    /**
     * @brief Fulfills the capture's fd promise once and stops the portal context loop
     *
     * @param capture Capture to resolve
     * @param succeeded Whether a Pipewire fd was obtained
     */
    static void resolveCapture(
      Capture* capture,
      bool succeeded
    );

    static void screencastPortalCaptureDestroy(
      Capture* capture
    );


    // private methods
  private:
    static void ensureConnection();

    static std::string getSenderName();

    static GDBusConnection* portalGetDbusConnection();

    static StringPair portalCreatePath(
      CreatePathTokenType createPathTokenType
    );

    static void ensureScreencastPortalProxy();

    static GDBusProxy* getScreencastPortalProxy();

    static uint32_t getAvailableCursorModes();

    static uint32_t getScreencastVersion();

    static void onCancelledCallback(
      GCancellable* cancellable,
      void* data
    );

    static DbusCallData* subscribeToSignal(
      Capture* capture,
      const char* path,
      GDBusSignalCallback callback
    );

    static void dbusCallDataFree(
      DbusCallData* call
    );

    static void onPipewireRemoteOpenedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    );

    static void openPipewireRemote(
      Capture* capture
    );

    static void onStartResponseReceivedCallback(
      GDBusConnection* connection,
      const char* sender_name,
      const char* object_path,
      const char* interfaceName,
      const char* signalName,
      GVariant* parameters,
      void* userData
    );

    static void onStartedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    );

    static void start(
      Capture* capture
    );

    static void onSelectSourceResponseReceivedCallback(
      GDBusConnection* connection,
      const char* senderName,
      const char* objectPath,
      const char* interfaceName,
      const char* signalName,
      GVariant* parameters,
      void* userData
    );

    static void onSourceSelectedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    );

    static void selectSource(
      Capture* capture
    );

    static void onCreateSessionResponseReceivedCallback(
      GDBusConnection* connection,
      const char* senderName,
      const char* objectPath,
      const char* interfaceName,
      const char* signalName,
      GVariant* parameters,
      void* userData
    );

    static void onSessionCreatedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    );

  private:
    // Attributes
    static GDBusConnection* m_connection;
    static GDBusProxy* m_screencastProxy;

    // Plain constants: they must stay valid whatever the static destruction order
    static constexpr const char* ObjectPath = "/org/freedesktop/portal/desktop";
    static constexpr const char* BusName = "org.freedesktop.portal.Desktop";
  };
}
