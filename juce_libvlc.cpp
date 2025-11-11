#include "juce_libvlc.h"

// Implementation files are included here for the JUCE module system
#include "juce_media/VLCMediaPlayer.cpp"

// VLC static module stub
// When linking statically against libVLC, it expects a vlc_static_modules array.
// Since we're using dynamic plugin loading (plugins are .dylib files), we provide
// an empty NULL-terminated array to satisfy the linker.
extern "C" {
    typedef int (*vlc_plugin_cb)(void *);
    vlc_plugin_cb vlc_static_modules[] = { nullptr };
}