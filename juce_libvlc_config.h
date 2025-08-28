/*
  ==============================================================================

   Configuration header for juce_libvlc module
   This file contains compile-time configuration for the module.

  ==============================================================================
*/

#pragma once

// Module configuration
#ifndef JUCE_LIBVLC_ENABLED
 #define JUCE_LIBVLC_ENABLED 1
#endif

// Platform-specific libVLC configuration
#if JUCE_MAC
 #ifndef JUCE_LIBVLC_USE_FRAMEWORK
  #define JUCE_LIBVLC_USE_FRAMEWORK 0
 #endif
#endif

#if JUCE_WINDOWS
 #ifndef JUCE_LIBVLC_STATIC_LINKING
  #define JUCE_LIBVLC_STATIC_LINKING 0
 #endif
#endif

// Debug configuration
#ifndef JUCE_LIBVLC_DEBUG_LOGGING
 #if JUCE_DEBUG
  #define JUCE_LIBVLC_DEBUG_LOGGING 1
 #else
  #define JUCE_LIBVLC_DEBUG_LOGGING 0
 #endif
#endif

// Audio buffer configuration
#ifndef JUCE_LIBVLC_AUDIO_BUFFER_SIZE
 #define JUCE_LIBVLC_AUDIO_BUFFER_SIZE 96000  // 2 seconds at 48kHz
#endif

#ifndef JUCE_LIBVLC_DEFAULT_SAMPLE_RATE
 #define JUCE_LIBVLC_DEFAULT_SAMPLE_RATE 44100
#endif

// Video configuration
#ifndef JUCE_LIBVLC_USE_NATIVE_WINDOW
 #define JUCE_LIBVLC_USE_NATIVE_WINDOW 1
#endif
