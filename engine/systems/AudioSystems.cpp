#include "systems/AudioSystems.h"

#include <SDL3/SDL.h>

#include <set>

#include "components/AudioComponents.h"
#include "core/World.h"
#include "platform/Audio.h"

// --- Efectos -----------------------------------------------------------------------------

void SoundSystem::onPlay(World& world) {
    world.each<SoundEffects>([](Entity, SoundEffects& fx) {
        fx.started = false;
        for (SoundRule& rule : fx.sounds) rule.wasTrue = false;
    });
}

void SoundSystem::update(World& world, float) {
    static std::set<std::string> warned; // un aviso por nombre desconocido
    world.each<SoundEffects>([&](Entity e, SoundEffects& fx) {
        for (SoundRule& rule : fx.sounds) {
            const Condition* condition = conditions.find(rule.condition);
            if (!condition) {
                if (!rule.condition.empty() && warned.insert(rule.condition).second)
                    SDL_Log("SoundEffects: no existe la condicion '%s' (no suena nunca)", rule.condition.c_str());
                continue;
            }
            const bool now = (*condition)(world, e);
            // Solo en el frame en que EMPIEZA a cumplirse. El primer frame se anota sin
            // sonar: lo que ya era cierto al empezar a jugar (estar apoyado) no es un suceso.
            if (now && !rule.wasTrue && fx.started) audio.playSound(rule.sound, rule.volume);
            rule.wasTrue = now;
        }
        fx.started = true;
    });
}

// --- Musica ------------------------------------------------------------------------------

void MusicSystem::onPlay(World&) {
    playing.clear(); // el primer update la hace sonar
}

void MusicSystem::update(World& world, float) {
    const Music* music = nullptr;
    world.each<Music>([&](Entity, Music& m) {
        if (!music) music = &m;
    });
    if (music && !music->music.empty()) {
        // Cada frame: si es la misma solo se ajusta el volumen (tocarlo en el Inspector
        // mientras se juega se oye en el acto).
        audio.playMusic(music->music, music->volume, music->loop);
        playing = music->music;
    } else if (!playing.empty()) {
        audio.stopMusic();
        playing.clear();
    }
}
