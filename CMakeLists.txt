# CMakeLists.txt for juce_libvlc module
cmake_minimum_required(VERSION 3.15)

# This CMakeLists.txt is primarily for testing the module as a standalone project
# When used as a JUCE module, this file is typically not needed

# Only configure as a project if this is the main CMakeLists.txt
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(juce_libvlc_test VERSION 0.1.0 LANGUAGES CXX)
    
    # Set C++ standard
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    
    # Add cmake modules path for FindLibVLC.cmake
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    
    # Find required packages
    find_package(JUCE REQUIRED)
    find_package(LibVLC REQUIRED)
    
    # Note: We're not using juce_add_module for now, including sources directly in targets
    
    # Build options
    option(JUCE_LIBVLC_BUILD_EXAMPLES "Build juce_libvlc GUI examples" ON)
    option(JUCE_LIBVLC_BUILD_CONSOLE_TEST "Build console test application" OFF)
    
    if(JUCE_LIBVLC_BUILD_EXAMPLES)
        message(STATUS "Building juce_libvlc example application...")
        
        # Create example GUI app
        juce_add_gui_app(VideoPlayerExample
            PRODUCT_NAME "Video Player Example"
            VERSION 1.0.0
            COMPANY_NAME "Mach1"
            COMPANY_WEBSITE "https://mach1.tech"
            COMPANY_EMAIL "whatsup@mach1.tech"
            PLUGIN_MANUFACTURER_CODE "Mac1"
            BUNDLE_ID "com.mach1.juce-libvlc.example"
            PLUGIN_NAME "juce_libvlc"
            PRODUCT_NAME "juce_libvlc"
        )
        
        juce_generate_juce_header(VideoPlayerExample)

        # Add source files to the example
        target_sources(VideoPlayerExample PRIVATE
            examples/example_main.cpp
            examples/VideoPlayerExample.h
        )

        # Link libraries - use standard JUCE linking (gui_extra includes most dependencies)
        target_link_libraries(VideoPlayerExample PRIVATE
            juce::juce_audio_devices  # For AudioDeviceManager and AudioIODeviceCallback
            juce::juce_audio_processors  # For additional audio processing
            juce::juce_gui_extra  # Includes core, events, graphics, gui_basics automatically
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
        )
        
        # Add our libVLC source files directly to the target
        target_sources(VideoPlayerExample PRIVATE
            juce_media/ISeekableMedia.h
            juce_media/VLCMediaPlayer.h
            juce_media/VLCMediaPlayer.cpp
        )
        
        # Link libVLC explicitly
        if(TARGET LibVLC::LibVLC)
            target_link_libraries(VideoPlayerExample PRIVATE LibVLC::LibVLC)
        endif()

        message(STATUS "VideoPlayerExample configured successfully")
    endif()
    
    if(JUCE_LIBVLC_BUILD_CONSOLE_TEST)
        message(STATUS "Building juce_libvlc console test...")
        
        # Create a simple console app to test the module
        juce_add_console_app(juce_libvlc_test
            PRODUCT_NAME "juce_libvlc Test"
            COMPANY_NAME "juce_libvlc"
        )

        juce_generate_juce_header(juce_libvlc_test)
        
        # Add test source files
        target_sources(juce_libvlc_test PRIVATE
            test/test_main.cpp
        )
        
        # Link libraries - our module should bring in all its dependencies
        target_link_libraries(juce_libvlc_test PRIVATE
            juce_libvlc
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
        )
        
        message(STATUS "juce_libvlc test application configured successfully")
    endif()
    
endif()

# Note: When using this as a JUCE module in your project:
# 1. Copy this directory to your project's modules folder
# 2. Add the FindLibVLC.cmake directory to your CMAKE_MODULE_PATH:
#    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/path/to/juce_libvlc/cmake")
# 3. (Optional) Configure libVLC before adding the module:
#    set(LIBVLC_ROOT "/path/to/custom/vlc" CACHE PATH "Custom libVLC root")
#    set(LIBVLC_STATIC ON CACHE BOOL "Link libVLC statically")
# 4. Add the module: juce_add_module(path/to/juce_libvlc)
# 5. Link to your target: target_link_libraries(YourTarget PRIVATE juce_libvlc)
# 6. Link LibVLC explicitly: target_link_libraries(YourTarget PRIVATE LibVLC::LibVLC)