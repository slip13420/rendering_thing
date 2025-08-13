# Automatic dependency management using FetchContent
# This file handles automatic download and configuration of external dependencies
# Supports cross-platform builds on Linux, Windows, and macOS

include(FetchContent)

# Set FetchContent properties for better performance and reliability
set(FETCHCONTENT_QUIET FALSE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Enable FetchContent caching for better offline support
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
    set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")
endif()

# Define dependency versions for consistency and easy maintenance
set(SDL2_VERSION "release-2.28.5")
set(GLFW_VERSION "3.3.9")

# Trusted repository URLs for security
set(SDL2_REPO_URL "https://github.com/libsdl-org/SDL.git")
set(GLFW_REPO_URL "https://github.com/glfw/glfw.git")

# Platform-specific configurations
if(WIN32)
    message(STATUS "Configuring dependencies for Windows")
    # Windows-specific dependency settings if needed
elseif(APPLE)
    message(STATUS "Configuring dependencies for macOS")
    # macOS-specific dependency settings if needed
else()
    message(STATUS "Configuring dependencies for Linux")
    # Linux-specific dependency settings if needed
endif()

# SDL2 dependency with automatic download
if(USE_SDL)
    message(STATUS "Configuring SDL2 dependency...")
    
    # First try to find system-installed SDL2
    find_package(SDL2 QUIET)
    
    if(NOT SDL2_FOUND OR NOT TARGET SDL2::SDL2main)
        if(SDL2_FOUND AND NOT TARGET SDL2::SDL2main)
            message(STATUS "System SDL2 found but SDL2main not available, downloading complete SDL2...")
        else()
            message(STATUS "SDL2 not found on system, downloading automatically...")
        endif()
        
        FetchContent_Declare(
            SDL2
            GIT_REPOSITORY ${SDL2_REPO_URL}
            GIT_TAG ${SDL2_VERSION}  # Pinned to specific version for consistency
            GIT_SHALLOW TRUE
        )
        
        # Configure SDL2 build options
        set(SDL_SHARED OFF CACHE BOOL "Build SDL2 as shared library")
        set(SDL_STATIC ON CACHE BOOL "Build SDL2 as static library")
        set(SDL_TEST OFF CACHE BOOL "Build SDL2 tests")
        set(SDL_TESTS OFF CACHE BOOL "Build SDL2 tests")
        
        # Platform-specific SDL2 options
        if(WIN32)
            set(SDL_DIRECTX ON CACHE BOOL "Enable DirectX support on Windows")
        elseif(APPLE)
            set(SDL_COCOA ON CACHE BOOL "Enable Cocoa support on macOS")
        endif()
        
        FetchContent_MakeAvailable(SDL2)
        
        # Create SDL2::SDL2 and SDL2::SDL2main aliases for compatibility
        if(TARGET SDL2-static)
            add_library(SDL2::SDL2 ALIAS SDL2-static)
            add_library(SDL2::SDL2main ALIAS SDL2main)
        elseif(TARGET SDL2)
            add_library(SDL2::SDL2 ALIAS SDL2)
            add_library(SDL2::SDL2main ALIAS SDL2main)
        endif()
        
        message(STATUS "SDL2 downloaded and configured successfully")
    else()
        message(STATUS "Using system-installed SDL2 with all required targets")
    endif()
endif()

# GLFW dependency with automatic download
if(USE_GLFW)
    message(STATUS "Configuring GLFW dependency...")
    
    # First try to find system-installed GLFW
    find_package(glfw3 QUIET)
    
    if(NOT glfw3_FOUND)
        message(STATUS "GLFW not found on system, downloading automatically...")
        
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY ${GLFW_REPO_URL}
            GIT_TAG ${GLFW_VERSION}  # Pinned to specific version for consistency
            GIT_SHALLOW TRUE
        )
        
        # Configure GLFW build options
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
        set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install target")
        
        FetchContent_MakeAvailable(glfw)
        
        message(STATUS "GLFW downloaded and configured successfully")
    else()
        message(STATUS "Using system-installed GLFW")
    endif()
endif()

# Function to provide detailed dependency diagnostics
function(log_dependency_info)
    message(STATUS "=== Dependency Resolution Summary ===")
    message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "CMake Version: ${CMAKE_VERSION}")
    message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    
    if(USE_SDL)
        message(STATUS "SDL2 Configuration:")
        if(TARGET SDL2::SDL2)
            message(STATUS "  ✓ SDL2::SDL2 target available")
        else()
            message(STATUS "  ✗ SDL2::SDL2 target missing")
        endif()
        if(TARGET SDL2::SDL2main)
            message(STATUS "  ✓ SDL2::SDL2main target available")
        else()
            message(STATUS "  ✗ SDL2::SDL2main target missing")
        endif()
    endif()
    
    if(USE_GLFW)
        message(STATUS "GLFW Configuration:")
        if(TARGET glfw)
            message(STATUS "  ✓ GLFW target available")
        else()
            message(STATUS "  ✗ GLFW target missing")
        endif()
    endif()
    message(STATUS "=====================================")
endfunction()

# Function to check if all dependencies are available
function(check_dependencies_available)
    set(missing_deps "")
    set(error_details "")
    
    if(USE_SDL)
        if(NOT TARGET SDL2::SDL2)
            list(APPEND missing_deps "SDL2::SDL2")
            list(APPEND error_details "SDL2 main library target not found")
        endif()
        if(NOT TARGET SDL2::SDL2main)
            list(APPEND missing_deps "SDL2::SDL2main")
            list(APPEND error_details "SDL2main library target not found (required for main function on some platforms)")
        endif()
    endif()
    
    if(USE_GLFW AND NOT TARGET glfw)
        list(APPEND missing_deps "GLFW")
        list(APPEND error_details "GLFW library target not found")
    endif()
    
    log_dependency_info()
    
    if(missing_deps)
        string(JOIN ", " missing_deps_str ${missing_deps})
        string(JOIN "\n  - " error_details_str ${error_details})
        message(FATAL_ERROR 
            "\n=== DEPENDENCY CONFIGURATION FAILED ===\n"
            "Missing targets: ${missing_deps_str}\n"
            "Error details:\n  - ${error_details_str}\n"
            "\nTroubleshooting steps:\n"
            "1. Check internet connection for automatic downloads\n"
            "2. Verify CMake version >= 3.16\n"
            "3. Ensure C++17 compatible compiler\n"
            "4. Try cleaning build directory: rm -rf build/\n"
            "5. Check DEPENDENCIES.md for more information\n"
            "========================================")
    endif()
    
    message(STATUS "✓ All dependencies configured successfully")
endfunction()

# Validate that dependencies are properly configured
check_dependencies_available()