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
    
    // When using statically linked VLC, plugins are loaded from bundled location
    // Try to find plugins relative to the application bundle
    juce::File appBundle = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    juce::File pluginsDir;
    
    DBG ("Application path: " + appBundle.getFullPathName());
    
    // First check if we're in an app bundle
    if (appBundle.getFileExtension() == ".app")
    {
        // VLC plugins are stored in Resources (not PlugIns) to avoid codesign issues
        // with the plugins.dat cache file which is a data file, not code
        pluginsDir = appBundle.getChildFile("Contents/Resources/vlc/plugins");
        DBG ("Checking for plugins at: " + pluginsDir.getFullPathName());
        
        if (!pluginsDir.exists())
        {
            // Try legacy PlugIns location for backwards compatibility
            pluginsDir = appBundle.getChildFile("Contents/PlugIns/vlc/plugins");
            DBG ("Checking legacy path: " + pluginsDir.getFullPathName());
        }
        
        // Set library path for VLC plugin dependencies
        // Our bundle_vlc_deps.sh script copies Homebrew dependencies to vlc/lib/
        // and rewrites plugin paths to use @loader_path/../../lib/
        // We need to ensure the dyld library path includes this location for any
        // plugins that might have transitive dependencies
        juce::File libsDir = appBundle.getChildFile("Contents/Resources/vlc/lib");
        if (libsDir.exists())
        {
            DBG ("Found bundled VLC libraries at: " + libsDir.getFullPathName());
            
            // Get current DYLD_LIBRARY_PATH and prepend our libs dir
            const char* currentPath = getenv("DYLD_LIBRARY_PATH");
            juce::String newPath = libsDir.getFullPathName();
            if (currentPath != nullptr && strlen(currentPath) > 0)
            {
                newPath += ":" + juce::String(currentPath);
            }
            setenv("DYLD_LIBRARY_PATH", newPath.toUTF8(), 1);
            DBG ("Set DYLD_LIBRARY_PATH to include: " + libsDir.getFullPathName());
        }
    }
    else
    {
        // Running from build directory or as standalone executable
        // Try to find plugins relative to executable
        juce::File executableDir = appBundle.getParentDirectory();
        pluginsDir = executableDir.getChildFile("vlc-install/lib/vlc/plugins");
        DBG ("Checking build directory path: " + pluginsDir.getFullPathName());
    }
    
    // If plugins found, set the path for VLC
    if (pluginsDir.exists())
    {
        // Verify plugins.dat exists
        juce::File pluginsCache = pluginsDir.getChildFile("plugins.dat");
        if (pluginsCache.exists())
        {
            DBG ("Found VLC plugins cache at: " + pluginsCache.getFullPathName());
        }
        else
        {
            DBG ("WARNING: plugins.dat not found - VLC may fail to load plugins!");
            DBG ("Run 'vlc-cache-gen' on the plugins directory to generate this file.");
        }
        
        DBG ("Setting VLC_PLUGIN_PATH to: " + pluginsDir.getFullPathName());
        setenv("VLC_PLUGIN_PATH", pluginsDir.getFullPathName().toUTF8(), 1);
    }
    else
    {
        DBG ("VLC plugins not found in expected locations");
        DBG ("VLC will attempt to use static plugins or system paths");
    }
    
    // Build VLC initialization arguments
    // Note: Don't specify --vout explicitly - VLC automatically uses vmem when
    // libvlc_video_set_callbacks() is called. Specifying it too early can cause issues.
    // Audio is disabled since we handle audio through JUCE's audio system.
    const char* const vlc_args[] = {
        "--intf=dummy",                 // Use dummy interface (no UI)
        "--no-video-title-show",        // Disable video title overlay
        "--verbose=2",                  // Enable verbose output for debugging
        "--no-audio",                   // Disable VLC audio output (JUCE handles audio)
        "--network-caching=1000",       // Network caching (ms)
        "--file-caching=1000",          // File caching (ms)
        "--live-caching=1000",          // Live stream caching (ms)
#if JUCE_MAC
        "--no-xlib",                    // Disable X11 on macOS
#endif
        "--no-drop-late-frames",        // Don't drop frames (for precise seeking)
        "--no-skip-frames",             // Don't skip frames
    };
    
    int argc = sizeof(vlc_args) / sizeof(vlc_args[0]);
    DBG ("Trying libVLC initialization with " + juce::String(argc) + " arguments...");
    vlcInstance = libvlc_new (argc, vlc_args);
    
    if (vlcInstance == nullptr)
    {
        // Try with fewer arguments if first attempt fails
        DBG ("First attempt failed, trying with minimal arguments...");
        const char* const minimal_args[] = {
            "--intf=dummy",
            "--no-video-title-show",
            "--verbose=2",
        };
        vlcInstance = libvlc_new (3, minimal_args);
    }
    
    if (vlcInstance == nullptr)
    {
        DBG ("Minimal args failed, trying with no arguments...");
        vlcInstance = libvlc_new (0, nullptr);
    }
    
    if (vlcInstance == nullptr)
    {
        DBG ("Failed to initialize libVLC instance - all methods failed");
        DBG ("Current working directory: " + juce::File::getCurrentWorkingDirectory().getFullPathName());
        DBG ("VLC_PLUGIN_PATH env var: " + juce::String (getenv("VLC_PLUGIN_PATH") ? getenv("VLC_PLUGIN_PATH") : "not set"));
        
        // Try to get last libVLC error
        const char* vlcError = libvlc_errmsg();
        if (vlcError != nullptr)
        {
            DBG ("libVLC error: " + juce::String(vlcError));
        }
        
        return;
    }
    
    DBG ("libVLC instance created successfully!");
    
    // Get libVLC version info
    const char* version = libvlc_get_version();
    if (version != nullptr)
    {
        DBG ("libVLC version: " + juce::String (version));
    }
    
    // Log available video outputs
    libvlc_module_description_t* vouts = libvlc_video_filter_list_get(vlcInstance);
    if (vouts != nullptr)
    {
        DBG ("Available video filters detected");
        libvlc_module_description_list_release(vouts);
    }
    
    mediaPlayer = libvlc_media_player_new (vlcInstance);
    if (mediaPlayer == nullptr)
    {
        DBG ("Failed to create libVLC media player");
        const char* vlcError = libvlc_errmsg();
        if (vlcError != nullptr)
        {
            DBG ("libVLC error: " + juce::String(vlcError));
        }
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
    DBG("VLCMediaPlayer::open - Starting media parsing");
    libvlc_media_parse (currentMedia);
    
    // Wait a bit for parsing to complete, then update media information
    // Note: libvlc_media_parse is asynchronous, so we need to wait
    juce::Thread::sleep(50);  // Give VLC some time to start parsing
    
    // Update media information
    updateMediaInfo();
    
    DBG("VLCMediaPlayer::open - Media parsing and info update completed");
    
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
    if (data == nullptr || planes == nullptr)
        return nullptr;
        
    auto* player = static_cast<VLCMediaPlayer*>(data);
    if (player == nullptr)
        return nullptr;
    
    // Return pointer to our video frame buffer
    if (player->videoFrameBuffer != nullptr)
    {
        *planes = player->videoFrameBuffer.get();
        return player->videoFrameBuffer.get();
    }
    
    return nullptr;
}

