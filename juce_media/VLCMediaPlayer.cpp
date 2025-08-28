/*
  ==============================================================================

   This file is part of the juce_libvlc module.

  ==============================================================================
*/

#include "VLCMediaPlayer.h"

// Include libVLC headers
#include <vlc/vlc.h>

namespace juce
{

//==============================================================================
VLCMediaPlayer::AudioBuffer::AudioBuffer (int channels, int samples)
    : numChannels (channels), numSamples (samples)
{
    resize (channels, samples);
}

VLCMediaPlayer::AudioBuffer::~AudioBuffer()
{
    if (data != nullptr)
    {
        for (int i = 0; i < numChannels; ++i)
            delete[] data[i];
        delete[] data;
    }
}

void VLCMediaPlayer::AudioBuffer::resize (int channels, int samples)
{
    if (data != nullptr)
    {
        for (int i = 0; i < numChannels; ++i)
            delete[] data[i];
        delete[] data;
    }
    
    numChannels = channels;
    numSamples = samples;
    
    if (channels > 0 && samples > 0)
    {
        data = new float*[channels];
        for (int i = 0; i < channels; ++i)
            data[i] = new float[samples];
        clear();
    }
    else
    {
        data = nullptr;
    }
}

void VLCMediaPlayer::AudioBuffer::clear()
{
    writePosition = 0;
    readPosition = 0;
    availableSamples = 0;
    
    if (data != nullptr)
    {
        for (int i = 0; i < numChannels; ++i)
            FloatVectorOperations::clear (data[i], numSamples);
    }
}

//==============================================================================
VLCMediaPlayer::VLCMediaPlayer()
{
    initializeVLC();
    
    // Create audio ring buffer (2 seconds at 48kHz stereo)
    audioRingBuffer = std::make_unique<AudioBuffer> (2, 96000);
    
    // Start timer for position updates (60 FPS)
    startTimer (16);
}

VLCMediaPlayer::~VLCMediaPlayer()
{
    close();
    shutdownVLC();
}

//==============================================================================
void VLCMediaPlayer::initializeVLC()
{
    DBG ("Attempting to initialize libVLC...");
    
    // Check VLC installation first
    juce::File vlcApp ("/Applications/VLC.app");
    if (!vlcApp.exists())
    {
        DBG ("VLC.app not found at /Applications/VLC.app - libVLC will not work");
        return;
    }
    
    // Set environment variables for libVLC to find its components
    juce::File pluginsDir = vlcApp.getChildFile ("Contents/MacOS/plugins");
    juce::File libDir = vlcApp.getChildFile ("Contents/MacOS/lib");
    
    if (pluginsDir.exists())
    {
        DBG ("Setting VLC_PLUGIN_PATH to: " + pluginsDir.getFullPathName());
        setenv("VLC_PLUGIN_PATH", pluginsDir.getFullPathName().toUTF8(), 1);
    }
    
    if (libDir.exists())
    {
        DBG ("Setting DYLD_LIBRARY_PATH to: " + libDir.getFullPathName());
        juce::String currentPath = getenv("DYLD_LIBRARY_PATH") ? getenv("DYLD_LIBRARY_PATH") : "";
        juce::String newPath = libDir.getFullPathName();
        if (currentPath.isNotEmpty())
            newPath += ":" + currentPath;
        setenv("DYLD_LIBRARY_PATH", newPath.toUTF8(), 1);
    }
    
    // Try initialization with minimal, compatible arguments
    const char* const vlc_args[] = {
        "--intf=dummy",                 // Use dummy interface (essential)
        "--no-video-title-show",        // Disable video title overlay
        "--verbose=0",                  // Reduce verbosity
        "--aout=dummy",                 // Use dummy audio output to prevent conflicts with JUCE
        "--network-caching=1000",       // Network caching (ms)
        "--file-caching=1000",          // File caching (ms)
        "--live-caching=1000",          // Live stream caching (ms)
    };
    
    int argc = sizeof(vlc_args) / sizeof(vlc_args[0]);
    DBG ("Trying libVLC initialization with " + juce::String(argc) + " arguments...");
    vlcInstance = libvlc_new (argc, vlc_args);
    
    if (vlcInstance == nullptr)
    {
        DBG ("Failed with arguments, trying with no arguments...");
        vlcInstance = libvlc_new (0, nullptr);
    }
    
    if (vlcInstance == nullptr)
    {
        DBG ("Failed to initialize libVLC instance - all methods failed");
        DBG ("VLC.app path: " + vlcApp.getFullPathName());
        DBG ("VLC lib path: " + libDir.getFullPathName());
        DBG ("VLC plugins path: " + pluginsDir.getFullPathName());
        
        // Check if the library file exists
        juce::File libVLC = libDir.getChildFile ("libvlc.dylib");
        DBG ("libvlc.dylib exists: " + juce::String (libVLC.exists() ? "YES" : "NO"));
        
        // Try to get more detailed error information
        DBG ("Current working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
        DBG ("VLC_PLUGIN_PATH env var: " + juce::String (getenv("VLC_PLUGIN_PATH") ? getenv("VLC_PLUGIN_PATH") : "not set"));
        
        return;
    }
    
    DBG ("libVLC instance created successfully!");
    
    // Get libVLC version info
    const char* version = libvlc_get_version();
    if (version != nullptr)
    {
        DBG ("libVLC version: " + juce::String (version));
    }
    
    mediaPlayer = libvlc_media_player_new (vlcInstance);
    if (mediaPlayer == nullptr)
    {
        DBG ("Failed to create libVLC media player");
        return;
    }
    
    DBG ("libVLC media player created successfully!");
    setupEventHandling();
}

void VLCMediaPlayer::shutdownVLC()
{
    // Stop playback first to prevent callbacks during destruction
    if (mediaPlayer != nullptr)
    {
        libvlc_media_player_stop (mediaPlayer);
        
        // Clear all callbacks before releasing to prevent memory corruption
        libvlc_video_set_callbacks (mediaPlayer, nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks (mediaPlayer, nullptr, nullptr);
        
        // Wait a bit for any running callbacks to finish
        juce::Thread::sleep (50);
        
        libvlc_media_player_release (mediaPlayer);
        mediaPlayer = nullptr;
    }
    
    if (currentMedia != nullptr)
    {
        libvlc_media_release (currentMedia);
        currentMedia = nullptr;
    }
    
    if (vlcInstance != nullptr)
    {
        libvlc_release (vlcInstance);
        vlcInstance = nullptr;
    }
}

//==============================================================================
bool VLCMediaPlayer::open (const File& media, String* error)
{
    close();
    
    if (!media.exists())
    {
        if (error != nullptr)
            *error = "File does not exist: " + media.getFullPathName();
        return false;
    }
    
    if (vlcInstance == nullptr || mediaPlayer == nullptr)
    {
        if (error != nullptr)
            *error = "libVLC not initialized";
        return false;
    }
    
    // Create media from file path
    auto mediaPath = media.getFullPathName().toUTF8();
    currentMedia = libvlc_media_new_path (vlcInstance, mediaPath.getAddress());
    
    if (currentMedia == nullptr)
    {
        if (error != nullptr)
            *error = "Failed to create libVLC media from file";
        return false;
    }
    
    // Set media to player
    libvlc_media_player_set_media (mediaPlayer, currentMedia);
    
    // Setup callbacks
    setupAudioCallbacks();
    setupVideoCallbacks();
    
    // Parse media to get information (use simpler API for compatibility)
    libvlc_media_parse (currentMedia);
    
    // Update media information
    updateMediaInfo();
    
    return true;
}

void VLCMediaPlayer::close()
{
    stop();
    
    if (mediaPlayer != nullptr)
    {
        libvlc_media_player_set_media (mediaPlayer, nullptr);
    }
    
    if (currentMedia != nullptr)
    {
        libvlc_media_release (currentMedia);
        currentMedia = nullptr;
    }
    
    // Reset state
    hasVideoStream = false;
    hasAudioStream = false;
    mediaDuration = -1.0;
    totalAudioSamples = -1;
    currentAudioSample = 0;
    videoWidth = 0;
    videoHeight = 0;
    
    if (audioRingBuffer != nullptr)
        audioRingBuffer->clear();
}

//==============================================================================
void VLCMediaPlayer::play()
{
    if (mediaPlayer != nullptr && currentMedia != nullptr)
    {
        DBG ("Starting playback...");
        
        // Make sure video output is set up before playing
        setupVideoOutput();
        
        libvlc_media_player_play (mediaPlayer);
        isCurrentlyPlaying = true;
        
        DBG ("Playback started successfully");
    }
}

void VLCMediaPlayer::pause()
{
    if (mediaPlayer != nullptr)
    {
        libvlc_media_player_pause (mediaPlayer);
        isCurrentlyPlaying = false;
    }
}

void VLCMediaPlayer::stop()
{
    if (mediaPlayer != nullptr)
    {
        libvlc_media_player_stop (mediaPlayer);
        isCurrentlyPlaying = false;
        currentAudioSample = 0;
        
        if (audioRingBuffer != nullptr)
            audioRingBuffer->clear();
    }
}

bool VLCMediaPlayer::isPlaying() const
{
    return isCurrentlyPlaying.load();
}

//==============================================================================
bool VLCMediaPlayer::seekToSample (int64_t sampleIndex, SeekMode mode)
{
    if (mediaPlayer == nullptr || !hasAudioStream.load())
        return false;
    
    double sampleRate = currentSampleRate.load();
    if (sampleRate <= 0.0)
        return false;
    
    double timeInSeconds = static_cast<double>(sampleIndex) / sampleRate;
    return seekToTime (timeInSeconds, mode);
}

bool VLCMediaPlayer::seekToTime (double timeInSeconds, SeekMode)
{
    if (mediaPlayer == nullptr || currentMedia == nullptr)
        return false;
    
    // Increment seek generation to cancel any in-flight seeks
    ++seekGeneration;
    
    // Convert to milliseconds
    int64_t timeInMs = static_cast<int64_t>(timeInSeconds * 1000.0);
    
    // Use libVLC seeking (compatible with 3.x and 4.x)
    libvlc_media_player_set_time (mediaPlayer, timeInMs);
    int result = 0; // Assume success for now
    
    if (result == 0)
    {
        // Clear audio buffer on seek to prevent stale audio
        if (audioRingBuffer != nullptr)
            audioRingBuffer->clear();
        
        return true;
    }
    
    return false;
}

//==============================================================================
void VLCMediaPlayer::setVideoComponent (Component* component)
{
    videoComponent = component;
    setupVideoOutput();
}

void VLCMediaPlayer::setAudioDevice (AudioDeviceManager* deviceManager)
{
    audioDeviceManager = deviceManager;
    
    if (deviceManager != nullptr)
    {
        // Add this as an audio callback
        deviceManager->addAudioCallback (this);
    }
}

//==============================================================================
int VLCMediaPlayer::getSampleRate() const
{
    return static_cast<int>(currentSampleRate.load());
}

int64_t VLCMediaPlayer::getTotalSamples() const
{
    return totalAudioSamples.load();
}

int64_t VLCMediaPlayer::getCurrentSample() const
{
    return currentAudioSample.load();
}

double VLCMediaPlayer::getTotalDuration() const
{
    return mediaDuration.load();
}

double VLCMediaPlayer::getCurrentTime() const
{
    double sampleRate = currentSampleRate.load();
    if (sampleRate > 0.0)
        return static_cast<double>(getCurrentSample()) / sampleRate;
    return 0.0;
}

bool VLCMediaPlayer::hasVideo() const
{
    return hasVideoStream.load();
}

bool VLCMediaPlayer::hasAudio() const
{
    return hasAudioStream.load();
}

Rectangle<int> VLCMediaPlayer::getVideoSize() const
{
    return Rectangle<int> (0, 0, videoWidth.load(), videoHeight.load());
}

//==============================================================================
void VLCMediaPlayer::addListener (Listener* listener)
{
    listeners.add (listener);
}

void VLCMediaPlayer::removeListener (Listener* listener)
{
    listeners.remove (listener);
}

//==============================================================================
// AudioIODeviceCallback implementation
void VLCMediaPlayer::audioDeviceIOCallback (const float** inputChannelData,
                                           int numInputChannels,
                                           float** outputChannelData,
                                           int numOutputChannels,
                                           int numSamples)
{
    ignoreUnused (inputChannelData, numInputChannels);
    
    // Clear output buffers first
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
            FloatVectorOperations::clear (outputChannelData[channel], numSamples);
    }
    
    if (audioRingBuffer == nullptr || !hasAudioStream.load() || !isPlaying())
        return;
    
    int numChannels = jmin (numOutputChannels, audioRingBuffer->numChannels);
    int availableSamples = audioRingBuffer->availableSamples.load();
    int samplesToRead = jmin (numSamples, availableSamples);
    
    if (samplesToRead > 0)
    {
        int readPos = audioRingBuffer->readPosition.load();
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* dest = outputChannelData[channel];
            float* src = audioRingBuffer->data[channel];
            
            for (int i = 0; i < samplesToRead; ++i)
            {
                dest[i] = src[(readPos + i) % audioRingBuffer->numSamples];
            }
        }
        
        // Update read position
        int newReadPos = (readPos + samplesToRead) % audioRingBuffer->numSamples;
        audioRingBuffer->readPosition = newReadPos;
        audioRingBuffer->availableSamples = availableSamples - samplesToRead;
        
        // Update current sample position
        currentAudioSample += samplesToRead;
    }
}

void VLCMediaPlayer::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                      int numInputChannels,
                                                      float* const* outputChannelData,
                                                      int numOutputChannels,
                                                      int numSamples,
                                                      const AudioIODeviceCallbackContext& context)
{
    ignoreUnused (context);
    // Delegate to the main callback
    audioDeviceIOCallback (const_cast<const float**>(inputChannelData), numInputChannels, 
                          const_cast<float**>(outputChannelData), numOutputChannels, numSamples);
}

