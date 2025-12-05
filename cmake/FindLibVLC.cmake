# FindLibVLC.cmake
# Finds the libVLC library and headers
#
# This module defines:
#  LibVLC_FOUND - True if libVLC is found
#  LibVLC_INCLUDE_DIRS - Include directories for libVLC
#  LibVLC_LIBRARIES - Libraries to link against
#  LibVLC_VERSION - Version of libVLC found
#  LibVLC::LibVLC - Imported target for libVLC
#  LibVLC_RUNTIME_DIR - Directory containing runtime DLLs (Windows)
#  LibVLC_PLUGINS_DIR - Directory containing VLC plugins
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

# If building from source, look for pre-built VLC libraries
# User should run:
#   macOS/Linux: m1-player/build_vlc.sh <build-dir>
#   Windows: powershell m1-player/build_vlc.ps1 -BuildDir <build-dir>
if(LIBVLC_BUILD_FROM_SOURCE)
    set(VLC_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../vlc")
    set(VLC_INSTALL_DIR "${CMAKE_BINARY_DIR}/vlc-install")
    
    # Windows: Check for downloaded SDK
    if(WIN32)
        message(STATUS "Building from source (Windows): Looking for VLC SDK")
        message(STATUS "  Expected location: ${VLC_INSTALL_DIR}")
        
        # Check for Windows SDK structure (lib/libvlc.lib)
        if(EXISTS "${VLC_INSTALL_DIR}/lib/libvlc.lib" AND EXISTS "${VLC_INSTALL_DIR}/include/vlc/vlc.h")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlc.lib")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlccore.lib")
            
            # Set variables
            set(LIBVLC_ROOT "${VLC_INSTALL_DIR}")
            set(LibVLC_FOUND TRUE)
            set(LibVLC_INCLUDE_DIRS "${VLC_INSTALL_DIR}/include")
            set(LibVLC_VERSION "3.0.21")
            
            # Find the DLLs for runtime
            set(LibVLC_RUNTIME_DIR "${VLC_INSTALL_DIR}/bin")
            set(LibVLC_PLUGINS_DIR "${VLC_INSTALL_DIR}/lib/vlc/plugins")
            
            # Create imported target for libvlc
            add_library(vlc_lib SHARED IMPORTED GLOBAL)
            set_target_properties(vlc_lib PROPERTIES
                IMPORTED_IMPLIB "${VLC_INSTALL_DIR}/lib/libvlc.lib"
                IMPORTED_LOCATION "${VLC_INSTALL_DIR}/bin/libvlc.dll"
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            # Create imported target for libvlccore
            add_library(vlccore_lib SHARED IMPORTED GLOBAL)
            set_target_properties(vlccore_lib PROPERTIES
                IMPORTED_IMPLIB "${VLC_INSTALL_DIR}/lib/libvlccore.lib"
                IMPORTED_LOCATION "${VLC_INSTALL_DIR}/bin/libvlccore.dll"
            )
            
            # Create LibVLC::LibVLC interface target
            add_library(LibVLC::LibVLC INTERFACE IMPORTED GLOBAL)
            set_property(TARGET LibVLC::LibVLC PROPERTY
                INTERFACE_LINK_LIBRARIES vlc_lib vlccore_lib
            )
            set_property(TARGET LibVLC::LibVLC PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            set(LibVLC_LIBRARIES vlc_lib vlccore_lib)
            
            # Store paths for bundling
            set(LIBVLC_RUNTIME_DIR "${LibVLC_RUNTIME_DIR}" CACHE PATH "VLC runtime DLLs directory")
            set(LIBVLC_PLUGINS_DIR "${LibVLC_PLUGINS_DIR}" CACHE PATH "VLC plugins directory")
            
            message(STATUS "Using VLC SDK (Windows dynamic linking)")
            message(STATUS "  Runtime DLLs: ${LibVLC_RUNTIME_DIR}")
            message(STATUS "  Plugins: ${LibVLC_PLUGINS_DIR}")
            return()
        else()
            message(STATUS "")
            message(STATUS "========================================")
            message(STATUS "VLC SDK not found!")
            message(STATUS "========================================")
            message(STATUS "This is expected on first configure.")
            message(STATUS "Download and setup VLC SDK by running:")
            message(STATUS "  cd ${CMAKE_SOURCE_DIR}")
            if(CMAKE_BINARY_DIR MATCHES "build-dev")
                message(STATUS "  powershell -ExecutionPolicy Bypass -File build_vlc.ps1 -BuildDir build-dev")
            else()
                message(STATUS "  powershell -ExecutionPolicy Bypass -File build_vlc.ps1 -BuildDir build")
            endif()
            message(STATUS "")
            message(STATUS "Or from the repo root:")
            message(STATUS "  make build-vlc")
            message(STATUS "")
            message(STATUS "Then reconfigure CMake:")
            if(CMAKE_BINARY_DIR MATCHES "build-dev")
                message(STATUS "  make dev-player")
            else()
                message(STATUS "  make configure")
            endif()
            message(STATUS "========================================")
            message(STATUS "")
            message(WARNING "VLC SDK not found. Run build_vlc.ps1 then reconfigure.")
            
            set(LIBVLC_BUILD_FROM_SOURCE OFF)
            set(LibVLC_FOUND FALSE)
        endif()
    # macOS/Linux: Check for source build
    elseif(EXISTS "${VLC_SOURCE_DIR}/meson.build" OR EXISTS "${VLC_SOURCE_DIR}/configure.ac" OR EXISTS "${VLC_SOURCE_DIR}/configure")
        message(STATUS "Building from source: Looking for pre-built VLC libraries")
        message(STATUS "  Expected location: ${VLC_INSTALL_DIR}")
        
        # Check if shared libraries exist (if not strictly static)
        if(NOT LIBVLC_STATIC AND EXISTS "${VLC_INSTALL_DIR}/lib/libvlc.dylib")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlc.dylib")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlccore.dylib")

            # Set variables to point to pre-built libraries
            set(LIBVLC_ROOT "${VLC_INSTALL_DIR}")
            set(LibVLC_FOUND TRUE)
            set(LibVLC_INCLUDE_DIRS "${VLC_INSTALL_DIR}/include")
            # Assume version
            set(LibVLC_VERSION "3.0.22")

            # Create imported targets
            add_library(LibVLC::LibVLC SHARED IMPORTED GLOBAL)
            set_target_properties(LibVLC::LibVLC PROPERTIES
                IMPORTED_LOCATION "${VLC_INSTALL_DIR}/lib/libvlc.dylib"
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            message(STATUS "Using pre-built VLC shared libraries")
            return()
        endif()

        # Check if static libraries exist
        if(EXISTS "${VLC_INSTALL_DIR}/lib/libvlc.a" AND EXISTS "${VLC_INSTALL_DIR}/lib/libvlccore.a")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlc.a")
            message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/libvlccore.a")
            
            # Set variables to point to pre-built libraries
            set(LIBVLC_ROOT "${VLC_INSTALL_DIR}")
            set(LIBVLC_STATIC ON)
            set(LibVLC_FOUND TRUE)
            set(LibVLC_INCLUDE_DIRS "${VLC_INSTALL_DIR}/include")
            set(LibVLC_VERSION "3.0.22")
            
            # Create imported targets
            add_library(vlc STATIC IMPORTED GLOBAL)
            set_target_properties(vlc PROPERTIES
                IMPORTED_LOCATION "${VLC_INSTALL_DIR}/lib/libvlc.a"
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            add_library(vlccore STATIC IMPORTED GLOBAL)
            set_target_properties(vlccore PROPERTIES
                IMPORTED_LOCATION "${VLC_INSTALL_DIR}/lib/libvlccore.a"
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            # Add libcompat if it exists (provides memrchr, tdestroy, etc.)
            if(EXISTS "${VLC_INSTALL_DIR}/lib/vlc/libcompat.a")
                add_library(vlccompat STATIC IMPORTED GLOBAL)
                set_target_properties(vlccompat PROPERTIES
                    IMPORTED_LOCATION "${VLC_INSTALL_DIR}/lib/vlc/libcompat.a"
                )
                message(STATUS "  Found: ${VLC_INSTALL_DIR}/lib/vlc/libcompat.a")
            endif()
            
            # Create LibVLC::LibVLC interface target with all dependencies
            add_library(LibVLC::LibVLC INTERFACE IMPORTED GLOBAL)
            
            # Link VLC libraries
            set(VLC_LINK_LIBS vlc vlccore)
            if(TARGET vlccompat)
                list(APPEND VLC_LINK_LIBS vlccompat)
            endif()
            
            # Add system dependencies for static linking
            if(APPLE)
                # macOS frameworks and libraries required by VLC
                list(APPEND VLC_LINK_LIBS
                    "-liconv"                          # Character encoding
                    "-framework CoreFoundation"        # macOS Core Foundation
                    "-framework CoreServices"          # macOS Core Services
                    "m"                                # Math library
                )
            elseif(UNIX)
                # Linux system libraries
                list(APPEND VLC_LINK_LIBS
                    "iconv"
                    "m"
                )
            endif()
            
            set_property(TARGET LibVLC::LibVLC PROPERTY
                INTERFACE_LINK_LIBRARIES ${VLC_LINK_LIBS}
            )
            set_property(TARGET LibVLC::LibVLC PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES "${VLC_INSTALL_DIR}/include"
            )
            
            set(LibVLC_LIBRARIES ${VLC_LINK_LIBS})
            
            message(STATUS "Using pre-built VLC static libraries")
            return()
        else()
            message(STATUS "")
            message(STATUS "========================================")
            message(STATUS "VLC libraries not found!")
            message(STATUS "========================================")
            message(STATUS "This is expected on first configure.")
            message(STATUS "Build VLC first by running:")
            message(STATUS "  cd ${CMAKE_SOURCE_DIR}")
            if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BINARY_DIR MATCHES "build-dev")
                message(STATUS "  ./build_vlc.sh build-dev")
            else()
                message(STATUS "  ./build_vlc.sh build")
            endif()
            message(STATUS "")
            message(STATUS "Or from the repo root:")
            message(STATUS "  make build-vlc")
            message(STATUS "")
            message(STATUS "Then reconfigure CMake:")
            if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BINARY_DIR MATCHES "build-dev")
                message(STATUS "  make dev-player")
            else()
                message(STATUS "  make configure")
            endif()
            message(STATUS "========================================")
            message(STATUS "")
            message(WARNING "VLC static libraries not built yet. Run build_vlc.sh then reconfigure.")
            
            # Don't fail here - allow configuration to complete so build directory exists
            # User will need to build VLC and reconfigure
            set(LIBVLC_BUILD_FROM_SOURCE OFF)
            set(LibVLC_FOUND FALSE)
        endif()
    else()
        message(WARNING "VLC source not found at ${VLC_SOURCE_DIR}")
        message(WARNING "  Run: git submodule update --init --recursive")
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
