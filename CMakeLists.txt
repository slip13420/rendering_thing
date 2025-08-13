cmake_minimum_required(VERSION 3.16)
project(PathTracerRenderer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Set build-type specific optimization flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Global compiler flags will be set per-target in src/CMakeLists.txt

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Target platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")

option(USE_SDL "Use SDL for windowing (default: ON)" ON)
option(USE_GLFW "Use GLFW for windowing (default: OFF)" OFF)

if(USE_SDL AND USE_GLFW)
    message(FATAL_ERROR "Cannot use both SDL and GLFW. Please choose one.")
endif()

if(NOT USE_SDL AND NOT USE_GLFW)
    message(STATUS "No windowing library selected - building console-only version")
endif()

find_package(Threads REQUIRED)

# Include automatic dependency management
include(dependencies)

# Configure windowing libraries after dependencies are resolved
if(USE_SDL)
    message(STATUS "Using SDL for windowing")
    set(WINDOWING_LIBS SDL2::SDL2 SDL2::SDL2main)
    add_compile_definitions(USE_SDL)
endif()

if(USE_GLFW)
    message(STATUS "Using GLFW for windowing")
    set(WINDOWING_LIBS glfw)
    add_compile_definitions(USE_GLFW)
endif()

add_subdirectory(src)