void VLCMediaPlayer::audioDeviceAboutToStart (AudioIODevice* device)
{
    ignoreUnused (device);
    // Audio preparation is handled by libVLC callbacks
}

void VLCMediaPlayer::audioDeviceStopped()
{
    // Resources are managed by libVLC
}

void VLCMediaPlayer::audioDeviceError (const String& errorMessage)
{
    DBG ("Audio device error: " + errorMessage);
    // Handle audio device errors if needed
}

//==============================================================================
// Timer callback for position updates
void VLCMediaPlayer::timerCallback()
{
    updateAudioPosition();
}

//==============================================================================
// libVLC Audio Callbacks
void* VLCMediaPlayer::audioLockCallback (void* data, void** pcm_buffer, size_t size)
{
    // Safety check to prevent accessing freed memory
    if (data == nullptr || pcm_buffer == nullptr)
        return nullptr;
        
    auto* player = static_cast<VLCMediaPlayer*>(data);
    
    // Additional safety check
    if (player == nullptr)
        return nullptr;
    
    // Allocate temporary buffer for this audio block
    static thread_local std::vector<uint8_t> tempBuffer;
    tempBuffer.resize (size);
    *pcm_buffer = tempBuffer.data();
    
    return tempBuffer.data();
}

void VLCMediaPlayer::audioUnlockCallback (void* data, void* pcm_buffer, size_t size)
{
    // Safety check to prevent accessing freed memory
    if (data == nullptr)
        return;
        
    auto* player = static_cast<VLCMediaPlayer*>(data);
    
    // Additional safety check
    if (player == nullptr)
        return;
        
    player->processAudioData (pcm_buffer, size);
}

