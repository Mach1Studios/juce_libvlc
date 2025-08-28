/*
  ==============================================================================

   Example: Simple Video Player using juce_libvlc module
   
   This example demonstrates how to use the VLCMediaPlayer class to create
   a basic video player with seeking capabilities.

  ==============================================================================
*/

#pragma once

#include "../juce_media/ISeekableMedia.h"
#include "../juce_media/VLCMediaPlayer.h"

class VideoPlayerExample : public juce::Component,
                          public juce::ISeekableMedia::Listener,
                          public juce::Button::Listener,
                          public juce::Slider::Listener,
                          public juce::Timer,
                          public juce::FileDragAndDropTarget
{
public:
    VideoPlayerExample()
    {
        // Create media player
        mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();
        mediaPlayer->addListener (this);
        
        // Create UI components
        setupUI();
        
        // Set up audio device manager
        audioDeviceManager.initialise (0, 2, nullptr, true);
        mediaPlayer->setAudioDevice (&audioDeviceManager);
        
        setSize (800, 600);
    }
    
    ~VideoPlayerExample() override
    {
        // Stop seek simulator if enabled
        /*
        // Uncomment this line if seek simulation is enabled
        seekSimulator = nullptr;
        */
        
        mediaPlayer->removeListener (this);
        mediaPlayer->close();
        audioDeviceManager.removeAudioCallback (mediaPlayer.get());
    }
    
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
        
        if (!mediaPlayer->hasVideo())
        {
            g.setColour (juce::Colours::white);
            g.setFont (20.0f);
            g.drawText ("No video loaded", getLocalBounds(), juce::Justification::centred);
            
            // Show drag and drop hint
            g.setFont (14.0f);
            g.setColour (juce::Colours::lightgrey);
            auto bounds = getLocalBounds();
            bounds.removeFromTop (bounds.getHeight() / 2 + 20);
            g.drawText ("Drag and drop a video file here or use the Open button", 
                       bounds.removeFromTop (30), juce::Justification::centred);
        }
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        
        // Video area takes most of the space
        auto videoArea = bounds.removeFromTop (bounds.getHeight() - 100);
        videoComponent.setBounds (videoArea);
        
        // Controls at the bottom
        auto controlsArea = bounds.reduced (10);
        
        auto buttonArea = controlsArea.removeFromTop (30);
        openButton.setBounds (buttonArea.removeFromLeft (80));
        buttonArea.removeFromLeft (10);
        playButton.setBounds (buttonArea.removeFromLeft (80));
        buttonArea.removeFromLeft (10);
        pauseButton.setBounds (buttonArea.removeFromLeft (80));
        buttonArea.removeFromLeft (10);
        stopButton.setBounds (buttonArea.removeFromLeft (80));
        
        controlsArea.removeFromTop (10);
        positionSlider.setBounds (controlsArea.removeFromTop (30));
        
        controlsArea.removeFromTop (10);
        auto infoArea = controlsArea;
        timeLabel.setBounds (infoArea.removeFromLeft (200));
        infoArea.removeFromLeft (10);
        statusLabel.setBounds (infoArea);
    }
    
