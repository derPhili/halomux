# Adapter
target_sources(huenicorn PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Adapters/GnuLinux/GnuLinuxAdapter.cpp
)

# Pipewire grabber (Wayland screencast through the XDG desktop portal)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
find_package(Gio REQUIRED)
pkg_search_module(LIB_PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(LIBGLIB REQUIRED glib-2.0 gio-2.0)

target_sources(huenicorn PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/PipewireGrabber.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.cpp
)

target_include_directories(huenicorn PUBLIC
  ${LIB_PIPEWIRE_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(huenicorn PUBLIC
  GIO::GIO
  ${LIB_PIPEWIRE_LIBRARIES}
  ${LIBGLIB_LIBRARIES}
)
