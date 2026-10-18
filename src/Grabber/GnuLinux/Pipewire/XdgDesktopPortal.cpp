// SPDX-License-Identifier: AGPL-3.0-only
//
// Adapted from xdg-dp-start by metamuffin (https://codeberg.org/metamuffin/xdg-dp-start), which is
// licensed under the GNU Affero General Public License version 3 only: see LICENSE-AGPL-3.0.txt.
// The rest of Halomux is under the GNU General Public License version 3 (LICENSE.txt), whose
// section 13 allows this combination.

// Requests a screen capture through the ScreenCast interface of xdg-desktop-portal. xdg-dp-start,
// which this is adapted from, extracted that part of OBS Studio's portal code.

#include <Halomux/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.hpp>

#include <algorithm>
#include <future>
#include <sstream>
#include <string>

#include <gio/gunixfdlist.h>

#include <Halomux/Core/Logger.hpp>


namespace Halomux::Grabber
{
  GDBusConnection* XdgDesktopPortal::m_connection = nullptr;
  GDBusProxy* XdgDesktopPortal::m_screencastProxy = nullptr;

    // Implementations

    void XdgDesktopPortal::createSession(
      Capture* capture
    )
    {
      StringPair requestPathAndToken = portalCreatePath(CreatePathTokenType::Request);
      StringPair sessionPathAndToken = portalCreatePath(CreatePathTokenType::Session);

      subscribeToResponse(capture, requestPathAndToken.first, onCreateSessionResponseReceivedCallback);

      GVariantBuilder builder;
      g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(requestPathAndToken.second.c_str()));
      g_variant_builder_add(&builder, "{sv}", "session_handle_token", g_variant_new_string(sessionPathAndToken.second.c_str()));

