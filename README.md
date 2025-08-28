# juce_libvlc

A JUCE module that provides libVLC-based media playback capabilities with precise seeking for video and audio content. This module is specifically designed for applications that need accurate timing control.

## Requirements

- **JUCE Framework**: Version 7.0 or later
- **libVLC**: Version 3.0 or later (4.0+ recommended for best seeking performance)
- **C++17**: Minimum C++ standard
- **CMake**: Version 3.15 or later (for building)

## Installation

### 1. Install libVLC

#### macOS
```bash
# Using Homebrew
brew install libvlc

# Or download VLC app from videolan.org
# The module will automatically find VLC.app installation
```

#### Windows
```bash
# Download VLC from videolan.org and install
# Or use vcpkg
vcpkg install libvlc
```

#### Linux
```bash
# Ubuntu/Debian
sudo apt-get install libvlc-dev

# Fedora/CentOS
sudo dnf install libvlc-devel
```

### 2. Add to Your JUCE Project

#### Method 1: Copy Module to Your Project (Recommended)
```bash
# Copy the entire juce_libvlc folder to your project's modules directory
cp -r juce_libvlc /path/to/your/project/modules/
```

Then in your CMakeLists.txt:
```cmake
# Add JUCE modules directory
juce_add_module(modules/juce_libvlc)

# Link to your target
target_link_libraries(YourTarget PRIVATE 
    juce_libvlc
    # ... other libraries
)
```

#### Method 2: Using Projucer
1. Copy the `juce_libvlc` folder to your project's modules directory
2. In Projucer, go to "Modules" and click "Add a module from a specified folder"
3. Select the `juce_libvlc` folder
4. Enable the module for your target platforms
5. Ensure libVLC is available in your system's library path

#### Method 3: As Git Submodule
```bash
# Add as submodule
git submodule add https://github.com/yourusername/juce_libvlc.git modules/juce_libvlc

# In your CMakeLists.txt
juce_add_module(modules/juce_libvlc)
target_link_libraries(YourTarget PRIVATE juce_libvlc)
```

## Usage

### Basic Video Player

```cpp
#include <juce_libvlc/juce_libvlc.h>

class MyVideoPlayer : public juce::Component,
                      public juce::ISeekableMedia::Listener
{
public:
    MyVideoPlayer()
    {
        // Create media player
        mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();
        mediaPlayer->addListener(this);
        
        // Set up audio and video output
        mediaPlayer->setVideoComponent(&videoComponent);
        mediaPlayer->setAudioDevice(&audioDeviceManager);
        
        addAndMakeVisible(videoComponent);
    }
    
    void loadAndPlay(const juce::File& videoFile)
    {
        juce::String error;
        if (mediaPlayer->open(videoFile, &error))
        {
            mediaPlayer->play();
        }
        else
        {
            DBG("Failed to load video: " + error);
        }
    }
    
    // Seek to specific sample (for DAW sync)
    void seekToSample(int64_t sampleIndex)
    {
        mediaPlayer->seekToSample(sampleIndex, 
                                 juce::ISeekableMedia::SeekMode::Precise);
    }
    
    // ISeekableMedia::Listener callbacks
    void mediaReady(juce::ISeekableMedia* media) override
    {
        DBG("Media ready for playback");
    }
    
    void mediaError(juce::ISeekableMedia* media, const juce::String& error) override
    {
        DBG("Media error: " + error);
    }

private:
    std::unique_ptr<juce::VLCMediaPlayer> mediaPlayer;
    juce::Component videoComponent;
    juce::AudioDeviceManager audioDeviceManager;
};
```

### DAW Synchronization Example

```cpp
class DAWSyncVideoPlayer : public juce::ISeekableMedia::Listener
{
public:
    DAWSyncVideoPlayer()
    {
        mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();
        mediaPlayer->addListener(this);
    }
    
    // Called when DAW playhead position changes
    void onPlayheadPositionChanged(double timeInSeconds)
    {
        if (mediaPlayer->hasAudio())
        {
            // Convert time to sample index
            int sampleRate = mediaPlayer->getSampleRate();
            int64_t sampleIndex = static_cast<int64_t>(timeInSeconds * sampleRate);
            
            // Seek to precise position
            mediaPlayer->seekToSample(sampleIndex, 
                                     juce::ISeekableMedia::SeekMode::Precise);
        }
    }
    
    void onDAWPlay()
    {
        mediaPlayer->play();
    }
    
    void onDAWStop()
    {
        mediaPlayer->pause();
    }

private:
    std::unique_ptr<juce::VLCMediaPlayer> mediaPlayer;
};
```

