/*
  ==============================================================================


  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace juce
{

/**
 * Interface for seekable media players that can handle both audio and video content.
 * This interface is designed for applications that need precise timing control,
 * particularly for video players synchronized with external DAW playheads.
 */
struct ISeekableMedia
{
    virtual ~ISeekableMedia() = default;

    //==============================================================================
    /** Open/close operations */
    
    /**
     * Opens a media file for playback.
     * @param media The file to open
     * @param error Optional pointer to receive error messages
     * @return true if the file was opened successfully
     */
    virtual bool open (const File& media, String* error = nullptr) = 0;
    
    /**
     * Closes the currently opened media and releases resources.
     */
    virtual void close() = 0;

    //==============================================================================
    /** Playback control */
    
    /**
     * Starts or resumes playback.
     */
    virtual void play() = 0;
    
    /**
     * Pauses playback.
     */
    virtual void pause() = 0;
    
    /**
     * Stops playback and resets position to beginning.
     */
    virtual void stop() = 0;
    
    /**
     * Returns true if the media is currently playing.
     */
    virtual bool isPlaying() const = 0;

    //==============================================================================
    /** Seeking operations */
    
    enum class SeekMode 
    { 
        Fast,       ///< Prioritize speed over accuracy (may seek to nearest keyframe)
        Precise     ///< Prioritize accuracy over speed (may be slower)
    };
    
    /**
     * Seeks to a specific sample position in the audio stream.
     * @param sampleIndex The target sample index at the stream's native sample rate
     * @param mode Whether to prioritize speed or precision
     * @return true if the seek operation was initiated successfully
     */
    virtual bool seekToSample (int64_t sampleIndex, SeekMode mode = SeekMode::Precise) = 0;
    
    /**
     * Seeks to a specific time position.
     * @param timeInSeconds The target time in seconds
     * @param mode Whether to prioritize speed or precision
     * @return true if the seek operation was initiated successfully
     */
    virtual bool seekToTime (double timeInSeconds, SeekMode mode = SeekMode::Precise) = 0;

    //==============================================================================
    /** Rendering and output setup */
    
    /**
     * Sets the component where video should be rendered.
     * @param component The JUCE component to render video into, or nullptr to disable video
     */
    virtual void setVideoComponent (Component* component) = 0;
    
    /**
     * Sets the audio device manager for audio output routing.
     * @param deviceManager The audio device manager, or nullptr to disable audio output
     */
    virtual void setAudioDevice (AudioDeviceManager* deviceManager) = 0;

    //==============================================================================
    /** Timing and information */
    
    /**
     * Returns the sample rate of the audio stream.
     * @return Sample rate in Hz, or 0 if no audio stream or not yet loaded
     */
    virtual int getSampleRate() const = 0;
    
    /**
     * Returns the total number of samples in the media.
     * @return Total samples, or -1 if unknown or not yet loaded
     */
    virtual int64_t getTotalSamples() const = 0;
    
    /**
     * Returns the current playback position in samples.
     * This is based on the audio clock and should be the authoritative timing source.
     * @return Current sample position
     */
    virtual int64_t getCurrentSample() const = 0;
    
    /**
     * Returns the total duration of the media in seconds.
     * @return Duration in seconds, or -1.0 if unknown
     */
    virtual double getTotalDuration() const = 0;
    
    /**
     * Returns the current playback position in seconds.
     * @return Current time in seconds
     */
    virtual double getCurrentTime() const = 0;
    
    /**
     * Returns true if the media has a video stream.
     */
    virtual bool hasVideo() const = 0;
    
    /**
     * Returns true if the media has an audio stream.
     */
    virtual bool hasAudio() const = 0;
    
    /**
     * Returns the video dimensions, if available.
     * @return Video size, or {0, 0} if no video or not yet loaded
     */
    virtual Rectangle<int> getVideoSize() const = 0;

    //==============================================================================
    /** Listener interface for media events */
    
    struct Listener
    {
        virtual ~Listener() = default;
        
        /** Called when media loading is complete and playback is ready. */
        virtual void mediaReady (ISeekableMedia*) {}
        
        /** Called when an error occurs during playback. */
        virtual void mediaError (ISeekableMedia*, const String&) {}
        
        /** Called when playback reaches the end of the media. */
        virtual void mediaFinished (ISeekableMedia*) {}
        
        /** Called when a seek operation completes. */
        virtual void seekCompleted (ISeekableMedia*, int64_t) {}
    };
    
    /**
     * Adds a listener for media events.
     */
    virtual void addListener (Listener* listener) = 0;
    
    /**
     * Removes a listener for media events.
     */
    virtual void removeListener (Listener* listener) = 0;
};

} // namespace juce
