cmake_minimum_required(VERSION 3.10.0)

project(HueDesktopSync VERSION 1.2.0)

set(BUILD_TESTS FALSE CACHE BOOL "Build tests")


# Check mandatory libraries
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS imgproc)
find_package(CURL REQUIRED)
find_library(MBEDTLS_LIBS mbedtls REQUIRED)
find_library(MBEDTLS_X509 mbedx509 REQUIRED)
find_library(MBEDTLS_CRYPTO mbedcrypto REQUIRED)


# Header-only libraries (installed system-wide)
find_package(nlohmann_json REQUIRED)
find_package(httplib REQUIRED)
find_package(glm REQUIRED)


# Generate version header
set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/Huenicorn/Version.hpp")

add_custom_target(generate_version_header DEPENDS ${VERSION_HEADER})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/Version.hpp.in
  ${VERSION_HEADER}
)

# Generate embedded webroot header
function(embed_directory TARGET_DIR)
  file(GLOB_RECURSE WEB_FILES CONFIGURE_DEPENDS "${TARGET_DIR}/*")
  set(CONCAT_CONTENT "")

  # The embedded header is generated at configure time: re-run it whenever an embedded file changes
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${WEB_FILES})

  foreach(DATA_FILE ${WEB_FILES})
    get_filename_component(KEY ${DATA_FILE} NAME)
    file(READ ${DATA_FILE} CONTENTS)

    # Escape special characters
    string(REPLACE "\\" "\\\\" CONTENTS "${CONTENTS}")
    string(REPLACE "\"" "\\\"" CONTENTS "${CONTENTS}")
    string(REPLACE "\n" "\\n" CONTENTS "${CONTENTS}")

    set(KEY_VALUE "      {\"${KEY}\", \"${CONTENTS}\"},\n")
    set(CONCAT_CONTENT "${CONCAT_CONTENT}${KEY_VALUE}")
  endforeach()
  set(EMBEDDED_FILES_CONTENT "${EMBEDDED_FILES_CONTENT}${CONCAT_CONTENT}" PARENT_SCOPE)
endfunction()

set(EMBEDDED_FILES_CONTENT "")
embed_directory("${CMAKE_SOURCE_DIR}/webroot")
embed_directory("${CMAKE_SOURCE_DIR}/assets")


set(EMBEDDED_WEBROOT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/Huenicorn/EmbeddedWebrootFiles.hpp")


# Custom command to ensure re-running when files change
add_custom_command(
  OUTPUT ${EMBEDDED_WEBROOT_HEADER}
  COMMAND ${CMAKE_COMMAND} -E echo "Regenerating embedded webroot header..."
  COMMAND ${CMAKE_COMMAND} -E touch ${EMBEDDED_WEBROOT_HEADER}  # Ensures file gets updated
  DEPENDS ${WEBROOT_FILES} ${ASSETS_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/config/EmbeddedWebrootFiles.hpp.in
  COMMENT "Updating EmbeddedWebrootFiles.hpp because webroot or assets changed."
)


add_custom_target(generate_webroot_header DEPENDS ${EMBEDDED_WEBROOT_HEADER})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/EmbeddedWebrootFiles.hpp.in
  ${EMBEDDED_WEBROOT_HEADER}
)

set(APP_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/App/CommandLine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/App/main.cpp
)

set(AUDIO_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/AudioAnalyzer.cpp
)

set(CORE_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Config.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/CoreService.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Runtime.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp
)


set(HUE_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Auth/Credentials.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/ApiTools.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/BridgeAddress.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/Channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/EntertainmentConfigurationSelector.cpp
)

set(IMAGING_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/ImageProcessing.cpp
)

set(NETWORK_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Http/Client/CurlClient.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Http/Server/SetupBackend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Http/Server/WebUIBackend.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Http/Server/HttpServer.cpp
)

set(PLATFORM_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Selector.cpp
)

set(STREAM_SOURCE
  # Dtls Impl
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Stream/Impl/MbedTls/DtlsClient.cpp

  # Common
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Stream/HuestreamHeader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Stream/Streamer.cpp
)

set(TIMING_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Timing/LoopRegulator.cpp
)


# Declare executable
add_executable(hue-desktop-sync
  ${APP_SOURCE}
  ${AUDIO_SOURCE}
  ${CORE_SOURCE}
  ${HUE_SOURCE}
  ${IMAGING_SOURCE}
  ${NETWORK_SOURCE}
  ${PLATFORM_SOURCE}
  ${STREAM_SOURCE}
  ${TIMING_SOURCE}

  ${VERSION_HEADER}
  ${EMBEDDED_WEBROOT_HEADER}
)

add_dependencies(hue-desktop-sync generate_version_header generate_webroot_header)

# Adapter
target_sources(hue-desktop-sync PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Platform/Adapters/GnuLinux/GnuLinuxAdapter.cpp
)

# Pipewire grabber (Wayland screencast through the XDG desktop portal)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
find_package(Gio REQUIRED)
pkg_search_module(LIB_PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(LIBGLIB REQUIRED glib-2.0 gio-2.0)

# GPU downscaling of DMA-BUF frames
pkg_check_modules(GPU REQUIRED egl glesv2 libdrm)

target_sources(hue-desktop-sync PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/PipewireGrabber.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/XdgDesktopPortal.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/EglDownscaler.cpp
)

target_include_directories(hue-desktop-sync PUBLIC
  ${LIB_PIPEWIRE_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${GPU_INCLUDE_DIRS}
)

target_link_libraries(hue-desktop-sync PUBLIC
  GIO::GIO
  ${LIB_PIPEWIRE_LIBRARIES}
  ${LIBGLIB_LIBRARIES}
  ${GPU_LIBRARIES}
)

set_target_properties(hue-desktop-sync PROPERTIES CXX_STANDARD 20)

target_include_directories(hue-desktop-sync PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR} # For adapters and grabbers
  ${OpenCV_INCLUDE_DIRS}
  include
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(hue-desktop-sync PUBLIC
  ${OpenCV_LIBS}
  ${CURL_LIBRARIES}
  ${MBEDTLS_LIBS}
  ${MBEDTLS_X509}
  ${MBEDTLS_CRYPTO}
  nlohmann_json::nlohmann_json
  httplib::httplib
  glm::glm
)


target_compile_options(hue-desktop-sync PRIVATE
  -Werror
  -Wall
  -Wextra
  -Wnon-virtual-dtor
  -pedantic
  -Wno-deprecated-declarations
  -Wno-deprecated-copy
)

if(${BUILD_TESTS})
  include(tests/AddTests.cmake)
endif(${BUILD_TESTS})