void VLCMediaPlayer::videoUnlockCallback (void* data, void* picture, void* const* planes)
{
    if (data == nullptr || picture == nullptr || planes == nullptr)
        return;
        
    auto* player = static_cast<VLCMediaPlayer*>(data);
    if (player == nullptr)
        return;
    
    // Video frame data is now available in the buffer
    // We'll process it in the display callback
}

void VLCMediaPlayer::videoDisplayCallback (void* data, void* picture)
{
    if (data == nullptr || picture == nullptr)
        return;
        
    auto* player = static_cast<VLCMediaPlayer*>(data);
    if (player == nullptr)
        return;
    
    // Track frame count for debugging (only log occasionally)
    static int frameCount = 0;
    frameCount++;
    if (frameCount == 1 || frameCount % 100 == 0)
    {
        DBG("VLCMediaPlayer::videoDisplayCallback - Frame " + juce::String(frameCount) + 
            " received, size: " + juce::String(player->videoWidth.load()) + "x" + 
            juce::String(player->videoHeight.load()));
    }
    
    // Convert the video frame buffer to JUCE Image
    player->updateVideoFrameFromBuffer();
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
    
    DBG("VLCMediaPlayer::videoFormatCallback - Setting up video format: " + 
        juce::String(*width) + "x" + juce::String(*height));
    
    // Set format to RGBA
    memcpy (chroma, "RV32", 4);
    
    player->updateVideoSize (*width, *height);
    
    *pitches = *width * 4; // 4 bytes per pixel for RGBA
    *lines = *height;
    
    // Allocate video frame buffer
    size_t bufferSize = (*width) * (*height) * 4; // RGBA = 4 bytes per pixel
    player->videoFrameBufferSize = bufferSize;
    player->videoFrameBuffer = std::make_unique<uint8_t[]>(bufferSize);
    
    DBG("VLCMediaPlayer::videoFormatCallback - Allocated video buffer: " + juce::String(bufferSize) + " bytes");
    
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
    
    DBG("VLCMediaPlayer::setupVideoCallbacks - Setting up video callbacks for frame capture");
    
    // Enable video callbacks for frame capture
    libvlc_video_set_callbacks (mediaPlayer,
                               videoLockCallback,
                               videoUnlockCallback,
                               videoDisplayCallback,
                               this);
    
    libvlc_video_set_format_callbacks (mediaPlayer,
                                      videoFormatCallback,
                                      videoCleanupCallback);
}

