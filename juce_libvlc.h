/*
  ==============================================================================

  BEGIN_JUCE_MODULE_DECLARATION

  ID:                 juce_libvlc
  vendor:             juce_libvlc
  version:            0.1.0
  name:               libVLC wrapper for handling video and audio
  description:        Provides classes to read video/audio streams from video files using libVLC with precise seeking capabilities for DAW synchronization
  dependencies:       juce_audio_basics juce_audio_devices juce_audio_formats juce_gui_basics juce_graphics juce_core juce_audio_utils juce_audio_processors
  linuxLibs:          vlc
  mingwLibs:          vlc
  OSXFrameworks:      
  iOSFrameworks:
  minimumCppStandard: 17

  END_JUCE_MODULE_DECLARATION

  ==============================================================================
*/

#pragma once

// JUCE includes
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

// Module components
#include "juce_media/ISeekableMedia.h"
#include "juce_media/VLCMediaPlayer.h"

/**
 * @file juce_libvlc.h
 * @brief Main header for the juce_libvlc module
 * 
 * This module provides libVLC-based media playback capabilities for JUCE applications,
 * with a focus on video playback and precise seeking for DAW synchronization.
 * 
 * Key features:
 * - Video and audio playback using libVLC
 * - Precise seeking by audio sample or time
 * - Integration with JUCE's audio and video systems
 * - Thread-safe operation with proper callback handling
 * - Support for external playhead synchronization
 * 
 * Usage example:
 * @code
 * auto mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();
 * mediaPlayer->setVideoComponent(&myVideoComponent);
 * mediaPlayer->setAudioDevice(&myAudioDeviceManager);
 * 
 * if (mediaPlayer->open(videoFile))
 * {
 *     mediaPlayer->play();
 *     
 *     // Seek to specific sample (for DAW sync)
 *     mediaPlayer->seekToSample(samplePosition, ISeekableMedia::SeekMode::Precise);
 * }
 * @endcode
 */

namespace juce
{
    // All classes are already included via the individual headers above
}