private:
    void setupUI()
    {
        // Video component
        addAndMakeVisible (videoComponent);
        
        // Buttons
        addAndMakeVisible (openButton);
        openButton.setButtonText ("Open");
        openButton.addListener (this);
        
        addAndMakeVisible (playButton);
        playButton.setButtonText ("Play");
        playButton.addListener (this);
        
        addAndMakeVisible (pauseButton);
        pauseButton.setButtonText ("Pause");
        pauseButton.addListener (this);
        
        addAndMakeVisible (stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.addListener (this);
        
        // Position slider
        addAndMakeVisible (positionSlider);
        positionSlider.setRange (0.0, 1.0);
        positionSlider.addListener (this);
        positionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        
        // Labels
        addAndMakeVisible (timeLabel);
        timeLabel.setText ("00:00 / 00:00", juce::dontSendNotification);
        
        addAndMakeVisible (statusLabel);
        statusLabel.setText ("No media loaded", juce::dontSendNotification);
        
        // Set video component for media player
        mediaPlayer->setVideoComponent (&videoComponent);
        
        // Start timer for UI updates
        startTimer (100); // 10 FPS for UI updates
        
        // Initialize seek simulator (commented out by default)
        /*
        // Uncomment these lines to enable seek simulation
        seekSimulator = std::make_unique<SeekSimulator> (this);
        seekSimulator->startTimer (2000); // Seek every 2 seconds
        */
    }
    
    // Button::Listener
    void buttonClicked (juce::Button* button) override
    {
        if (button == &openButton)
        {
            openFile();
        }
        else if (button == &playButton)
        {
            mediaPlayer->play();
        }
        else if (button == &pauseButton)
        {
            mediaPlayer->pause();
        }
        else if (button == &stopButton)
        {
            mediaPlayer->stop();
        }
    }
    
    // Slider::Listener
    void sliderValueChanged (juce::Slider* slider) override
    {
        if (slider == &positionSlider && !updatingSlider)
        {
            double totalDuration = mediaPlayer->getTotalDuration();
            if (totalDuration > 0.0)
            {
                double targetTime = slider->getValue() * totalDuration;
                mediaPlayer->seekToTime (targetTime, juce::ISeekableMedia::SeekMode::Fast);
            }
        }
    }
    
    // ISeekableMedia::Listener
    void mediaReady (juce::ISeekableMedia* media) override
    {
        juce::MessageManager::callAsync ([this]()
        {
            statusLabel.setText ("Media ready", juce::dontSendNotification);
            updateTimeDisplay();
        });
    }
    
    void mediaError (juce::ISeekableMedia* media, const juce::String& error) override
    {
        juce::MessageManager::callAsync ([this, error]()
        {
            statusLabel.setText ("Error: " + error, juce::dontSendNotification);
        });
    }
    
    void mediaFinished (juce::ISeekableMedia* media) override
    {
        juce::MessageManager::callAsync ([this]()
        {
            statusLabel.setText ("Playback finished", juce::dontSendNotification);
        });
    }
    
    void seekCompleted (juce::ISeekableMedia* media, int64_t newSamplePosition) override
    {
        juce::MessageManager::callAsync ([this]()
        {
            updateTimeDisplay();
        });
    }
    
    // Timer callback for UI updates
    void timerCallback() override
    {
        updateTimeDisplay();
        updatePositionSlider();
    }
    
    // FileDragAndDropTarget interface
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        // Check if any of the dragged files are video files
        for (const auto& file : files)
        {
            juce::File f (file);
            juce::String extension = f.getFileExtension().toLowerCase();
            
            if (extension == ".mp4" || extension == ".avi" || extension == ".mov" ||
                extension == ".mkv" || extension == ".wmv" || extension == ".flv" ||
                extension == ".webm" || extension == ".m4v" || extension == ".3gp" ||
                extension == ".ogv" || extension == ".ts" || extension == ".mts")
            {
                return true;
            }
        }
        return false;
    }
    
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        juce::ignoreUnused (x, y);
        
        DBG ("Files dropped: " + juce::String (files.size()) + " files");
        
        // Load the first valid video file
        for (const auto& filePath : files)
        {
            juce::File file (filePath);
            DBG ("Checking file: " + filePath);
            
            if (file.existsAsFile() && isVideoFile (file))
            {
                DBG ("Loading video file: " + filePath);
                loadVideoFile (file);
                break; // Only load the first valid file
            }
            else
            {
                DBG ("File rejected - exists: " + juce::String (file.existsAsFile() ? "true" : "false") + 
                     ", isVideo: " + juce::String (isVideoFile (file) ? "true" : "false"));
            }
        }
    }
    
    void fileDragEnter (const juce::StringArray& files, int x, int y) override
    {
        juce::ignoreUnused (files, x, y);
        // Visual feedback when files are dragged over
        statusLabel.setText ("Drop video file to load...", juce::dontSendNotification);
        repaint();
    }
    
    void fileDragExit (const juce::StringArray& files) override
    {
        juce::ignoreUnused (files);
        // Restore original status when drag leaves
        if (!mediaPlayer->isPlaying())
        {
            statusLabel.setText ("No media loaded", juce::dontSendNotification);
        }
        repaint();
    }
    
    void openFile()
    {
        // Create a shared FileChooser to keep it alive during async operation
        auto chooser = std::make_shared<juce::FileChooser> ("Select a video file...",
                                                           juce::File(),
                                                           "*.mp4;*.avi;*.mov;*.mkv;*.wmv;*.flv;*.webm");
        
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                loadVideoFile (file);
            }
        });
    }
    
    void updateTimeDisplay()
    {
        double currentTime = mediaPlayer->getCurrentTime();
        double totalTime = mediaPlayer->getTotalDuration();
        
        auto formatTime = [](double seconds) -> juce::String
        {
            int mins = static_cast<int>(seconds) / 60;
            int secs = static_cast<int>(seconds) % 60;
            return juce::String::formatted ("%02d:%02d", mins, secs);
        };
        
        juce::String timeText = formatTime (currentTime) + " / " + formatTime (totalTime);
        timeLabel.setText (timeText, juce::dontSendNotification);
    }
    
    void updatePositionSlider()
    {
        double totalDuration = mediaPlayer->getTotalDuration();
        if (totalDuration > 0.0)
        {
            double currentTime = mediaPlayer->getCurrentTime();
            double position = currentTime / totalDuration;
            
            updatingSlider = true;
            positionSlider.setValue (position, juce::dontSendNotification);
            updatingSlider = false;
        }
    }
    
    bool isVideoFile (const juce::File& file)
    {
        juce::String extension = file.getFileExtension().toLowerCase();
        return extension == ".mp4" || extension == ".avi" || extension == ".mov" ||
               extension == ".mkv" || extension == ".wmv" || extension == ".flv" ||
               extension == ".webm" || extension == ".m4v" || extension == ".3gp" ||
               extension == ".ogv" || extension == ".ts" || extension == ".mts";
    }
    
    void loadVideoFile (const juce::File& file)
    {
        DBG ("loadVideoFile called with: " + file.getFullPathName());
        
        juce::String error;
        
        if (mediaPlayer->open (file, &error))
        {
            DBG ("Successfully opened file: " + file.getFileName());
            statusLabel.setText ("Loaded: " + file.getFileName(), juce::dontSendNotification);
        }
        else
        {
            DBG ("Failed to open file: " + error);
            statusLabel.setText ("Failed to load: " + error, juce::dontSendNotification);
        }
    }
    
    // Components
    juce::Component videoComponent;
    juce::TextButton openButton, playButton, pauseButton, stopButton;
    juce::Slider positionSlider;
    juce::Label timeLabel, statusLabel;
    
    // Media player and audio
    std::unique_ptr<juce::VLCMediaPlayer> mediaPlayer;
    juce::AudioDeviceManager audioDeviceManager;
    
    // State
    bool updatingSlider = false;
    
    // Seek simulation
    // This will seek to random positions every 2 seconds for testing
    class SeekSimulator : public juce::Timer
    {
    public:
        SeekSimulator (VideoPlayerExample* owner) : parent (owner) {}
        
        void timerCallback() override
        {
            if (parent->mediaPlayer != nullptr && parent->mediaPlayer->isPlaying())
            {
                double totalDuration = parent->mediaPlayer->getTotalDuration();
                if (totalDuration > 0.0)
                {
                    // Generate random seek position (0% to 90% of total duration)
                    juce::Random random;
                    double randomPosition = random.nextDouble() * 0.9 * totalDuration;
                    
                    DBG ("Seek simulation: seeking to " + juce::String (randomPosition, 2) + " seconds");
                    parent->mediaPlayer->seekToTime (randomPosition, juce::ISeekableMedia::SeekMode::Fast);
                }
            }
        }
        
    private:
        VideoPlayerExample* parent;
    };
    
    std::unique_ptr<SeekSimulator> seekSimulator;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoPlayerExample)
};