      g_dbus_proxy_call(getScreencastPortalProxy(), "CreateSession", g_variant_new("(a{sv})", &builder), G_DBUS_CALL_FLAGS_NONE, -1, capture->cancellable, onSessionCreatedCallback, capture);
    }


    bool XdgDesktopPortal::initScreencastCapture(
      Capture* capture
    )
    {
      capture->cancellable = g_cancellable_new();
      GDBusConnection* connection = portalGetDbusConnection();
      if(!connection){
        return false;
      }

      GDBusProxy* proxy = getScreencastPortalProxy();
      if(!proxy){
        return false;
      }

      createSession(capture);

      return true;
    }


    void XdgDesktopPortal::screencastPortalDesktopCaptureCreate(
      Capture* capture,
      CaptureType captureType,
      bool cursorVisible
    )
    {
      capture->captureType = captureType;
      capture->cursorVisible = cursorVisible;

      if(!initScreencastCapture(capture)){
        Core::Logger::error("Could not reach the XDG desktop portal");
        resolveCapture(capture, false);
      }
    }


    void XdgDesktopPortal::resolveCapture(
      Capture* capture,
      bool succeeded
    )
    {
      if(capture->fdReadyResolved){
        return;
      }

      capture->fdReadyResolved = true;
      capture->fdReadyPromise.set_value(succeeded);
    }


    void XdgDesktopPortal::abort(
      Capture* capture
    )
    {
      closeRequest(capture);

      if(capture->cancellable){
        g_cancellable_cancel(capture->cancellable);
      }

      if(!capture->fdReadyResolved){
        capture->cancelled = true;
      }
      resolveCapture(capture, false);
    }


    void XdgDesktopPortal::rejectCapture(
      Capture* capture,
      uint32_t response
    )
    {
      // Response codes of the portal requests: 1 cancelled by the user, 2 ended in another way
      if(response == 1){
        Core::Logger::warn("The screen selection was cancelled");
        if(!capture->fdReadyResolved){
          capture->cancelled = true;
        }
      }
      else{
        Core::Logger::error("The screen capture portal refused the request (response ", response, ")");
      }

      resolveCapture(capture, false);
    }


    void XdgDesktopPortal::screencastPortalCaptureDestroy(
      Capture* capture
    )
    {
      if(!capture){
        return;
      }

      closeRequest(capture);

      std::string interfaceName = "org.freedesktop.portal.Session";
      std::string methodName = "Close";

      if(capture->sessionHandle){
        g_dbus_connection_call(portalGetDbusConnection(), BusName, capture->sessionHandle, interfaceName.c_str(), methodName.c_str(), NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
        g_clear_pointer(&capture->sessionHandle, g_free);
      }

      if(capture->cancellable){
        g_cancellable_cancel(capture->cancellable);
        g_clear_object(&capture->cancellable);
      }
    }


    void XdgDesktopPortal::ensureConnection()
    {
      g_autoptr(GError) error = NULL;
      if(!m_connection){
        m_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

        if(error){
         Core::Logger::warn("error retrieving D-Bus connection: ", std::string(error->message));
         return;
        }
      }
    }


    std::string XdgDesktopPortal::getSenderName()
    {
      ensureConnection();

      // The unique name without its leading colon, owned by the connection
      std::string senderName(g_dbus_connection_get_unique_name(m_connection) + 1);
      std::replace_if(senderName.begin(), senderName.end(), [](char c) { return c == '.'; }, '_');

      return senderName;
    }


    GDBusConnection* XdgDesktopPortal::portalGetDbusConnection()
    {
      ensureConnection();
      return m_connection;
    }


    XdgDesktopPortal::StringPair XdgDesktopPortal::portalCreatePath(
      CreatePathTokenType createPathTokenType
    )
    {
      static uint32_t count[2] = {0, 0};

      std::string type = "";
      switch(createPathTokenType)
      {
        case CreatePathTokenType::Request:
          type = "request";
          break;

        case CreatePathTokenType::Session:
          type = "session";
          break;
      }

      StringPair pathAndToken;
      std::ostringstream oss;
      oss << "halomux";
      oss << ++count[static_cast<int>(createPathTokenType)];
      pathAndToken.second = oss.str();
      pathAndToken.first = std::string(ObjectPath) + '/' + type + '/' + getSenderName() + '/' + pathAndToken.second;

      return pathAndToken;
    }


    void XdgDesktopPortal::ensureScreencastPortalProxy()
    {
      g_autoptr(GError) error = NULL;
      if(!m_screencastProxy){

        std::string interfaceName = "org.freedesktop.portal.ScreenCast";

        // Shared by the captures: its own signals would be dispatched to the context of the
        // thread creating it, which ends with its capture. Only its cached properties are used.
        m_screencastProxy = g_dbus_proxy_new_sync(portalGetDbusConnection(), G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, NULL, BusName, ObjectPath, interfaceName.c_str(), NULL, &error);

        if(error){
          Core::Logger::warn("error retrieving D-Bus proxy: ", std::string(error->message));
          return;
        }
      }
    }


    GDBusProxy* XdgDesktopPortal::getScreencastPortalProxy()
    {
      ensureScreencastPortalProxy();
      return m_screencastProxy;
    }


    uint32_t XdgDesktopPortal::getAvailableCursorModes()
    {
      ensureScreencastPortalProxy();

      if(!m_screencastProxy){
        return 0;
      }

      g_autoptr(GVariant) cachedCursorModes = g_dbus_proxy_get_cached_property(m_screencastProxy, "AvailableCursorModes");
      return cachedCursorModes ? g_variant_get_uint32(cachedCursorModes) : 0;
    }


    uint32_t XdgDesktopPortal::getScreencastVersion()
    {
      g_autoptr(GVariant) cached_version = NULL;
      uint32_t version;

      ensureScreencastPortalProxy();

      if(!m_screencastProxy){
        return 0;
      }

      cached_version = g_dbus_proxy_get_cached_property(m_screencastProxy, "version");
      version = cached_version ? g_variant_get_uint32(cached_version) : 0;

      return version;
    }


    void XdgDesktopPortal::subscribeToResponse(
      Capture* capture,
      const std::string& path,
      GDBusSignalCallback callback
    )
    {
      unsubscribeResponse(capture);

      // The capture outlives the subscription: it's removed in the portal thread, before the
      // grabber ends, and nothing is dispatched once that thread stopped
      capture->requestPath = path;
      std::string interfaceName = "org.freedesktop.portal.Request";
      capture->responseSignalId = g_dbus_connection_signal_subscribe(portalGetDbusConnection(), BusName, interfaceName.c_str(), "Response", capture->requestPath.c_str(), NULL, G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, callback, capture, NULL);
    }


    void XdgDesktopPortal::unsubscribeResponse(
      Capture* capture
    )
    {
      if(capture->responseSignalId != 0){
        g_dbus_connection_signal_unsubscribe(portalGetDbusConnection(), capture->responseSignalId);
        capture->responseSignalId = 0;
      }
    }


    void XdgDesktopPortal::closeRequest(
      Capture* capture
    )
    {
      if(capture->responseSignalId == 0){
        return;
      }

      // A request still awaiting its Response, such as an open screen selection dialog
      std::string interfaceName = "org.freedesktop.portal.Request";
      g_dbus_connection_call(portalGetDbusConnection(), BusName, capture->requestPath.c_str(), interfaceName.c_str(), "Close", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
      unsubscribeResponse(capture);
    }


    void XdgDesktopPortal::onPipewireRemoteOpenedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    )
    {
      Capture* capture = static_cast<Capture*>(userData);
      g_autoptr(GUnixFDList) fdList = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GVariant) result = g_dbus_proxy_call_with_unix_fd_list_finish(G_DBUS_PROXY(source), &fdList, res, &error);

      if(error){
        if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)){
          Core::Logger::error("[pipewire] error retrieving pipewire fd: ", std::string(error->message));
        }

        resolveCapture(capture, false);
        return;
      }

      int fdIndex;
      g_variant_get(result, "(h)", &fdIndex);

      int pwFd = g_unix_fd_list_get(fdList, fdIndex, &error);
      if(error){
        if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)){
          Core::Logger::error("[pipewire] error retrieving pipewire fd: ", std::string(error->message));
        }

        resolveCapture(capture, false);
        return;
      }

      capture->pwFd = static_cast<uint32_t>(pwFd);
      resolveCapture(capture, true);
    }


    void XdgDesktopPortal::openPipewireRemote(
      Capture* capture
    )
    {
      GVariantBuilder builder;
      g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

      g_dbus_proxy_call_with_unix_fd_list(getScreencastPortalProxy(), "OpenPipeWireRemote", g_variant_new("(oa{sv})", capture->sessionHandle, &builder), G_DBUS_CALL_FLAGS_NONE, -1, NULL, capture->cancellable, onPipewireRemoteOpenedCallback, capture);
    }


    void XdgDesktopPortal::onStartResponseReceivedCallback(
      GDBusConnection* /*connection*/,
      const char* /*senderName*/,
      const char* /*objectPath*/,
      const char* /*interfaceName*/,
      const char* /*signalName*/,
      GVariant* parameters,
      void* userData
    )
    {
      Capture* capture = static_cast<Capture*>(userData);
      unsubscribeResponse(capture);

      uint32_t response;
      g_autoptr(GVariant) result = NULL;
      g_variant_get(parameters, "(u@a{sv})", &response, &result);

      if(response != 0){
        rejectCapture(capture, response);
        return;
      }

      g_autoptr(GVariant) streams = g_variant_lookup_value(result, "streams", G_VARIANT_TYPE_ARRAY);
      if(!streams){
        Core::Logger::error("The screencast portal returned no stream");
        resolveCapture(capture, false);
        return;
      }

      GVariantIter iter;
      g_variant_iter_init(&iter, streams);

      capture->streams.clear();
      uint32_t node = 0;
      GVariant* streamProperties = nullptr;
      while(g_variant_iter_loop(&iter, "(u@a{sv})", &node, &streamProperties)){
        PortalStream stream{.node = node, .geometry = std::nullopt};

        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;
        if(g_variant_lookup(streamProperties, "position", "(ii)", &x, &y) && g_variant_lookup(streamProperties, "size", "(ii)", &width, &height)){
          stream.geometry = MonitorGeometry{x, y, width, height};
        }

        capture->streams.push_back(stream);
      }

      if(capture->streams.empty()){
        Core::Logger::error("The screencast portal returned no stream");
        resolveCapture(capture, false);
        return;
      }

      const char* token = nullptr;
      if(g_variant_lookup(result, "restore_token", "&s", &token)){
        capture->restoreToken = token;
      }

      openPipewireRemote(capture);
    }


    void XdgDesktopPortal::onStartedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    )
    {
      Capture* capture = static_cast<Capture*>(userData);

      g_autoptr(GError) error = NULL;
      g_autoptr(GVariant) result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
      (void)result;
      if(error){
        // Cancelled: the capture is being torn down or aborted
        if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)){
          Core::Logger::error("error starting the screencast: ", std::string(error->message));

          // No Response signal will follow a failed call
          unsubscribeResponse(capture);
          resolveCapture(capture, false);
        }
        return;
      }
    }


    void XdgDesktopPortal::start(
      Capture* capture
    )
    {
      try{
        StringPair pathAndToken = portalCreatePath(CreatePathTokenType::Request);
        subscribeToResponse(capture, pathAndToken.first, onStartResponseReceivedCallback);

        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(pathAndToken.second.c_str()));

        g_dbus_proxy_call(getScreencastPortalProxy(), "Start", g_variant_new("(osa{sv})", capture->sessionHandle, "", &builder), G_DBUS_CALL_FLAGS_NONE, -1, capture->cancellable, onStartedCallback, capture);
      }
      catch(const std::exception& e){
        Core::Logger::error(e.what());
        resolveCapture(capture, false);
      }
    }


    void XdgDesktopPortal::onSelectSourceResponseReceivedCallback(
      GDBusConnection* /*connection*/,
      const char* /*senderName*/,
      const char* /*objectPath*/,
      const char* /*interfaceName*/,
      const char* /*signalName*/,
      GVariant* parameters,
      void* userData
    )
    {
      Capture* capture = static_cast<Capture*>(userData);
      unsubscribeResponse(capture);

      uint32_t response;
      g_autoptr(GVariant) ret = NULL;
      g_variant_get(parameters, "(u@a{sv})", &response, &ret);

      if(response != 0){
        rejectCapture(capture, response);
        return;
      }

      start(capture);
    }


    void XdgDesktopPortal::onSourceSelectedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    )
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GVariant) result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
      (void)result;
      if(error){
        // Cancelled: the capture is being torn down or aborted
        if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)){
          Core::Logger::error("error selecting screencast source: ", std::string(error->message));

          // No Response signal will follow a failed call
          Capture* capture = static_cast<Capture*>(userData);
          unsubscribeResponse(capture);
          resolveCapture(capture, false);
        }

        return;
      }
    }


    void XdgDesktopPortal::selectSource(
      Capture* capture
    )
    {
      StringPair pathAndToken = portalCreatePath(CreatePathTokenType::Request);
      subscribeToResponse(capture, pathAndToken.first, onSelectSourceResponseReceivedCallback);

      GVariantBuilder builder;
      g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add(&builder, "{sv}", "types", g_variant_new_uint32(capture->captureType));
      // Lamps may follow different monitors: every ticked monitor becomes a stream
      g_variant_builder_add(&builder, "{sv}", "multiple", g_variant_new_boolean(TRUE));
      g_variant_builder_add(&builder, "{sv}", "handle_token", g_variant_new_string(pathAndToken.second.c_str()));

      if(capture->askForMonitors){
        Core::Logger::log("Asking for the monitors to capture");
      }
      else if(capture->config->restoreToken().has_value()){
        Core::Logger::log("Using saved restore token for screen capture session");
        g_variant_builder_add(
          &builder,
          "{sv}",
          "restore_token",
          g_variant_new_string(capture->config->restoreToken()->c_str())
        );
      }
      else{
        Core::Logger::log("No restore token available, user will be prompted for screen selection");
      }

      uint32_t available_cursor_modes = getAvailableCursorModes();
      if(available_cursor_modes & CursorMode::Metadata){
        g_variant_builder_add(&builder, "{sv}", "cursor_mode", g_variant_new_uint32(CursorMode::Metadata));
      }
      else if((available_cursor_modes & CursorMode::Embedded) && capture->cursorVisible){
        g_variant_builder_add(&builder, "{sv}", "cursor_mode", g_variant_new_uint32(CursorMode::Embedded));
      }
      else{
        g_variant_builder_add(&builder, "{sv}", "cursor_mode", g_variant_new_uint32(CursorMode::Hidden));
      }

      if(getScreencastVersion() >= 4){
        g_variant_builder_add(&builder, "{sv}", "persist_mode", g_variant_new_uint32(2));
      }

      g_dbus_proxy_call(getScreencastPortalProxy(), "SelectSources", g_variant_new("(oa{sv})", capture->sessionHandle, &builder), G_DBUS_CALL_FLAGS_NONE, -1, capture->cancellable, onSourceSelectedCallback, capture);
    }


    void XdgDesktopPortal::onCreateSessionResponseReceivedCallback(
      GDBusConnection* /*connection*/,
      const char* /*senderName*/,
      const char* /*objectPath*/,
      const char* /*interfaceName*/,
      const char* /*signalName*/,
      GVariant* parameters,
      void* userData
    )
    {
      Capture* capture = static_cast<Capture*>(userData);
      unsubscribeResponse(capture);

      uint32_t response;
      g_autoptr(GVariant) result = NULL;
      g_variant_get(parameters, "(u@a{sv})", &response, &result);

      if(response != 0){
        rejectCapture(capture, response);
        return;
      }

      g_autoptr(GVariant) sessionHandleVariant = g_variant_lookup_value(result, "session_handle", NULL);
      capture->sessionHandle = g_variant_dup_string(sessionHandleVariant, NULL);

      selectSource(capture);
    }


    void XdgDesktopPortal::onSessionCreatedCallback(
      GObject* source,
      GAsyncResult* res,
      void* userData
    )
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GVariant) result = g_dbus_proxy_call_finish(G_DBUS_PROXY(source), res, &error);
      (void)result;

      if(error){
        // Cancelled: the capture is being torn down or aborted
        if(!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)){
          Core::Logger::error("error creating screencast session: ", std::string(error->message));

          // No Response signal will follow a failed call
          Capture* capture = static_cast<Capture*>(userData);
          unsubscribeResponse(capture);
          resolveCapture(capture, false);
        }

        return;
      }
    }


}