void VLCMediaPlayer::audioPlayCallback (void* data, void* pcm_buffer, size_t size)
{
    // Audio is handled through JUCE's audio system, not directly played here
    ignoreUnused (data, pcm_buffer, size);
}

void VLCMediaPlayer::audioPauseCallback (void* data, int64_t pts)
{
    ignoreUnused (data, pts);
}

void VLCMediaPlayer::audioResumeCallback (void* data, int64_t pts)
{
    ignoreUnused (data, pts);
}

void VLCMediaPlayer::audioFlushCallback (void* data, int64_t)
{
    auto* player = static_cast<VLCMediaPlayer*>(data);
    if (player->audioRingBuffer != nullptr)
        player->audioRingBuffer->clear();
}

void VLCMediaPlayer::audioDrainCallback (void* data)
{
    ignoreUnused (data);
}

//==============================================================================
// libVLC Video Callbacks
void* VLCMediaPlayer::videoLockCallback (void* data, void** planes)
{
    ignoreUnused (data, planes);
    // Video rendering implementation would go here
    return nullptr;
}

void VLCMediaPlayer::videoUnlockCallback (void* data, void* picture, void* const* planes)
{
    ignoreUnused (data, picture, planes);
}

void VLCMediaPlayer::videoDisplayCallback (void* data, void* picture)
{
    ignoreUnused (data, picture);
}

