# Locate SDL2 library
# This module defines
# SDL2_FOUND - system has SDL2
# SDL2_INCLUDE_DIRS - the SDL2 include directory
# SDL2_LIBRARIES - Link these to use SDL2

if(WIN32)
    find_path(SDL2_INCLUDE_DIR SDL.h
        HINTS
        $ENV{SDL2DIR}
        PATH_SUFFIXES include/SDL2 include
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/include/SDL2
        /usr/include/SDL2
        /sw/include/SDL2
        /opt/local/include/SDL2
        /opt/csw/include/SDL2
        /opt/include/SDL2
    )
    
    find_library(SDL2_LIBRARY
        NAMES SDL2 libSDL2
        HINTS
        $ENV{SDL2DIR}
        PATH_SUFFIXES lib64 lib lib/x64 lib/x86
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /usr
        /sw
        /opt/local
        /opt/csw
        /opt
    )
    
    find_library(SDL2MAIN_LIBRARY
        NAMES SDL2main libSDL2main
        HINTS
        $ENV{SDL2DIR}
        PATH_SUFFIXES lib64 lib lib/x64 lib/x86
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /usr
        /sw
        /opt/local
        /opt/csw
        /opt
    )
else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(_SDL2 QUIET sdl2)
    endif()
    
    find_path(SDL2_INCLUDE_DIR
        NAMES SDL.h
        HINTS
        ${_SDL2_INCLUDE_DIRS}
        PATHS
        /usr/include/SDL2
        /usr/local/include/SDL2
        PATH_SUFFIXES SDL2
    )
    
    find_library(SDL2_LIBRARY
        NAMES SDL2
        HINTS
        ${_SDL2_LIBRARY_DIRS}
        PATHS
        /usr/lib
        /usr/local/lib
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
    VERSION_VAR SDL2_VERSION_STRING)

if(SDL2_FOUND)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    if(SDL2MAIN_LIBRARY)
        list(APPEND SDL2_LIBRARIES ${SDL2MAIN_LIBRARY})
    endif()
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
    endif()
    
    if(SDL2MAIN_LIBRARY AND NOT TARGET SDL2::SDL2main)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
    endif()
endif()