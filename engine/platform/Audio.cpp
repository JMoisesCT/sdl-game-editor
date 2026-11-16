#include "platform/Audio.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>

// Music fades (ms): without them, starting or cutting in the middle of a wave sounds
// like a "click".
static const Sint64 MUSIC_FADE_IN_MS = 300;
static const Sint64 MUSIC_FADE_OUT_MS = 300;

Audio::~Audio() {
    shutdown();
}

bool Audio::init() {
    if (mixer) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio: could not start SDL audio (the game continues without sound): %s", SDL_GetError());
        return false;
    }
    subsystemReady = true;
    if (!MIX_Init()) {
        SDL_Log("Audio: could not start SDL_mixer: %s", SDL_GetError());
        shutdown();
        return false;
    }
    // The default output device, in the format it prefers.
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer) {
        SDL_Log("Audio: no audio device (the game continues without sound): %s", SDL_GetError());
        MIX_Quit();
        shutdown();
        return false;
    }
    musicTrack = MIX_CreateTrack(mixer);
    return true;
}

void Audio::shutdown() {
    // First the tracks and the sounds, then the mixer and the library.
    if (mixer) {
        MIX_StopAllTracks(mixer, 0);
        for (MIX_Track* t : voices) MIX_DestroyTrack(t);
        voices.clear();
        if (musicTrack) MIX_DestroyTrack(musicTrack);
        musicTrack = nullptr;
        for (auto& [path, audio] : sounds) MIX_DestroyAudio(audio);
        sounds.clear();
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
        MIX_Quit();
    }
    musicPath.clear();
    if (subsystemReady) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        subsystemReady = false;
    }
}

MIX_Audio* Audio::load(const std::string& path) {
    if (!mixer || path.empty()) return nullptr;
    auto it = sounds.find(path);
    if (it != sounds.end()) return it->second;
    if (failed.count(path)) return nullptr; // already failed: not retried (and does not flood the log)

    // predecode = true: decoded entirely on load. It uses more memory, but an effect
    // that plays many times is not decoded again every time. For long music, false would
    // be better (decoded while playing); here everything is short.
    MIX_Audio* audio = MIX_LoadAudio(mixer, path.c_str(), true);
    if (!audio) {
        SDL_Log("Audio: could not load '%s': %s", path.c_str(), SDL_GetError());
        failed.insert(path);
        return nullptr;
    }
    sounds[path] = audio;
    return audio;
}

void Audio::playSound(const std::string& path, float volume) {
    MIX_Audio* audio = load(path);
    if (!audio) return;

    // A free track; if there is none, another one is created; with all of them busy, the
    // one whose turn it is gets cut (almost always the oldest).
    MIX_Track* track = nullptr;
    for (MIX_Track* t : voices)
        if (!MIX_TrackPlaying(t)) { track = t; break; }
    if (!track && (int)voices.size() < MAX_VOICES) {
        track = MIX_CreateTrack(mixer);
        if (track) voices.push_back(track);
    }
    if (!track && !voices.empty()) {
        track = voices[nextVoice];
        nextVoice = (nextVoice + 1) % (int)voices.size();
    }
    if (!track) return;

    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackGain(track, std::max(volume, 0.0f));
    MIX_PlayTrack(track, 0);
}

void Audio::playMusic(const std::string& path, float volume, bool loop) {
    if (!musicTrack) return;
    MIX_SetTrackGain(musicTrack, std::max(volume, 0.0f));
    // Already requested (playing, or finished if it does not loop): only the volume.
    if (path == musicPath) return;

    MIX_Audio* audio = load(path);
    if (!audio) return;
    MIX_SetTrackAudio(musicTrack, audio);
    SDL_PropertiesID options = SDL_CreateProperties();
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0); // -1 = forever
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, MUSIC_FADE_IN_MS);
    MIX_PlayTrack(musicTrack, options);
    SDL_DestroyProperties(options);
    musicPath = path;
}

void Audio::stopMusic() {
    if (!musicTrack || musicPath.empty()) return;
    Sint64 fade = MIX_TrackMSToFrames(musicTrack, MUSIC_FADE_OUT_MS);
    MIX_StopTrack(musicTrack, fade > 0 ? fade : 0);
    musicPath.clear();
}

void Audio::stopAll() {
    if (!mixer) return;
    MIX_StopAllTracks(mixer, 0);
    musicPath.clear();
}

void Audio::setPaused(bool paused) {
    if (!mixer) return;
    if (paused) MIX_PauseAllTracks(mixer);
    else MIX_ResumeAllTracks(mixer);
}
