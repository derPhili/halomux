cmake_minimum_required(VERSION 3.10.0)

project(Huenicorn VERSION 1.2.0)

set(BUILD_DOCUMENTATION FALSE CACHE BOOL "Build documentation")
set(BUILD_TESTS FALSE CACHE BOOL "Build tests")

option(HUENICORN_FETCH_DEPS "Download dependencies if missing" ON)

include(FetchContent)


# Check mandatory libraries
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS imgproc)
find_package(CURL REQUIRED)
find_library(MBEDTLS_LIBS mbedtls REQUIRED)
find_library(MBEDTLS_X509 mbedx509 REQUIRED)
find_library(MBEDTLS_CRYPTO mbedcrypto REQUIRED)


# NLohmann JSON
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  if(HUENICORN_FETCH_DEPS)
    message(STATUS "nlohmann_json is not installed. Now fetching online...")
    FetchContent_Declare(
      nlohmann_json
      URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
  else()
    message(FATAL_ERROR "nlohmann_json is not installed and online fetch is disabled")
  endif()
else()
  message(STATUS "nlohmann_json is installed.")
endif()


# CppHttpLip
find_package(httplib QUIET)
if(NOT httplib_FOUND)
  if(HUENICORN_FETCH_DEPS)
    message(STATUS "httplib is not installed. Now fetching online...")
    FetchContent_Declare(
      cpp_httplib
      URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.46.0.tar.gz
    )
    FetchContent_MakeAvailable(cpp_httplib)
  else()
    message(FATAL_ERROR "httplib is not installed and online fetch is disabled")
  endif()
else()
  message(STATUS "httplib is installed.")
endif()


# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
  if(HUENICORN_FETCH_DEPS)
    message(STATUS "glm is not installed. Now fetching online...")
    FetchContent_Declare(
      glm
      URL https://github.com/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
    )
    FetchContent_MakeAvailable(glm)
  else()
    message(FATAL_ERROR "glm is not installed and online fetch is disabled")
  endif()
else()
  message(STATUS "glm is installed")
endif()


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

set(CORE_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Config.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/CoreService.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Runtime.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Logger.cpp
)

set(GRABBER_SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Grabber/DummyGrabber.cpp
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
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/Interpolation.cpp
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
add_executable(huenicorn
  ${APP_SOURCE}
  ${CORE_SOURCE}
  ${GRABBER_SOURCE}
  ${HUE_SOURCE}
  ${IMAGING_SOURCE}
  ${NETWORK_SOURCE}
  ${PLATFORM_SOURCE}
  ${STREAM_SOURCE}
  ${TIMING_SOURCE}

  ${VERSION_HEADER}
  ${EMBEDDED_WEBROOT_HEADER}
)

add_dependencies(huenicorn generate_version_header generate_webroot_header)

if(UNIX AND NOT APPLE) # For Gnu/Linux
  include(cmake/platforms/GnuLinux.cmake)
endif()

if(APPLE) # For MacOS
  include(cmake/platforms/MacOS.cmake)
endif()

if(WIN32) #For Windows
  include(cmake/platforms/Windows.cmake)
endif()

set_target_properties(huenicorn PROPERTIES CXX_STANDARD 20)

target_include_directories(huenicorn PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR} # For adapters and grabbers
  ${OpenCV_INCLUDE_DIRS}
  include
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(huenicorn PUBLIC
  ${OpenCV_LIBS}
  ${CURL_LIBRARIES}
  ${MBEDTLS_LIBS}
  ${MBEDTLS_X509}
  ${MBEDTLS_CRYPTO}
  nlohmann_json::nlohmann_json
  httplib::httplib
  glm::glm
)


target_compile_options(huenicorn PRIVATE
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


if(${BUILD_DOCUMENTATION})
  # Initializing Doxygen
  find_package(Doxygen REQUIRED dot OPTIONAL_COMPONENTS mscgen dia)

  # Defining Doxyfile path
  set(doxyfile_in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.doxygen)

  # Setting Doxyfile properties
  set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc)
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_FULL_PATH_NAMES NO)

  # Generating doxyfile
  doxygen_add_docs(
    doxygen
    ${PROJECT_SOURCE_DIR}
    COMMENT "Generate man pages"
  )

  # Building Doxygen documentation
  add_custom_target(documentationBuild ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile_in}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM
  )
endif(${BUILD_DOCUMENTATION})


# Post build commands

# Copy readme to build
add_custom_command(TARGET huenicorn POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md $<TARGET_FILE_DIR:huenicorn>/README.md
)

# Copy screenshots to build
add_custom_command(TARGET huenicorn POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/screenshots $<TARGET_FILE_DIR:huenicorn>/screenshots
)

# Copy assets to build
add_custom_command(TARGET huenicorn POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:huenicorn>/assets
)

# Copy license to build
add_custom_command(TARGET huenicorn POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
  ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt $<TARGET_FILE_DIR:huenicorn>/LICENSE.txt
)
