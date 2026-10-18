#pragma once

// SPDX-License-Identifier: AGPL-3.0-only
//
// Adapted from xdg-dp-start by metamuffin (https://codeberg.org/metamuffin/xdg-dp-start), which is
// licensed under the GNU Affero General Public License version 3 only: see LICENSE-AGPL-3.0.txt.
// The rest of Halomux is under the GNU General Public License version 3 (LICENSE.txt), whose
// section 13 allows this combination.

// Requests a screen capture through the ScreenCast interface of xdg-desktop-portal. xdg-dp-start,
// which this is adapted from, extracted that part of OBS Studio's portal code.

#include <atomic>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <gio/gunixfdlist.h>

#include <Halomux/Core/Config.hpp>
#include <Halomux/Grabber/Monitors.hpp>


namespace Halomux::Grabber
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
      // The portal thread's own GLib context: replies and signals of this capture are only
      // dispatched there, never to another capture's thread
      GMainContext* context{nullptr};
      GCancellable* cancellable{nullptr};
      char* sessionHandle{nullptr};
      // Pending portal request, whose Response signal is awaited (portal thread)
      std::string requestPath;
      guint responseSignalId{0};
      std::vector<PortalStream> streams;
      // Ignores the saved restore token, so the desktop asks for the monitors again
      bool askForMonitors{false};
      // Token received from the portal, saved by the grabber once capture works
      std::optional<std::string> restoreToken;
      uint32_t pwFd{0};
      char cursorVisible;
      std::promise<bool> fdReadyPromise;
      bool fdReadyResolved{false};
      // Set with a failed fdReadyPromise when the user cancelled the dialog or the capture was
      // aborted, as opposed to a portal that failed
      bool cancelled{false};
      // The portal thread runs until the grabber stops, so that teardown happens in it
      std::atomic<bool> running{true};
      Core::Config* config;
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

    /**
     * @brief Gives up the screen selection: closes the pending request (and its dialog), fails the capture (portal thread)
     *
     */
    static void abort(
      Capture* capture
    );


    /**
     * @brief Ends the capture after a request was answered with a failure (portal thread)
     *
     * @param capture Capture
     * @param response Response code of the request: 1 when the user cancelled it
     */
    static void rejectCapture(
      Capture* capture,
      uint32_t response
    );


    /**
     * @brief Closes the pending request and the session, cancels the calls in flight (portal thread)
     *
     */
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

    // Awaits the Response signal of a request, replacing the former one
    static void subscribeToResponse(
      Capture* capture,
      const std::string& path,
      GDBusSignalCallback callback
    );

    static void unsubscribeResponse(
      Capture* capture
    );


    // Closes the pending request, if any, and stops awaiting its Response
    static void closeRequest(
      Capture* capture
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