unsigned VLCMediaPlayer::videoFormatCallback (void** data, char* chroma, unsigned* width, 
                                            unsigned* height, unsigned* pitches, unsigned* lines)
{
    // Safety check to prevent accessing freed memory
    if (data == nullptr || *data == nullptr)
        return 0;
        
    auto* player = static_cast<VLCMediaPlayer*>(*data);
    
    // Additional safety check
    if (player == nullptr)
        return 0;
    
    // Set format to RGBA
    memcpy (chroma, "RV32", 4);
    
    player->updateVideoSize (*width, *height);
    
    *pitches = *width * 4; // 4 bytes per pixel for RGBA
    *lines = *height;
    
    return 1; // Success
}

void VLCMediaPlayer::videoCleanupCallback (void* data)
{
    ignoreUnused (data);
}

// Event callback removed - using polling instead for compatibility

//==============================================================================
// Internal methods
void VLCMediaPlayer::setupAudioCallbacks()
{
    if (mediaPlayer == nullptr)
        return;
    
    // For now, use default audio output - we'll route through JUCE's audio system
    // This avoids libVLC audio callback API compatibility issues
    // TODO: Implement proper audio routing in future versions
}

void VLCMediaPlayer::setupVideoCallbacks()
{
    if (mediaPlayer == nullptr)
        return;
    
    // Temporarily disable video callbacks to prevent memory corruption
    // TODO: Implement proper video rendering with thread-safe callbacks
    // libvlc_video_set_callbacks (mediaPlayer,
    //                            videoLockCallback,
    //                            videoUnlockCallback,
    //                            videoDisplayCallback,
    //                            this);
    
    // libvlc_video_set_format_callbacks (mediaPlayer,
    //                                   videoFormatCallback,
    //                                   videoCleanupCallback);
}

