#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MIX_Mixer; // forward declarations: SDL3_mixer only appears in the .cpp
struct MIX_Audio;
struct MIX_Track;

// The engine's AUDIO, on top of SDL3_mixer: sound effects that overlap and background
// music. Like the AssetManager, it OWNS everything it loads (each file is read only
// once and reused).
//
// Formats: WAV, OGG Vorbis and Opus (the post-build step copies the Opus DLLs).
//
// If there is no audio device (a machine without speakers, a remote session), init()
// returns false and everything else does nothing: the game goes on, muted.
//
// From a game system (although the usual way is the SoundEffects component):
//     app.audio().playSound("assets/audio/sfx/jump.wav", 0.8f);
class Audio {
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();
    bool ready() const { return mixer != nullptr; }

    // An effect: it plays in full and mixes with whatever is already playing (up to
    // MAX_VOICES at once; beyond that it cuts the oldest). volume: 0 = muted, 1 = as recorded.
    void playSound(const std::string& path, float volume = 1.0f);

    // The music: only one at a time, with a short fade. Requesting the one already
    // requested only changes its volume (it does not restart it; without loop, it does not repeat it either).
    void playMusic(const std::string& path, float volume = 1.0f, bool loop = true);
    void stopMusic();

    void stopAll();               // when the game stops
    void setPaused(bool paused);  // the editor's pause also freezes the sound

private:
    static const int MAX_VOICES = 16;
    MIX_Audio* load(const std::string& path);

    MIX_Mixer* mixer = nullptr;
    bool subsystemReady = false;
    std::unordered_map<std::string, MIX_Audio*> sounds; // owned
    std::unordered_set<std::string> failed;             // paths that failed to load
    std::vector<MIX_Track*> voices;                     // tracks for the effects
    int nextVoice = 0;                                  // the one that gets cut if all are busy
    MIX_Track* musicTrack = nullptr;
    std::string musicPath;                              // the one playing ("" = none)
};
