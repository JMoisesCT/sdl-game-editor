#pragma once
#include <string>

#include "systems/Conditions.h"
#include "systems/System.h"

class World;
class Audio;

// AUDIO systems (components in components/AudioComponents.h). They run at the end
// of the frame, with everything resolved (see main.cpp): the sound conditions look at
// the one-frame flags (jumped, damaged...) before they are cleared on the next frame.
//
// When the game stops, Application cuts all sound; when paused, it freezes it.

// Plays the SoundEffects rules whose conditions start to hold.
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

// Plays the scene's Music (the first one it finds) and switches it if it changes.
class MusicSystem : public System {
public:
    explicit MusicSystem(Audio& audio) : audio(audio) {}
    const char* name() const override { return "Music"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    Audio& audio;
    std::string playing; // the last one requested ("" = none)
};
