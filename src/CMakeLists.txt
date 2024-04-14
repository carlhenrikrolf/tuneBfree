cmake_minimum_required(VERSION 3.9.6...3.15.0)

IF(WIN32)
   set(CMAKE_TOOLCHAIN_FILE ../libs/vcpkg/scripts/buildsystems/vcpkg.cmake)
   set(VCPKG_TARGET_TRIPLET x64-windows-static)
ENDIF()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Build for 10.15")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

option(CLAP_GUI "Build plugin with GUI" FALSE)
option(DEBUG_PRINT "Print debugging logs" FALSE)

project(tuneBfree LANGUAGES C CXX)

if(NOT CLAP_GUI)

add_library(
  ${PROJECT_NAME}
  MODULE
  clap.cpp
  tonegen.cpp
  tuning.cpp
  vibrato.cpp
  whirl.cpp
  eqcomp.cpp
  overdrive.cpp
  reverb.cpp
  ../libs/MTS-ESP/Client/libMTSClient.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".clap" PREFIX "")

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCLAP $<$<BOOL:${DEBUG_PRINT}>:-DDEBUG_PRINT>)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  ../libs/clap/include
  ../libs/MTS-ESP/Client
  ../libs/readerwriterqueue
)

else()

set(ELEMENTS_ROOT ../libs/elements)

# Make sure ELEMENTS_ROOT is an absolute path to add to the CMake module path
get_filename_component(ELEMENTS_ROOT "${ELEMENTS_ROOT}" ABSOLUTE)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${ELEMENTS_ROOT}/cmake")

# If we are building outside the project, you need to set ELEMENTS_ROOT:
if (NOT ELEMENTS_BUILD_EXAMPLES)
   include(ElementsConfigCommon)
   set(ELEMENTS_BUILD_EXAMPLES OFF)
   add_subdirectory(${ELEMENTS_ROOT} elements)
endif()

set(ELEMENTS_APP_PROJECT "tuneBfree")
set(ELEMENTS_APP_TITLE "Hello CLAP")
set(ELEMENTS_APP_COPYRIGHT "Copyright (c) 2016-2023 Joel de Guzman")
set(ELEMENTS_APP_ID "com.cycfi.hello-universe")
set(ELEMENTS_APP_VERSION "1.0")

if(UNIX AND NOT APPLE)
  set(ELEMENTS_HOST_UI_LIBRARY "X11")
endif()

set(ELEMENTS_APP_SOURCES
  clap.cpp
  tonegen.cpp
  tuning.cpp
  vibrato.cpp
  whirl.cpp
  eqcomp.cpp
  overdrive.cpp
  reverb.cpp
  ../libs/MTS-ESP/Client/libMTSClient.cpp
)
# set(ELEMENTS_APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
# set(ELEMENTS_APP_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/space.jpg)

# For your custom application icon on macOS or Windows see cmake/AppIcon.cmake module
include(AppIcon)
include(ElementsConfigApp)

target_include_directories(
  ${ELEMENTS_APP_PROJECT}
  PUBLIC
  ../libs/clap/include
  ../libs/MTS-ESP/Client
  ../libs/readerwriterqueue
)

if(UNIX AND NOT APPLE)
  target_link_libraries(${ELEMENTS_APP_PROJECT} PUBLIC X11)
endif()

set_target_properties(${ELEMENTS_APP_PROJECT} PROPERTIES SUFFIX ".clap" PREFIX "")

target_compile_definitions(${ELEMENTS_APP_PROJECT} PRIVATE -DCLAP -DCLAP_GUI $<$<BOOL:${DEBUG_PRINT}>:-DDEBUG_PRINT>)

endif()
