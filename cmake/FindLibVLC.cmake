# FindLibVLC.cmake
# Finds the libVLC library and headers
#
# This module defines:
#  LibVLC_FOUND - True if libVLC is found
#  LibVLC_INCLUDE_DIRS - Include directories for libVLC
#  LibVLC_LIBRARIES - Libraries to link against
#  LibVLC_VERSION - Version of libVLC found
#  LibVLC::LibVLC - Imported target for libVLC
#
# Cache variables:
#  LIBVLC_ROOT - Root directory to search for libVLC (takes precedence)
#  LIBVLC_STATIC - If ON, look for static libraries instead of shared
#  LIBVLC_BUILD_FROM_SOURCE - If ON, build VLC from source in the vlc/ subdirectory

cmake_minimum_required(VERSION 3.15)

include(FindPackageHandleStandardArgs)

# Options
set(LIBVLC_STATIC OFF CACHE BOOL "Link libVLC statically")
set(LIBVLC_BUILD_FROM_SOURCE OFF CACHE BOOL "Build libVLC from source")

# If building from source, check if VLC source directory exists
if(LIBVLC_BUILD_FROM_SOURCE)
    set(VLC_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../vlc")
    
    if(EXISTS "${VLC_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Building libVLC from source in ${VLC_SOURCE_DIR}")
        
        # Add VLC subdirectory
        add_subdirectory("${VLC_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/vlc_build")
        
        # The VLC CMakeLists.txt will create the LibVLC::LibVLC target
        if(TARGET LibVLC::LibVLC)
            set(LibVLC_FOUND TRUE)
            set(LibVLC_INCLUDE_DIRS "${VLC_SOURCE_DIR}/include")
            set(LibVLC_VERSION "4.0.0")
            
            message(STATUS "libVLC configured to build from source")
            return()
        endif()
    else()
        message(WARNING "VLC source directory not found at ${VLC_SOURCE_DIR}. Falling back to system search.")
        set(LIBVLC_BUILD_FROM_SOURCE OFF)
    endif()
endif()

# Try to find libVLC using pkg-config first (unless custom path is specified)
if(NOT LIBVLC_ROOT)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_LibVLC QUIET libvlc)
    endif()
endif()

# Build the search paths, prioritizing LIBVLC_ROOT if set
set(LibVLC_SEARCH_PATHS)
if(LIBVLC_ROOT)
    list(APPEND LibVLC_SEARCH_PATHS
        "${LIBVLC_ROOT}/include"
        "${LIBVLC_ROOT}"
    )
endif()

list(APPEND LibVLC_SEARCH_PATHS
    ${PC_LibVLC_INCLUDEDIR}
    ${PC_LibVLC_INCLUDE_DIRS}
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

# Find the header file
find_path(LibVLC_INCLUDE_DIR
    NAMES vlc/vlc.h
    HINTS ${LibVLC_SEARCH_PATHS}
    NO_DEFAULT_PATH
)

# Fallback to default search if not found
if(NOT LibVLC_INCLUDE_DIR)
    find_path(LibVLC_INCLUDE_DIR
        NAMES vlc/vlc.h
        HINTS ${LibVLC_SEARCH_PATHS}
    )
endif()

# Build library search paths
set(LibVLC_LIB_SEARCH_PATHS)
if(LIBVLC_ROOT)
    list(APPEND LibVLC_LIB_SEARCH_PATHS
        "${LIBVLC_ROOT}/lib"
        "${LIBVLC_ROOT}/lib64"
        "${LIBVLC_ROOT}"
    )
endif()

list(APPEND LibVLC_LIB_SEARCH_PATHS
    ${PC_LibVLC_LIBDIR}
    ${PC_LibVLC_LIBRARY_DIRS}
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

# Determine library names based on static/dynamic preference
if(LIBVLC_STATIC)
    if(WIN32)
        set(LibVLC_NAMES libvlc.lib vlc.lib)
    else()
        set(LibVLC_NAMES libvlc.a)
    endif()
else()
    if(WIN32)
        set(LibVLC_NAMES libvlc.dll.a libvlc.lib vlc.lib)
    else()
        set(LibVLC_NAMES vlc libvlc)
    endif()
endif()

# Find the library
find_library(LibVLC_LIBRARY
    NAMES ${LibVLC_NAMES}
    HINTS ${LibVLC_LIB_SEARCH_PATHS}
    NO_DEFAULT_PATH
)

# Fallback to default search if not found
if(NOT LibVLC_LIBRARY)
    find_library(LibVLC_LIBRARY
        NAMES ${LibVLC_NAMES}
        HINTS ${LibVLC_LIB_SEARCH_PATHS}
    )
endif()

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
        # Determine library type
        if(LIBVLC_STATIC)
            add_library(LibVLC::LibVLC STATIC IMPORTED)
        else()
            add_library(LibVLC::LibVLC UNKNOWN IMPORTED)
        endif()
        
        set_target_properties(LibVLC::LibVLC PROPERTIES
            IMPORTED_LOCATION "${LibVLC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibVLC_INCLUDE_DIR}"
        )
        
        # Add platform-specific link requirements
        if(WIN32)
            # On Windows, we might need additional libraries
            if(LIBVLC_STATIC)
                set(LibVLCCore_NAMES libvlccore.lib vlccore.lib libvlccore.a)
            else()
                set(LibVLCCore_NAMES libvlccore.dll.a libvlccore.lib vlccore.lib vlccore libvlccore)
            endif()
            
            find_library(LibVLCCore_LIBRARY
                NAMES ${LibVLCCore_NAMES}
                HINTS ${LibVLC_LIB_SEARCH_PATHS}
                NO_DEFAULT_PATH
            )
            
            if(NOT LibVLCCore_LIBRARY)
                find_library(LibVLCCore_LIBRARY
                    NAMES ${LibVLCCore_NAMES}
                    HINTS ${LibVLC_LIB_SEARCH_PATHS}
                )
            endif()
            
            if(LibVLCCore_LIBRARY)
                set_property(TARGET LibVLC::LibVLC APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${LibVLCCore_LIBRARY}")
                list(APPEND LibVLC_LIBRARIES "${LibVLCCore_LIBRARY}")
            endif()
        endif()
    endif()
    
    mark_as_advanced(LibVLC_INCLUDE_DIR LibVLC_LIBRARY)
endif()

# Provide some helpful information
if(LibVLC_FOUND)
    set(_libvlc_type "shared")
    if(LIBVLC_STATIC)
        set(_libvlc_type "static")
    endif()
    
    if(LIBVLC_ROOT)
        message(STATUS "Found libVLC (${_libvlc_type}, custom): ${LibVLC_LIBRARY} (version ${LibVLC_VERSION})")
        message(STATUS "  Using custom libVLC from: ${LIBVLC_ROOT}")
    else()
        message(STATUS "Found libVLC (${_libvlc_type}): ${LibVLC_LIBRARY} (version ${LibVLC_VERSION})")
    endif()
else()
    if(LibVLC_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find libVLC library. Please install VLC media player, set LIBVLC_ROOT to a custom path, or set VLC_DIR environment variable.")
    else()
        message(STATUS "libVLC not found. Video playback will not be available.")
    endif()
endif()
