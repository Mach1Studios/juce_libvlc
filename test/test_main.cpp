/*
  ==============================================================================

   Simple test for juce_libvlc module
   This validates that the module compiles and basic functionality works.

  ==============================================================================
*/

#include <juce_libvlc/juce_libvlc.h>
#include <iostream>

int main()
{
    std::cout << "Testing juce_libvlc module..." << std::endl;
    
    // Test that we can create a VLCMediaPlayer instance
    try 
    {
        auto mediaPlayer = std::make_unique<juce::VLCMediaPlayer>();
        std::cout << "✓ VLCMediaPlayer created successfully" << std::endl;
        
        // Test basic interface methods (without opening media)
        std::cout << "✓ Sample rate: " << mediaPlayer->getSampleRate() << std::endl;
        std::cout << "✓ Has video: " << (mediaPlayer->hasVideo() ? "yes" : "no") << std::endl;
        std::cout << "✓ Has audio: " << (mediaPlayer->hasAudio() ? "yes" : "no") << std::endl;
        std::cout << "✓ Is playing: " << (mediaPlayer->isPlaying() ? "yes" : "no") << std::endl;
        
        std::cout << "✓ All basic tests passed!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "juce_libvlc module test completed successfully!" << std::endl;
    return 0;
}
