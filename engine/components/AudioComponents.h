#pragma once
#include <string>
#include <vector>

class ComponentRegistry;
class ConditionRegistry;

// AUDIO components. Like all of them, they are ONLY data: SoundSystem and MusicSystem
// (systems/AudioSystems.h) play them through platform/Audio.

// A sound and WHEN it plays: when the condition STARTS to hold (the frame in which it
// goes from false to true). That way it works the same for a one-frame event (jumped,
// damaged) as for a state (dead, collected): it plays once on death, not every dead frame.
struct SoundRule {
    std::string condition;   // name of a registered condition (systems/Conditions.h)
    std::string sound;       // audio file
    float volume = 1.0f;     // 0 = muted, 1 = as recorded

    // --- Runtime state ---
    bool wasTrue = false;    // the condition in the previous frame
};

// An entity's sound effects: a list of rules "play X when Y", like the
// AnimatorStateMachine's but without priority (every one that starts plays).
struct SoundEffects {
    std::vector<SoundRule> sounds;

    // --- Runtime state ---
    bool started = false;    // the conditions were already checked once (the first frame
                             // only records them: what was already true at the start does not play)
};

// The background music. It plays while there is an entity with Music (the first one
// found); if the game changes the file, the music changes.
struct Music {
    std::string music;       // audio file
    float volume = 0.5f;
    bool loop = true;
};

// Declares the components above to the reflection. Application calls it at startup,
// after registerPresentationComponents.
void registerAudioComponents(ComponentRegistry& registry, const ConditionRegistry& conditions);
