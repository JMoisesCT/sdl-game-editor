#include "platform/Audio.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>

// Fundidos de la musica (ms): sin ellos, empezar o cortar a mitad de una onda suena
// como un "click".
static const Sint64 MUSIC_FADE_IN_MS = 300;
static const Sint64 MUSIC_FADE_OUT_MS = 300;

Audio::~Audio() {
    shutdown();
}

bool Audio::init() {
    if (mixer) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("Audio: no se pudo iniciar el audio de SDL (el juego sigue sin sonido): %s", SDL_GetError());
        return false;
    }
    subsystemReady = true;
    if (!MIX_Init()) {
        SDL_Log("Audio: no se pudo iniciar SDL_mixer: %s", SDL_GetError());
        shutdown();
        return false;
    }
    // El dispositivo de salida por defecto, en el formato que el prefiera.
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mixer) {
        SDL_Log("Audio: no hay dispositivo de audio (el juego sigue sin sonido): %s", SDL_GetError());
        MIX_Quit();
        shutdown();
        return false;
    }
    musicTrack = MIX_CreateTrack(mixer);
    return true;
}

void Audio::shutdown() {
    // Primero las pistas y los sonidos, despues el mezclador y la libreria.
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
    if (failed.count(path)) return nullptr; // ya fallo: no se reintenta (ni se llena el log)

    // predecode = true: se decodifica entero al cargar. Gasta mas memoria, pero un
    // efecto que suena muchas veces no se vuelve a decodificar cada vez. Para la musica
    // larga convendria false (se decodifica mientras suena); aqui todo es corto.
    MIX_Audio* audio = MIX_LoadAudio(mixer, path.c_str(), true);
    if (!audio) {
        SDL_Log("Audio: no se pudo cargar '%s': %s", path.c_str(), SDL_GetError());
        failed.insert(path);
        return nullptr;
    }
    sounds[path] = audio;
    return audio;
}

void Audio::playSound(const std::string& path, float volume) {
    MIX_Audio* audio = load(path);
    if (!audio) return;

    // Una pista libre; si no hay, se crea otra; con todas ocupadas, se corta la que
    // toca por turno (casi siempre la mas vieja).
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
    // Ya se pidio (suena, o termino si no repite): solo el volumen.
    if (path == musicPath) return;

    MIX_Audio* audio = load(path);
    if (!audio) return;
    MIX_SetTrackAudio(musicTrack, audio);
    SDL_PropertiesID options = SDL_CreateProperties();
    SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0); // -1 = sin fin
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