## API Reference

### ISeekableMedia Interface

The main interface for media playback control:

#### Playback Control
- `bool open(const File& media, String* error = nullptr)`
- `void close()`
- `void play()`
- `void pause()`
- `void stop()`
- `bool isPlaying() const`

#### Seeking
- `bool seekToSample(int64_t sampleIndex, SeekMode mode = SeekMode::Precise)`
- `bool seekToTime(double timeInSeconds, SeekMode mode = SeekMode::Precise)`

#### Output Setup
- `void setVideoComponent(Component* component)`
- `void setAudioDevice(AudioDeviceManager* deviceManager)`

#### Information
- `int getSampleRate() const`
- `int64_t getTotalSamples() const`
- `int64_t getCurrentSample() const`
- `double getTotalDuration() const`
- `double getCurrentTime() const`
- `bool hasVideo() const`
- `bool hasAudio() const`
- `Rectangle<int> getVideoSize() const`

### VLCMediaPlayer Class

Concrete implementation of `ISeekableMedia` using libVLC:

```cpp
auto mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();

// Configure before opening media
mediaPlayer->setVideoComponent(&myVideoComponent);
mediaPlayer->setAudioDevice(&myAudioDeviceManager);

// Open and control media
if (mediaPlayer->open(videoFile))
{
    mediaPlayer->play();
    
    // Seek to 30 seconds with precise mode
    mediaPlayer->seekToTime(30.0, juce::ISeekableMedia::SeekMode::Precise);
}
```

## Seeking Modes

The module supports two seeking modes:

- **`SeekMode::Fast`**: Prioritizes speed over accuracy. May seek to the nearest keyframe.
- **`SeekMode::Precise`**: Prioritizes accuracy over speed. Attempts frame-accurate seeking.

Note: Even in precise mode, some video formats may not support exact frame-level seeking due to codec limitations.

## Threading Considerations

- All libVLC operations run on background threads
- JUCE's message thread handles UI updates and painting
- Audio callbacks are processed on the audio thread
- Use `MessageManager::callAsync()` for UI updates from callbacks

## Supported Formats

The module supports all formats that libVLC can handle, including:

- **Video**: MP4, AVI, MOV, MKV, WMV, FLV, WebM, and many others
- **Audio**: MP3, WAV, FLAC, OGG, AAC, and many others
- **Streaming**: HTTP, RTSP, and other network protocols

## Building

### CMake Build

```bash
mkdir build
cd build
cmake ..
make
```

### CMake Options

- `JUCE_LIBVLC_BUILD_EXAMPLES`: Build example applications (default: OFF)

## Examples

See the `examples/` directory for complete usage examples:

- `VideoPlayerExample.h`: Basic video player with UI controls
- More examples coming soon...

## Troubleshooting

### libVLC Not Found

If CMake cannot find libVLC:

1. **Set VLC_DIR environment variable**:
   ```bash
   export VLC_DIR=/path/to/vlc/installation
   ```

2. **Install VLC in standard location**:
   - macOS: Install VLC.app to `/Applications/`
   - Windows: Install VLC to default Program Files location
   - Linux: Install via package manager

3. **Manual path specification**:
   ```cmake
   set(LibVLC_ROOT_DIR "/path/to/vlc")
   find_package(LibVLC REQUIRED)
   ```

### Audio Issues

- Ensure `AudioDeviceManager` is properly initialized
- Check that the audio device supports the required sample rate
- Verify that `setAudioDevice()` is called before opening media
- On some systems, you may need to call `audioDeviceManager.addAudioCallback(mediaPlayer.get())`

### Video Issues

- Ensure the video component has a valid native window handle
- Call `setVideoComponent()` before opening media
- Check that the component is added to a visible parent
- On macOS, ensure the component is properly added to a window before setting it

### Runtime Issues

**"libVLC not found" at runtime:**
- Ensure libVLC dynamic libraries are in your system PATH
- On Windows, copy VLC DLLs to your executable directory
- On macOS, ensure VLC.app is installed or libVLC is in /usr/local/lib
- On Linux, ensure libVLC packages are installed