void VLCMediaPlayer::setupVideoOutput()
{
    if (mediaPlayer == nullptr)
        return;
    
    // We use video memory callbacks (vmem) to capture frames into JUCE images,
    // NOT native window rendering. The callbacks are set up in setupVideoCallbacks().
    // Do NOT set native window handles (nsobject/hwnd/xwindow) as they conflict
    // with the callback-based approach.
    
    DBG ("Video output configured for memory callbacks (vmem)");
    DBG ("  Video component: " + (videoComponent != nullptr ? 
        juce::String::toHexString ((juce::pointer_sized_int)videoComponent.getComponent()) : "none"));
    
    // Note: If you want to render directly to a native window instead of using callbacks,
    // uncomment the native window code below and remove setupVideoCallbacks() from open().
    // The two approaches are mutually exclusive.
    
    /*
    // Native window rendering (alternative to callbacks):
    if (videoComponent != nullptr)
    {
        #if JUCE_WINDOWS
            auto windowHandle = videoComponent->getWindowHandle();
            libvlc_media_player_set_hwnd (mediaPlayer, windowHandle);
        #elif JUCE_MAC
            auto windowHandle = videoComponent->getWindowHandle();
            libvlc_media_player_set_nsobject (mediaPlayer, windowHandle);
        #elif JUCE_LINUX
            auto windowHandle = videoComponent->getWindowHandle();
            libvlc_media_player_set_xwindow (mediaPlayer, (uint32_t)(juce::pointer_sized_int)windowHandle);
        #endif
    }
    */
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
    {
        DBG("VLCMediaPlayer::updateMediaInfo - No current media");
        return;
    }
    
    DBG("VLCMediaPlayer::updateMediaInfo - Updating media information");
    
    // Get duration
    int64_t durationMs = libvlc_media_get_duration (currentMedia);
    DBG("VLCMediaPlayer::updateMediaInfo - Duration from libVLC: " + juce::String(durationMs) + " ms");
    
    if (durationMs > 0)
    {
        mediaDuration = static_cast<double>(durationMs) / 1000.0;
        DBG("VLCMediaPlayer::updateMediaInfo - Set mediaDuration to: " + juce::String(mediaDuration.load()) + " seconds");
        
        double sampleRate = currentSampleRate.load();
        if (sampleRate > 0.0)
        {
            totalAudioSamples = static_cast<int64_t>(mediaDuration.load() * sampleRate);
            DBG("VLCMediaPlayer::updateMediaInfo - Calculated totalAudioSamples: " + juce::String(totalAudioSamples.load()));
        }
    }
    else if (durationMs == 0)
    {
        DBG("VLCMediaPlayer::updateMediaInfo - Duration is 0, might be a live stream or unknown duration");
        mediaDuration = -1.0;  // Keep as unknown
    }
    else
    {
        DBG("VLCMediaPlayer::updateMediaInfo - Duration is negative (" + juce::String(durationMs) + "), media not yet parsed");
        mediaDuration = -1.0;  // Keep as unknown until parsing completes
    }
    
    // Check for audio/video tracks using media tracks (not player tracks)
    // Note: libvlc_audio_get_track_count and libvlc_video_get_track_count only work after playback starts
    // We need to use libvlc_media_tracks_get to get track info from parsed media
    if (currentMedia != nullptr)
    {
        libvlc_media_track_t** tracks = nullptr;
        unsigned int trackCount = libvlc_media_tracks_get(currentMedia, &tracks);
        
        DBG("VLCMediaPlayer::updateMediaInfo - Total track count from media: " + juce::String(trackCount));
        
        int audioTrackCount = 0;
        int videoTrackCount = 0;
        
        for (unsigned int i = 0; i < trackCount; ++i)
        {
            if (tracks[i] != nullptr)
            {
                switch (tracks[i]->i_type)
                {
                    case libvlc_track_audio:
                        audioTrackCount++;
                        DBG("VLCMediaPlayer::updateMediaInfo - Found audio track " + juce::String(i));
                        break;
                    case libvlc_track_video:
                        videoTrackCount++;
                        DBG("VLCMediaPlayer::updateMediaInfo - Found video track " + juce::String(i) + 
                            " - " + juce::String(tracks[i]->video->i_width) + "x" + juce::String(tracks[i]->video->i_height));
                        
                        // Store video dimensions
                        if (tracks[i]->video != nullptr)
                        {
                            videoWidth = tracks[i]->video->i_width;
                            videoHeight = tracks[i]->video->i_height;
                        }
                        break;
                    case libvlc_track_text:
                        DBG("VLCMediaPlayer::updateMediaInfo - Found subtitle track " + juce::String(i));
                        break;
                    default:
                        DBG("VLCMediaPlayer::updateMediaInfo - Found unknown track type " + juce::String(tracks[i]->i_type));
                        break;
                }
            }
        }
        
        // Release track info
        if (tracks != nullptr)
        {
            libvlc_media_tracks_release(tracks, trackCount);
        }
        
        DBG("VLCMediaPlayer::updateMediaInfo - Audio track count: " + juce::String(audioTrackCount));
        DBG("VLCMediaPlayer::updateMediaInfo - Video track count: " + juce::String(videoTrackCount));
        
        hasAudioStream = (audioTrackCount > 0);
        hasVideoStream = (videoTrackCount > 0);
        
        DBG("VLCMediaPlayer::updateMediaInfo - hasAudioStream: " + juce::String(hasAudioStream.load() ? "true" : "false"));
        DBG("VLCMediaPlayer::updateMediaInfo - hasVideoStream: " + juce::String(hasVideoStream.load() ? "true" : "false"));
    }
    else
    {
        DBG("VLCMediaPlayer::updateMediaInfo - No current media available");
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

void VLCMediaPlayer::updateVideoFrameFromBuffer()
{
    if (videoFrameBuffer == nullptr || videoFrameBufferSize == 0)
        return;
    
    int width = videoWidth.load();
    int height = videoHeight.load();
    
    if (width <= 0 || height <= 0)
        return;
    
    std::lock_guard<std::mutex> lock(videoFrameMutex);
    
    // Create or recreate the JUCE Image if dimensions changed
    if (!currentVideoFrame.isValid() || 
        currentVideoFrame.getWidth() != width || 
        currentVideoFrame.getHeight() != height)
    {
        currentVideoFrame = juce::Image(juce::Image::ARGB, width, height, true);
        DBG("VLCMediaPlayer::updateVideoFrameFromBuffer - Created new video frame: " + 
            juce::String(width) + "x" + juce::String(height));
    }
    
    // Copy video data from VLC buffer to JUCE Image
    juce::Image::BitmapData bitmapData(currentVideoFrame, juce::Image::BitmapData::writeOnly);
    
    // VLC provides RGBA data, JUCE expects ARGB
    // We need to convert RGBA -> ARGB
    const uint8_t* srcData = videoFrameBuffer.get();
    uint8_t* destData = bitmapData.data;
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int srcIndex = (y * width + x) * 4;
            int destIndex = y * bitmapData.lineStride + x * bitmapData.pixelStride;
            
            // Convert RGBA to ARGB
            uint8_t r = srcData[srcIndex + 0];
            uint8_t g = srcData[srcIndex + 1];
            uint8_t b = srcData[srcIndex + 2];
            uint8_t a = srcData[srcIndex + 3];
            
            destData[destIndex + 0] = b; // Blue
            destData[destIndex + 1] = g; // Green  
            destData[destIndex + 2] = r; // Red
            destData[destIndex + 3] = a; // Alpha
        }
    }
}

juce::Image VLCMediaPlayer::getCurrentVideoFrame() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(videoFrameMutex));
    return currentVideoFrame;
}

} // namespace juce
