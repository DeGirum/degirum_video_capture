cmake_minimum_required(VERSION 3.16)

#
# DeGirum Video Capture CMake Project
#
# Copyright 2026 DeGirum Corporation
#

# Single source of truth project version declaration
project(DEGIRUM_VIDEO_CAPTURE VERSION 0.1.0 LANGUAGES C CXX)

# ##############################################################################
# Configuration Options
# ##############################################################################
# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Build type not specified, defaulting to Release")
endif()

option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(RUN_TESTS "Run tests automatically after build" OFF)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configuration types to generate build rules for")

# ##############################################################################
# Compiler Settings
# ##############################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "================== Build type: ${CMAKE_BUILD_TYPE} =========================================")

# Platform-specific compiler settings
if(UNIX AND NOT APPLE)
  add_compile_options(-Werror) # Treat warnings as errors
  add_compile_options(-fPIC)
  add_link_options($<$<CONFIG:Debug>:-rdynamic>)
  
  # Enable vectorization
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize -O3")
  endif()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize -O3 -march=armv8-a -mtune=native")
  endif()
elseif(APPLE)
  add_compile_options(-fvisibility=hidden)
  add_compile_options(-fvisibility-inlines-hidden)
  add_compile_options(-fPIC)
  add_compile_options(-Werror)
  add_link_options($<$<CONFIG:Debug>:-rdynamic>)
elseif(MSVC)
  add_compile_definitions(NOMINMAX)
  add_compile_options(/bigobj)
  add_compile_options(/EHsc)
  add_compile_options(/WX)
endif()

# ##############################################################################
# External Dependencies
# ##############################################################################
find_package(Threads REQUIRED)
find_package(Git REQUIRED)

# Python components (if building Python bindings)
if(BUILD_PYTHON_BINDINGS)
  find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
  
  if(NOT Python3_FOUND)
    message(FATAL_ERROR "*** Python3 is not installed. Please install Python3")
  endif()
endif()

include(BuildExternalDependency)
set(BuildExternalDependency_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/_BuildExternalDependency CACHE INTERNAL "" FORCE)

# pybind11
if(BUILD_PYTHON_BINDINGS)
  BuildExternalDependency(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.6
    CMAKE_ARGS
      -DPYBIND11_TEST=OFF
  )
endif()

# FFmpeg (built as static libraries)
BuildExternalDependency(
  ffmpeg
  GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
  GIT_TAG n6.1
  CUSTOM_BUILD
  SOURCE_SUBDIR .
)

# ##############################################################################
# Subdirectories
# ##############################################################################
add_subdirectory(src)

if(BUILD_PYTHON_BINDINGS)
  add_subdirectory(python)
endif()

