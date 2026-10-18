message("Building tests")

find_package(OpenCV REQUIRED COMPONENTS imgproc imgcodecs)
pkg_check_modules(GBM REQUIRED gbm)

enable_testing()

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# The application's sources but the entry point, with what they need to build
get_target_property(APP_SOURCES halomux SOURCES)
list(FILTER APP_SOURCES EXCLUDE REGEX "/src/App/")
get_target_property(APP_INCLUDE_DIRECTORIES halomux INCLUDE_DIRECTORIES)
get_target_property(APP_COMPILE_DEFINITIONS halomux COMPILE_DEFINITIONS)
get_target_property(APP_LINK_LIBRARIES halomux LINK_LIBRARIES)


# halomux_add_test(<Name>
#   [SOURCES <application sources>]   tests/src/Test<Name>.cpp and the logger are added
#   [WITH_APP]                        builds against all the application's sources instead
#   [LIBRARIES <libraries>]
#   [INCLUDES <include directories>]  besides include/
#   [DEPENDS <targets>]
#   [SANITIZE <sanitizers>]           address,undefined unless given, "none" for none
#   [ARGS <arguments>]
#   [TIMEOUT <seconds>]
#   [ENVIRONMENT <variables>]
#   [MANUAL])                         built, but not run by ctest
#
# Builds Test<Name> and registers it as the ctest test <Name>. Tests find their data files under
# HALOMUX_TEST_FIXTURES.
function(halomux_add_test name)
  cmake_parse_arguments(PARSE_ARGV 1 TEST "WITH_APP;MANUAL" "SANITIZE;TIMEOUT" "SOURCES;LIBRARIES;INCLUDES;DEPENDS;ARGS;ENVIRONMENT")

  set(target Test${name})

  if(TEST_WITH_APP)
    add_executable(${target} ${APP_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/${target}.cpp)
    add_dependencies(${target} generate_version_header generate_webroot_header)
    target_include_directories(${target} PUBLIC ${APP_INCLUDE_DIRECTORIES})
    if(APP_COMPILE_DEFINITIONS)
      target_compile_definitions(${target} PUBLIC ${APP_COMPILE_DEFINITIONS})
    endif()
    target_link_libraries(${target} PUBLIC ${APP_LINK_LIBRARIES})
  else()
    add_executable(${target} ${TEST_SOURCES} ${SRC}/Core/Logger.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/${target}.cpp)
    target_include_directories(${target} PUBLIC include)
  endif()

  set_target_properties(${target} PROPERTIES CXX_STANDARD 20)
  target_include_directories(${target} PUBLIC ${TEST_INCLUDES})
  target_link_libraries(${target} PUBLIC ${TEST_LIBRARIES})
  target_compile_definitions(${target} PRIVATE HALOMUX_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

  if(TEST_DEPENDS)
    add_dependencies(${target} ${TEST_DEPENDS})
  endif()

  if(NOT DEFINED TEST_SANITIZE)
    set(TEST_SANITIZE address,undefined)
  endif()
  if(NOT TEST_SANITIZE STREQUAL "none")
    target_compile_options(${target} PRIVATE -fsanitize=${TEST_SANITIZE})
    target_link_options(${target} PRIVATE -fsanitize=${TEST_SANITIZE})
  endif()

  if(TEST_MANUAL)
    return()
  endif()

  add_test(NAME ${name} COMMAND ${target} ${TEST_ARGS})
  if(TEST_TIMEOUT)
    set_tests_properties(${name} PROPERTIES TIMEOUT ${TEST_TIMEOUT})
  endif()
  if(TEST_ENVIRONMENT)
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
  endif()
endfunction()


# Imaging and capture

# Area average of the CPU path; without arguments, only the self-checking tests run
halomux_add_test(ImageProcessing
  SOURCES ${SRC}/Imaging/ImageProcessing.cpp
  INCLUDES ${OpenCV_INCLUDE_DIRS}
  LIBRARIES ${OpenCV_LIBS} glm::glm
  SANITIZE none
)

# Captures the screen (needs a graphical session)
halomux_add_test(Grabber WITH_APP
  LIBRARIES ${OpenCV_LIBS}
  SANITIZE none
  MANUAL
)

# A failed screen selection leaves the running capture alone
halomux_add_test(GrabberCancel WITH_APP
  LIBRARIES ${OpenCV_LIBS}
)

# GPU downscale of a real DMA-BUF against the exact CPU area average
halomux_add_test(GpuDownscale
  SOURCES ${SRC}/Grabber/GnuLinux/Pipewire/EglDownscaler.cpp ${SRC}/Imaging/ImageProcessing.cpp
  INCLUDES ${OpenCV_INCLUDE_DIRS} ${GPU_INCLUDE_DIRS} ${GBM_INCLUDE_DIRS}
  LIBRARIES glm::glm ${OpenCV_LIBS} ${GPU_LIBRARIES} ${GBM_LIBRARIES}
  SANITIZE none
  ARGS ${CMAKE_CURRENT_SOURCE_DIR}/screenshots/WebUI.png
)

# Matching monitors between the portal, the compositor and the profile
halomux_add_test(Monitors
  SOURCES ${SRC}/Grabber/Monitors.cpp
  LIBRARIES glm::glm
)

# Resolutions a captured image can be downscaled to
halomux_add_test(SubsampleResolutions
  SOURCES ${SRC}/Grabber/SubsampleResolutions.cpp
  LIBRARIES glm::glm
)

# Prints the compositor's monitors (needs a Wayland session)
halomux_add_test(WaylandOutputs
  SOURCES ${SRC}/Grabber/GnuLinux/WaylandOutputs.cpp
  LIBRARIES wayland_protocols
  SANITIZE none
  MANUAL
)


# Lamps and their colors

# Brightness flash envelope of a channel
halomux_add_test(HighlightBoost
  SOURCES ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# The color streamed for a lamp from the color it should show
halomux_add_test(ChannelColor
  SOURCES ${SRC}/Core/ChannelColor.cpp ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# The monitor each lamp follows, in the profile
halomux_add_test(ChannelMonitors
  SOURCES ${SRC}/Grabber/Monitors.cpp ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# The lamps' settings as the light manager edits them
halomux_add_test(ChannelStore
  SOURCES ${SRC}/Core/ChannelStore.cpp ${SRC}/Grabber/Monitors.cpp ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# Lamp settings kept per entertainment area
halomux_add_test(Profile
  SOURCES ${SRC}/Core/Profile.cpp ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# Synthetic content of the settings preview
halomux_add_test(PreviewSequence
  LIBRARIES glm::glm
)

# Color notation and serialization of the static scenes
halomux_add_test(StaticColors
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)


# Sound

# Sound transient detection on synthetic energies
halomux_add_test(TransientDetector)

# Starting and stopping the sound analysis, which quits a Pipewire loop from another thread
halomux_add_test(AudioLifecycle
  SOURCES ${SRC}/Audio/AudioAnalyzer.cpp ${SRC}/Pipewire/LoopThread.cpp
  INCLUDES ${LIB_PIPEWIRE_INCLUDE_DIRS}
  LIBRARIES ${LIB_PIPEWIRE_LIBRARIES}
  TIMEOUT 60
)

# Starting and stopping Pipewire loop threads in every order
halomux_add_test(LoopThread
  SOURCES ${SRC}/Pipewire/LoopThread.cpp
  INCLUDES ${LIB_PIPEWIRE_INCLUDE_DIRS}
  LIBRARIES ${LIB_PIPEWIRE_LIBRARIES}
  TIMEOUT 60
)


# Bridge

# Reading mDNS answers, which come from the network
halomux_add_test(BridgeDiscovery
  SOURCES ${SRC}/Hue/Api/BridgeDiscovery.cpp
)

# Reading the bridge's CLIP v2 answers, which may be empty, errors or malformed
halomux_add_test(ClipParsing
  SOURCES ${SRC}/Hue/Api/ClipParsing.cpp ${SRC}/Hue/Api/Channel.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# Selecting the entertainment area and recovering when the bridge stopped streaming
halomux_add_test(AreaSession
  SOURCES ${SRC}/Core/AreaSession.cpp ${SRC}/Hue/Api/Channel.cpp
  INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/include
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
  DEPENDS generate_version_header
)

# Streaming never waits for the connection to the bridge
halomux_add_test(Streamer
  SOURCES ${STREAM_SOURCE} ${SRC}/Hue/Auth/Credentials.cpp
  INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/include
  LIBRARIES ${MBEDTLS_LIBS} ${MBEDTLS_X509} ${MBEDTLS_CRYPTO} nlohmann_json::nlohmann_json glm::glm
  DEPENDS generate_version_header
)

# Reading the clientkey, the pre-shared key of the streaming connection
halomux_add_test(Credentials
  SOURCES ${SRC}/Hue/Auth/Credentials.cpp
  LIBRARIES nlohmann_json::nlohmann_json
)

# Failed connection attempts release their TLS contexts
halomux_add_test(DtlsClient
  SOURCES ${SRC}/Stream/Impl/MbedTls/DtlsClient.cpp ${SRC}/Hue/Auth/Credentials.cpp
  LIBRARIES ${MBEDTLS_LIBS} ${MBEDTLS_X509} ${MBEDTLS_CRYPTO} nlohmann_json::nlohmann_json
)


# Light manager and configuration

# Which Host headers the light manager answers, against DNS rebinding
halomux_add_test(HostCheck
  SOURCES ${SRC}/Network/Http/Server/HostCheck.cpp
)

# Malformed requests get 400, unknown resources 404, in one answer shape
halomux_add_test(HttpErrors
  SOURCES ${SRC}/Network/Http/Server/HttpServer.cpp ${SRC}/Network/Http/Server/HostCheck.cpp ${SRC}/Network/Http/Server/RouteTools.cpp
  LIBRARIES httplib::httplib nlohmann_json::nlohmann_json glm::glm
)

# Files only ever replaced by a complete content
halomux_add_test(AtomicFile
  SOURCES ${SRC}/Core/AtomicFile.cpp
)

# How the configuration, which holds the bridge credentials, is stored
halomux_add_test(ConfigFile
  SOURCES ${SRC}/Core/Config.cpp ${SRC}/Core/AtomicFile.cpp ${SRC}/Hue/Auth/Credentials.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
)

# Configuration read while the setup writes it, under ThreadSanitizer
halomux_add_test(ConfigThreads
  SOURCES ${SRC}/Core/Config.cpp ${SRC}/Core/AtomicFile.cpp ${SRC}/Hue/Auth/Credentials.cpp
  LIBRARIES nlohmann_json::nlohmann_json glm::glm
  SANITIZE thread
  ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1"
)
