#include "systems/AudioSystems.h"

#include <SDL3/SDL.h>

#include <set>

#include "components/AudioComponents.h"
#include "core/World.h"
#include "platform/Audio.h"

// --- Effects -----------------------------------------------------------------------------

void SoundSystem::onPlay(World& world) {
    world.each<SoundEffects>([](Entity, SoundEffects& fx) {
        fx.started = false;
        for (SoundRule& rule : fx.sounds) rule.wasTrue = false;
    });
}

void SoundSystem::update(World& world, float) {
    static std::set<std::string> warned; // one warning per unknown name
    world.each<SoundEffects>([&](Entity e, SoundEffects& fx) {
        for (SoundRule& rule : fx.sounds) {
            const Condition* condition = conditions.find(rule.condition);
            if (!condition) {
                if (!rule.condition.empty() && warned.insert(rule.condition).second)
                    SDL_Log("SoundEffects: condition '%s' does not exist (it never plays)", rule.condition.c_str());
                continue;
            }
            const bool now = (*condition)(world, e);
            // Only on the frame in which it STARTS to hold. The first frame is recorded without
            // playing: what was already true when play started (being grounded) is not an event.
            if (now && !rule.wasTrue && fx.started) audio.playSound(rule.sound, rule.volume);
            rule.wasTrue = now;
        }
        fx.started = true;
    });
}

// --- Music -------------------------------------------------------------------------------

void MusicSystem::onPlay(World&) {
    playing.clear(); // the first update starts it
}

void MusicSystem::update(World& world, float) {
    const Music* music = nullptr;
    world.each<Music>([&](Entity, Music& m) {
        if (!music) music = &m;
    });
    if (music && !music->music.empty()) {
        // Every frame: if it is the same one, only the volume is adjusted (changing it in the
        // Inspector while playing is heard right away).
        audio.playMusic(music->music, music->volume, music->loop);
        playing = music->music;
    } else if (!playing.empty()) {
        audio.stopMusic();
        playing.clear();
    }
}
