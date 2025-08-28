# FindLibVLC.cmake
# Finds the libVLC library and headers
#
# This module defines:
#  LibVLC_FOUND - True if libVLC is found
#  LibVLC_INCLUDE_DIRS - Include directories for libVLC
#  LibVLC_LIBRARIES - Libraries to link against
#  LibVLC_VERSION - Version of libVLC found
#  LibVLC::LibVLC - Imported target for libVLC

cmake_minimum_required(VERSION 3.15)

include(FindPackageHandleStandardArgs)

# Try to find libVLC using pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LibVLC QUIET libvlc)
endif()

# Find the header file
find_path(LibVLC_INCLUDE_DIR
    NAMES vlc/vlc.h
    HINTS
        ${PC_LibVLC_INCLUDEDIR}
        ${PC_LibVLC_INCLUDE_DIRS}
    PATHS
        /usr/include
        /usr/local/include
        /opt/local/include
        # macOS specific paths
        /Applications/VLC.app/Contents/MacOS/include
        /usr/local/Cellar/libvlc/*/include
        # Windows specific paths
        "C:/Program Files/VideoLAN/VLC/sdk/include"
        "C:/Program Files (x86)/VideoLAN/VLC/sdk/include"
        "$ENV{VLC_DIR}/include"
        "$ENV{PROGRAMFILES}/VideoLAN/VLC/sdk/include"
        "$ENV{PROGRAMFILES\(X86\)}/VideoLAN/VLC/sdk/include"
)

# Find the library
find_library(LibVLC_LIBRARY
    NAMES vlc libvlc
    HINTS
        ${PC_LibVLC_LIBDIR}
        ${PC_LibVLC_LIBRARY_DIRS}
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        # macOS specific paths
        /Applications/VLC.app/Contents/MacOS/lib
        /usr/local/Cellar/libvlc/*/lib
        # Windows specific paths
        "C:/Program Files/VideoLAN/VLC/sdk/lib"
        "C:/Program Files (x86)/VideoLAN/VLC/sdk/lib"
        "$ENV{VLC_DIR}/lib"
        "$ENV{PROGRAMFILES}/VideoLAN/VLC/sdk/lib"
        "$ENV{PROGRAMFILES\(X86\)}/VideoLAN/VLC/sdk/lib"
)

# Try to get version information
if(LibVLC_INCLUDE_DIR AND EXISTS "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h")
    file(STRINGS "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h" 
         LibVLC_VERSION_MAJOR_LINE REGEX "^#define[ \t]+LIBVLC_VERSION_MAJOR[ \t]+[0-9]+")
    file(STRINGS "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h" 
         LibVLC_VERSION_MINOR_LINE REGEX "^#define[ \t]+LIBVLC_VERSION_MINOR[ \t]+[0-9]+")
    file(STRINGS "${LibVLC_INCLUDE_DIR}/vlc/libvlc_version.h" 
         LibVLC_VERSION_REVISION_LINE REGEX "^#define[ \t]+LIBVLC_VERSION_REVISION[ \t]+[0-9]+")
    
    string(REGEX REPLACE "^#define[ \t]+LIBVLC_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" 
           LibVLC_VERSION_MAJOR "${LibVLC_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+LIBVLC_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" 
           LibVLC_VERSION_MINOR "${LibVLC_VERSION_MINOR_LINE}")
    string(REGEX REPLACE "^#define[ \t]+LIBVLC_VERSION_REVISION[ \t]+([0-9]+).*" "\\1" 
           LibVLC_VERSION_REVISION "${LibVLC_VERSION_REVISION_LINE}")
    
    set(LibVLC_VERSION "${LibVLC_VERSION_MAJOR}.${LibVLC_VERSION_MINOR}.${LibVLC_VERSION_REVISION}")
endif()

# Handle standard arguments
find_package_handle_standard_args(LibVLC
    REQUIRED_VARS LibVLC_LIBRARY LibVLC_INCLUDE_DIR
    VERSION_VAR LibVLC_VERSION
)

if(LibVLC_FOUND)
    set(LibVLC_LIBRARIES ${LibVLC_LIBRARY})
    set(LibVLC_INCLUDE_DIRS ${LibVLC_INCLUDE_DIR})
    
    # Create imported target
    if(NOT TARGET LibVLC::LibVLC)
        add_library(LibVLC::LibVLC UNKNOWN IMPORTED)
        set_target_properties(LibVLC::LibVLC PROPERTIES
            IMPORTED_LOCATION "${LibVLC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibVLC_INCLUDE_DIR}"
        )
        
        # Add platform-specific link requirements
        if(WIN32)
            # On Windows, we might need additional libraries
            find_library(LibVLCCore_LIBRARY
                NAMES vlccore libvlccore
                HINTS ${PC_LibVLC_LIBDIR} ${PC_LibVLC_LIBRARY_DIRS}
                PATHS
                    "C:/Program Files/VideoLAN/VLC/sdk/lib"
                    "C:/Program Files (x86)/VideoLAN/VLC/sdk/lib"
                    "$ENV{VLC_DIR}/lib"
            )
            if(LibVLCCore_LIBRARY)
                set_property(TARGET LibVLC::LibVLC APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${LibVLCCore_LIBRARY}")
            endif()
        endif()
    endif()
    
    mark_as_advanced(LibVLC_INCLUDE_DIR LibVLC_LIBRARY)
endif()

# Provide some helpful information
if(LibVLC_FOUND)
    message(STATUS "Found libVLC: ${LibVLC_LIBRARY} (version ${LibVLC_VERSION})")
else()
    if(LibVLC_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find libVLC library. Please install VLC media player or set VLC_DIR environment variable.")
    else()
        message(STATUS "libVLC not found. Video playback will not be available.")
    endif()
endif()
