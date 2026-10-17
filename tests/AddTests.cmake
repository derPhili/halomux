message("Building tests")

find_package(OpenCV REQUIRED COMPONENTS imgproc imgcodecs)

enable_testing()


# TestImageProcessing
add_executable(TestImageProcessing
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/ImageProcessing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestImageProcessing.cpp
)

set_target_properties(TestImageProcessing PROPERTIES CXX_STANDARD 20)

target_include_directories(TestImageProcessing PUBLIC
  include
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(TestImageProcessing PUBLIC
  ${OpenCV_LIBS}
  glm::glm
)

# Without arguments, only the self-checking tests run
add_test(NAME ImageProcessing COMMAND TestImageProcessing)


# TestGrabber (manual: needs a graphical session)
# Reuses every application source but the entry point
get_target_property(APP_SOURCES halomux SOURCES)
list(FILTER APP_SOURCES EXCLUDE REGEX "/src/App/")

add_executable(TestGrabber
  ${APP_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestGrabber.cpp
)

add_dependencies(TestGrabber generate_version_header generate_webroot_header)

set_target_properties(TestGrabber PROPERTIES CXX_STANDARD 20)

get_target_property(APP_INCLUDE_DIRECTORIES halomux INCLUDE_DIRECTORIES)
get_target_property(APP_COMPILE_DEFINITIONS halomux COMPILE_DEFINITIONS)
get_target_property(APP_LINK_LIBRARIES halomux LINK_LIBRARIES)

target_include_directories(TestGrabber PUBLIC ${APP_INCLUDE_DIRECTORIES})
if(APP_COMPILE_DEFINITIONS)
  target_compile_definitions(TestGrabber PUBLIC ${APP_COMPILE_DEFINITIONS})
endif()
target_link_libraries(TestGrabber PUBLIC ${APP_LINK_LIBRARIES} ${OpenCV_LIBS})


# TestGpuDownscale: GPU downscale of a real DMA-BUF against the exact CPU area average
pkg_check_modules(GBM REQUIRED gbm)

add_executable(TestGpuDownscale
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/Pipewire/EglDownscaler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/ImageProcessing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestGpuDownscale.cpp
)

set_target_properties(TestGpuDownscale PROPERTIES CXX_STANDARD 20)

target_include_directories(TestGpuDownscale PUBLIC
  include
  ${OpenCV_INCLUDE_DIRS}
  ${GPU_INCLUDE_DIRS}
  ${GBM_INCLUDE_DIRS}
)

target_link_libraries(TestGpuDownscale PUBLIC
  glm::glm
  ${OpenCV_LIBS}
  ${GPU_LIBRARIES}
  ${GBM_LIBRARIES}
)

add_test(NAME GpuDownscale COMMAND TestGpuDownscale ${CMAKE_CURRENT_SOURCE_DIR}/screenshots/WebUI.png)


# TestHighlightBoost: brightness flash envelope of a channel
add_executable(TestHighlightBoost
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/Channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestHighlightBoost.cpp
)

set_target_properties(TestHighlightBoost PROPERTIES CXX_STANDARD 20)

target_include_directories(TestHighlightBoost PUBLIC
  include
)

target_link_libraries(TestHighlightBoost PUBLIC
  nlohmann_json::nlohmann_json
  glm::glm
)

add_test(NAME HighlightBoost COMMAND TestHighlightBoost)


# TestTransientDetector: sound transient detection on synthetic energies
add_executable(TestTransientDetector
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestTransientDetector.cpp
)

set_target_properties(TestTransientDetector PROPERTIES CXX_STANDARD 20)

target_include_directories(TestTransientDetector PUBLIC
  include
)

add_test(NAME TransientDetector COMMAND TestTransientDetector)


# TestPreviewSequence: synthetic content of the settings preview
add_executable(TestPreviewSequence
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestPreviewSequence.cpp
)

set_target_properties(TestPreviewSequence PROPERTIES CXX_STANDARD 20)

target_include_directories(TestPreviewSequence PUBLIC
  include
)

target_link_libraries(TestPreviewSequence PUBLIC
  glm::glm
)

add_test(NAME PreviewSequence COMMAND TestPreviewSequence)


# TestStaticColors: color notation and serialization of the static scenes
add_executable(TestStaticColors
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestStaticColors.cpp
)

set_target_properties(TestStaticColors PROPERTIES CXX_STANDARD 20)

target_include_directories(TestStaticColors PUBLIC
  include
)

target_link_libraries(TestStaticColors PUBLIC
  nlohmann_json::nlohmann_json
  glm::glm
)

add_test(NAME StaticColors COMMAND TestStaticColors)


# TestStreamer: streaming never waits for the connection to the bridge
add_executable(TestStreamer
  # Application
  ${STREAM_SOURCE}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Auth/Credentials.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestStreamer.cpp
)

set_target_properties(TestStreamer PROPERTIES CXX_STANDARD 20)

target_include_directories(TestStreamer PUBLIC
  include
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_dependencies(TestStreamer generate_version_header)

target_link_libraries(TestStreamer PUBLIC
  ${MBEDTLS_LIBS}
  ${MBEDTLS_X509}
  ${MBEDTLS_CRYPTO}
  nlohmann_json::nlohmann_json
  glm::glm
)

add_test(NAME Streamer COMMAND TestStreamer)


# TestBridgeDiscovery: reading mDNS answers, which come from the network
add_executable(TestBridgeDiscovery
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/BridgeDiscovery.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestBridgeDiscovery.cpp
)

set_target_properties(TestBridgeDiscovery PROPERTIES CXX_STANDARD 20)

target_include_directories(TestBridgeDiscovery PUBLIC
  include
)

target_compile_options(TestBridgeDiscovery PRIVATE -fsanitize=address,undefined)
target_link_options(TestBridgeDiscovery PRIVATE -fsanitize=address,undefined)

add_test(NAME BridgeDiscovery COMMAND TestBridgeDiscovery)


# TestMonitors: matching monitors between the portal, the compositor and the profile
add_executable(TestMonitors
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/Monitors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestMonitors.cpp
)

set_target_properties(TestMonitors PROPERTIES CXX_STANDARD 20)

target_include_directories(TestMonitors PUBLIC
  include
)

target_link_libraries(TestMonitors PUBLIC
  glm::glm
)

target_compile_options(TestMonitors PRIVATE -fsanitize=address,undefined)
target_link_options(TestMonitors PRIVATE -fsanitize=address,undefined)

add_test(NAME Monitors COMMAND TestMonitors)


# TestWaylandOutputs (manual: needs a Wayland session): prints the compositor's monitors
add_executable(TestWaylandOutputs
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/GnuLinux/WaylandOutputs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestWaylandOutputs.cpp
)

set_target_properties(TestWaylandOutputs PROPERTIES CXX_STANDARD 20)

target_include_directories(TestWaylandOutputs PUBLIC
  include
)

target_link_libraries(TestWaylandOutputs PUBLIC
  wayland_protocols
)


# TestChannelMonitors: the monitor each lamp follows, in the profile
add_executable(TestChannelMonitors
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/Monitors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Api/Channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestChannelMonitors.cpp
)

set_target_properties(TestChannelMonitors PROPERTIES CXX_STANDARD 20)

target_include_directories(TestChannelMonitors PUBLIC
  include
)

target_link_libraries(TestChannelMonitors PUBLIC
  nlohmann_json::nlohmann_json
  glm::glm
)

target_compile_options(TestChannelMonitors PRIVATE -fsanitize=address,undefined)
target_link_options(TestChannelMonitors PRIVATE -fsanitize=address,undefined)

add_test(NAME ChannelMonitors COMMAND TestChannelMonitors)


# TestGrabberCancel: a failed screen selection leaves the running capture alone
add_executable(TestGrabberCancel
  ${APP_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestGrabberCancel.cpp
)

add_dependencies(TestGrabberCancel generate_version_header generate_webroot_header)

set_target_properties(TestGrabberCancel PROPERTIES CXX_STANDARD 20)

target_include_directories(TestGrabberCancel PUBLIC ${APP_INCLUDE_DIRECTORIES})
if(APP_COMPILE_DEFINITIONS)
  target_compile_definitions(TestGrabberCancel PUBLIC ${APP_COMPILE_DEFINITIONS})
endif()
target_link_libraries(TestGrabberCancel PUBLIC ${APP_LINK_LIBRARIES} ${OpenCV_LIBS})

add_test(NAME GrabberCancel COMMAND TestGrabberCancel)


# TestHostCheck: which Host headers the light manager answers, against DNS rebinding
add_executable(TestHostCheck
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Network/Http/Server/HostCheck.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestHostCheck.cpp
)

set_target_properties(TestHostCheck PROPERTIES CXX_STANDARD 20)

target_include_directories(TestHostCheck PUBLIC
  include
)

target_compile_options(TestHostCheck PRIVATE -fsanitize=address,undefined)
target_link_options(TestHostCheck PRIVATE -fsanitize=address,undefined)

add_test(NAME HostCheck COMMAND TestHostCheck)


# TestConfigFile: how the configuration, which holds the bridge credentials, is stored
add_executable(TestConfigFile
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Hue/Auth/Credentials.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestConfigFile.cpp
)

set_target_properties(TestConfigFile PROPERTIES CXX_STANDARD 20)

target_include_directories(TestConfigFile PUBLIC
  include
)

target_link_libraries(TestConfigFile PUBLIC
  nlohmann_json::nlohmann_json
  glm::glm
)

add_test(NAME ConfigFile COMMAND TestConfigFile)


# TestProfile: lamp settings kept per entertainment area
add_executable(TestProfile
  # Application
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Profile.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp

  # Test
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestProfile.cpp
)

set_target_properties(TestProfile PROPERTIES CXX_STANDARD 20)

target_include_directories(TestProfile PUBLIC
  include
)

target_link_libraries(TestProfile PUBLIC
  nlohmann_json::nlohmann_json
)

target_compile_options(TestProfile PRIVATE -fsanitize=address,undefined)
target_link_options(TestProfile PRIVATE -fsanitize=address,undefined)

add_test(NAME Profile COMMAND TestProfile)
