#pragma once
#include <string>

#include "systems/Conditions.h"
#include "systems/System.h"

class World;
class Audio;

// Sistemas de AUDIO (componentes en components/AudioComponents.h). Van al final del
// frame, con todo ya resuelto (ver main.cpp): las condiciones de los sonidos miran los
// avisos de un frame (jumped, damaged...) antes de que se apaguen en el frame siguiente.
//
// Al detener el juego, Application corta todo el sonido; en pausa, lo congela.

// Hace sonar las reglas de los SoundEffects cuyas condiciones empiezan a cumplirse.
class SoundSystem : public System {
public:
    SoundSystem(Audio& audio, const ConditionRegistry& conditions) : audio(audio), conditions(conditions) {}
    const char* name() const override { return "Sound"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    Audio& audio;
    const ConditionRegistry& conditions;
};

// Hace sonar la Music de la escena (la primera que encuentra) y la cambia si cambia.
class MusicSystem : public System {
public:
    explicit MusicSystem(Audio& audio) : audio(audio) {}
    const char* name() const override { return "Music"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    Audio& audio;
    std::string playing; // la que se pidio por ultima vez ("" = ninguna)
};