void VLCMediaPlayer::setupVideoOutput()
{
    if (mediaPlayer == nullptr || videoComponent == nullptr)
        return;
    
    DBG ("Setting up video output for component: " + juce::String::toHexString ((juce::pointer_sized_int)videoComponent.getComponent()));
    
    // Set native window handle for video output
#if JUCE_WINDOWS
    auto windowHandle = videoComponent->getWindowHandle();
    DBG ("Windows HWND: " + juce::String::toHexString ((juce::pointer_sized_int)windowHandle));
    libvlc_media_player_set_hwnd (mediaPlayer, windowHandle);
#elif JUCE_MAC
    auto windowHandle = videoComponent->getWindowHandle();
    DBG ("macOS NSView: " + juce::String::toHexString ((juce::pointer_sized_int)windowHandle));
    libvlc_media_player_set_nsobject (mediaPlayer, windowHandle);
#elif JUCE_LINUX
    auto windowHandle = videoComponent->getWindowHandle();
    DBG ("Linux X11 Window: " + juce::String::toHexString ((juce::pointer_sized_int)windowHandle));
    libvlc_media_player_set_xwindow (mediaPlayer, (uint32_t)(juce::pointer_sized_int)windowHandle);
#endif
}

void VLCMediaPlayer::setupEventHandling()
{
    if (mediaPlayer == nullptr)
        return;
    
    // Simplified event handling - avoid libVLC event API compatibility issues
    // We'll use polling in timerCallback() instead
    // TODO: Implement proper event handling in future versions
}

