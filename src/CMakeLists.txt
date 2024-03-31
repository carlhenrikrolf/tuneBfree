cmake_minimum_required(VERSION 3.15)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.11 CACHE STRING "Build for 10.1")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(tuneBfree)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)


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

target_compile_definitions(${PROJECT_NAME} PRIVATE -DCLAP)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
  ../libs/clap/include
  ../libs/MTS-ESP/Client
  ../libs/readerwriterqueue
)
