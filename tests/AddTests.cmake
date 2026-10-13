message("Building tests")

find_package(OpenCV REQUIRED COMPONENTS imgproc imgcodecs)

enable_testing()


# TestImageProcessing
add_executable(TestImageProcessing
  #Huenicorn
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/ImageProcessing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Imaging/Interpolation.cpp
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
get_target_property(HUENICORN_SOURCES huenicorn SOURCES)
list(FILTER HUENICORN_SOURCES EXCLUDE REGEX "/src/App/")

add_executable(TestGrabber
  ${HUENICORN_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/src/TestGrabber.cpp
)

add_dependencies(TestGrabber generate_version_header generate_webroot_header)

set_target_properties(TestGrabber PROPERTIES CXX_STANDARD 20)

get_target_property(HUENICORN_INCLUDE_DIRECTORIES huenicorn INCLUDE_DIRECTORIES)
get_target_property(HUENICORN_COMPILE_DEFINITIONS huenicorn COMPILE_DEFINITIONS)
get_target_property(HUENICORN_LINK_LIBRARIES huenicorn LINK_LIBRARIES)

target_include_directories(TestGrabber PUBLIC ${HUENICORN_INCLUDE_DIRECTORIES})
if(HUENICORN_COMPILE_DEFINITIONS)
  target_compile_definitions(TestGrabber PUBLIC ${HUENICORN_COMPILE_DEFINITIONS})
endif()
target_link_libraries(TestGrabber PUBLIC ${HUENICORN_LINK_LIBRARIES} ${OpenCV_LIBS})
