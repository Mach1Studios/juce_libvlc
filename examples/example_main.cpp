/*
  ==============================================================================

   Example application using juce_libvlc module
   This creates a simple video player application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VideoPlayerExample.h"

class VideoPlayerApplication : public juce::JUCEApplication
{
public:
    VideoPlayerApplication() = default;

    const juce::String getApplicationName() override       { return "Video Player Example"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override
    {
        ignoreUnused (commandLine);
        
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        ignoreUnused (commandLine);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                        .findColour (juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new VideoPlayerExample(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (VideoPlayerApplication)