void VLCMediaPlayer::updateMediaInfo()
{
    if (currentMedia == nullptr)
        return;
    
    // Get duration
    int64_t durationMs = libvlc_media_get_duration (currentMedia);
    if (durationMs > 0)
    {
        mediaDuration = static_cast<double>(durationMs) / 1000.0;
        
        double sampleRate = currentSampleRate.load();
        if (sampleRate > 0.0)
            totalAudioSamples = static_cast<int64_t>(mediaDuration.load() * sampleRate);
    }
    
    // Check for audio/video tracks
    if (mediaPlayer != nullptr)
    {
        hasAudioStream = (libvlc_audio_get_track_count (mediaPlayer) > 0);
        hasVideoStream = (libvlc_video_get_track_count (mediaPlayer) > 0);
    }
    
    // Notify listeners that media is ready
    notifyListeners ([this](Listener* l) { l->mediaReady (this); });
}

void VLCMediaPlayer::notifyListeners (std::function<void(Listener*)> callback)
{
    listeners.call ([&callback](Listener& l) { callback (&l); });
}

void VLCMediaPlayer::processAudioData (void* buffer, size_t size)
{
    if (audioRingBuffer == nullptr || buffer == nullptr || size == 0)
        return;
    
    // Assume 32-bit float, stereo
    int numSamples = static_cast<int>(size / (sizeof(float) * 2));
    float* audioData = static_cast<float*>(buffer);
    
    int writePos = audioRingBuffer->writePosition.load();
    int availableSpace = audioRingBuffer->numSamples - audioRingBuffer->availableSamples.load();
    int samplesToWrite = jmin (numSamples, availableSpace);
    
    if (samplesToWrite > 0)
    {
        for (int i = 0; i < samplesToWrite; ++i)
        {
            int bufferIndex = (writePos + i) % audioRingBuffer->numSamples;
            audioRingBuffer->data[0][bufferIndex] = audioData[i * 2];     // Left
            audioRingBuffer->data[1][bufferIndex] = audioData[i * 2 + 1]; // Right
        }
        
        audioRingBuffer->writePosition = (writePos + samplesToWrite) % audioRingBuffer->numSamples;
        audioRingBuffer->availableSamples += samplesToWrite;
    }
}

void VLCMediaPlayer::updateAudioPosition()
{
    if (mediaPlayer == nullptr || !isPlaying())
        return;
    
    // Get current time from libVLC
    int64_t currentTimeMs = libvlc_media_player_get_time (mediaPlayer);
    if (currentTimeMs >= 0)
    {
        double currentTimeSeconds = static_cast<double>(currentTimeMs) / 1000.0;
        double sampleRate = currentSampleRate.load();
        if (sampleRate > 0.0)
        {
            int64_t newSamplePosition = static_cast<int64_t>(currentTimeSeconds * sampleRate);
            currentAudioSample = newSamplePosition;
        }
    }
}

void VLCMediaPlayer::updateVideoSize (int width, int height)
{
    videoWidth = width;
    videoHeight = height;
    
    if (videoComponent != nullptr)
    {
        MessageManager::callAsync ([this, width, height]()
        {
            if (videoComponent != nullptr)
                videoComponent->setSize (width, height);
        });
    }
}

} // namespace juce
