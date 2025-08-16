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
option(USE_GPU "Enable GPU acceleration with compute shaders (default: ON)" ON)

if(USE_SDL AND USE_GLFW)
    message(FATAL_ERROR "Cannot use both SDL and GLFW. Please choose one.")
endif()

if(NOT USE_SDL AND NOT USE_GLFW)
    message(STATUS "No windowing library selected - building console-only version")
endif()

if(USE_GPU)
    message(STATUS "GPU acceleration enabled")
else()
    message(STATUS "GPU acceleration disabled - CPU-only build")
endif()

find_package(Threads REQUIRED)

# Include automatic dependency management
include(dependencies)

# GPU acceleration setup
if(USE_GPU)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GL REQUIRED gl)
    if(GL_FOUND)
        message(STATUS "OpenGL found via pkg-config: ${GL_LIBRARIES}")
        set(GPU_LIBS ${GL_LIBRARIES})
        set(GPU_INCLUDE_DIRS ${GL_INCLUDE_DIRS})
        add_compile_definitions(USE_GPU)
    else()
        # Fallback to CMake's FindOpenGL
        find_package(OpenGL QUIET)
        if(OpenGL_FOUND)
            message(STATUS "OpenGL found: ${OPENGL_LIBRARIES}")
            set(GPU_LIBS ${OPENGL_LIBRARIES})
            add_compile_definitions(USE_GPU)
        else()
            message(WARNING "OpenGL not found - GPU acceleration will be disabled")
            set(USE_GPU OFF)
        endif()
    endif()
endif()

# Validate GPU capabilities if enabled (moved here after GPU setup)
if(USE_GPU)
    validate_gpu_capabilities()
endif()

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

# Combine all libraries for passing to subdirectories
set(ALL_LIBS ${WINDOWING_LIBS} ${GPU_LIBS})
if(GPU_LIBS)
    message(STATUS "GPU libraries configured: ${GPU_LIBS}")
endif()

add_subdirectory(src)