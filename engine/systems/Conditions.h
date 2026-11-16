#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/Entity.h"

class World;

// Named CONDITIONS: given an entity, whether something holds NOW. They are used by
// the components that react to an entity's state without anyone writing code for
// them, picking the condition by name in the editor:
//   - AnimatorStateMachine: "play the jump clip while rising holds".
//   - SoundEffects: "play jump.wav when jumped starts to hold".
// That way the rule is built in the editor and the logic stays in C++.
using Condition = std::function<bool(World&, Entity)>;

// Application owns a registry (conditions()) and loads the engine's into it; the
// game adds its own before playing:
//     app.conditions().add("attacking", [](World& w, Entity e) {
//         const Weapon* weapon = w.get<Weapon>(e);
//         return weapon && weapon->cooldown > 0.0f;
//     });
class ConditionRegistry {
public:
    // Adds (or replaces) a condition.
    void add(const std::string& name, Condition condition) { conditions[name] = std::move(condition); }
    // nullptr if it does not exist.
    const Condition* find(const std::string& name) const;
    // The names, in alphabetical order (for the Inspector).
    std::vector<std::string> names() const;

private:
    std::map<std::string, Condition> conditions;
};

// The engine's. The movement ones look at RigidBody2D (and at the PlatformerMotor, if
// any, for the grounding smoothed by coyote time); the rule ones, at their components.
//   States (last while they happen):
//     grounded  on the ground              airborne  in the air
//     rising    in the air, going up       falling   in the air, going down
//     moving    moving (speed > 1 px/s)
//     dead      Health at 0                hurt      in the grace period after a hit (alive)
//     collected Collectible collected      activated Checkpoint activated
//     crumbling FallingPlatform stepped on (about to fall or falling)
//   Events (a single frame, from the justX flags):
//     jumped    took off (PlatformerMotor) landed    landed (PlatformerMotor)
//     damaged   took a hit (Health)        respawned respawned (Respawn)
//     bounced   the Bouncer launched someone dropped   dropped off a platform (PlatformerMotor)
void registerEngineConditions(ConditionRegistry& conditions);
