/*
  ==============================================================================


  ==============================================================================
*/

#pragma once

#include "ISeekableMedia.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <memory>
#include <mutex>

// Forward declarations for libVLC types
struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_media_t;

namespace juce
{

/**
 * Implementation of ISeekableMedia using libVLC for video and audio playback.
 * This class provides precise seeking capabilities and integrates with JUCE's
 * audio and video systems.
 */
class VLCMediaPlayer : public ISeekableMedia,
                       public AudioIODeviceCallback,
                       public Timer
{
public:
    //==============================================================================
    VLCMediaPlayer();
    ~VLCMediaPlayer() override;

    //==============================================================================
    // ISeekableMedia implementation
    bool open(const File& media, String* error = nullptr) override;
    void close() override;
    
    void play() override;
    void pause() override;
    void stop() override;
    bool isPlaying() const override;
    
    bool seekToSample (int64_t sampleIndex, SeekMode mode = SeekMode::Precise) override;
    bool seekToTime (double timeInSeconds, SeekMode mode = SeekMode::Precise) override;
    
    void setVideoComponent (Component* component) override;
    void setAudioDevice (AudioDeviceManager* deviceManager) override;
    
    int getSampleRate() const override;
    int64_t getTotalSamples() const override;
    int64_t getCurrentSample() const override;
    double getTotalDuration() const override;
    double getCurrentTime() const override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    Rectangle<int> getVideoSize() const override;
    
    // Video frame access
    juce::Image getCurrentVideoFrame() const;
    
    void addListener (Listener* listener) override;
    void removeListener (Listener* listener) override;

    //==============================================================================
    // AudioIODeviceCallback implementation
    void audioDeviceIOCallback (const float** inputChannelData,
                               int numInputChannels,
                               float** outputChannelData,
                               int numOutputChannels,
                               int numSamples);
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const String& errorMessage) override;

    //==============================================================================
    // Timer implementation (for position updates)
    void timerCallback() override;

private:
    //==============================================================================
    struct AudioBuffer
    {
        AudioBuffer (int channels, int samples);
        ~AudioBuffer();
        
        void resize (int channels, int samples);
        void clear();
        
        float** data = nullptr;
        int numChannels = 0;
        int numSamples = 0;
        std::atomic<int> writePosition { 0 };
        std::atomic<int> readPosition { 0 };
        std::atomic<int> availableSamples { 0 };
    };

    //==============================================================================
    // libVLC instance and player
    libvlc_instance_t* vlcInstance = nullptr;
    libvlc_media_player_t* mediaPlayer = nullptr;
    libvlc_media_t* currentMedia = nullptr;
    
    // Audio system integration
    AudioDeviceManager* audioDeviceManager = nullptr;
    std::unique_ptr<AudioBuffer> audioRingBuffer;
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> audioChannels { 2 };
    std::atomic<int64_t> totalAudioSamples { -1 };
    std::atomic<int64_t> currentAudioSample { 0 };
    
    // Video system integration
    Component::SafePointer<Component> videoComponent;
    std::atomic<int> videoWidth { 0 };
    std::atomic<int> videoHeight { 0 };
    std::atomic<bool> hasVideoStream { false };
    std::atomic<bool> hasAudioStream { false };
    
    // Video frame capture
    juce::Image currentVideoFrame;
    std::mutex videoFrameMutex;
    std::unique_ptr<uint8_t[]> videoFrameBuffer;
    size_t videoFrameBufferSize { 0 };
    
    // Playback state
    std::atomic<bool> isCurrentlyPlaying { false };
    std::atomic<double> mediaDuration { -1.0 };
    std::atomic<int64_t> seekGeneration { 0 };
    
    // Thread safety
    mutable std::mutex stateMutex;
    ListenerList<Listener> listeners;
    
    //==============================================================================
    // libVLC callback functions
    static void* audioLockCallback (void* data, void** pcm_buffer, size_t size);
    static void audioUnlockCallback (void* data, void* pcm_buffer, size_t size);
    static void audioPlayCallback (void* data, void* pcm_buffer, size_t size);
    static void audioPauseCallback (void* data, int64_t pts);
    static void audioResumeCallback (void* data, int64_t pts);
    static void audioFlushCallback (void* data, int64_t pts);
    static void audioDrainCallback (void* data);
    
    static void* videoLockCallback (void* data, void** planes);
    static void videoUnlockCallback (void* data, void* picture, void* const* planes);
    static void videoDisplayCallback (void* data, void* picture);
    static unsigned videoFormatCallback (void** data, char* chroma, unsigned* width, 
                                       unsigned* height, unsigned* pitches, unsigned* lines);
    static void videoCleanupCallback (void* data);
    
    // Event callbacks removed for compatibility
    
    //==============================================================================
    // Internal methods
    void initializeVLC();
    void shutdownVLC();
    void setupAudioCallbacks();
    void setupVideoCallbacks();
    void setupEventHandling();
    void updateMediaInfo();
    void notifyListeners (std::function<void(Listener*)> callback);
    
    // Audio processing
    void processAudioData (void* buffer, size_t size);
    int getAvailableAudioSamples() const;
    void updateAudioPosition();
    
    // Video processing
    void setupVideoOutput();
    void updateVideoSize (int width, int height);
    void updateVideoFrameFromBuffer();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VLCMediaPlayer)
};

} // namespace juce
