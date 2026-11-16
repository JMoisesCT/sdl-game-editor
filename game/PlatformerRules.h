#pragma once
#include <string>

#include "systems/System.h"

class ConditionRegistry;
class ComponentRegistry;

// GAME code (not engine code): the rules of THIS platformer, built on top of the
// engine's generic pieces (Collectible, Health, Text...). It is an example of how a
// game adds ITS OWN components, conditions and systems without touching the engine.

// The level's goal: when someone with targetTag touches it, the level is complete. Needs
// a BoxCollider (trigger). It belongs to the game because "winning" does not mean the same in every game.
struct LevelGoal {
    std::string targetTag = "Player";

    // --- Runtime state ---
    bool reached = false;
};

// Declares the game's components to the reflection: they appear in the editor's "Add
// component" and are saved in the scene like the engine's. main.cpp calls it before
// opening the scene.
void registerPlatformerComponents(ComponentRegistry& registry);

// The game's conditions (systems/Conditions.h), for AnimatorStateMachine and SoundEffects:
//   goalReached  the entity's LevelGoal has been touched (the goal sinks)
void registerPlatformerConditions(ConditionRegistry& conditions);

// The playthrough: counts the fruits (Collectible tagged "Pickup") there are and the
// ones collected, and whether the goal was reached. The engine keeps no count: this
// system reads the one-frame flags (justCollected) and decides what they mean.
//
// HUD: writes the text of the entities with a Text and these tags (position, font
// and color are chosen in the editor; here only the content):
//   HudLives   "HP: 2/3" (the player's Health)
//   HudFruits  "FRUITS: 1/3"
//   HudBanner  becomes visible when the level is complete (its text is the scene's)
//
// Runs AFTER the engine's rule systems (see main.cpp), to see their flags in the
// same frame.
class PlatformerRulesSystem : public System {
public:
    const char* name() const override { return "PlatformerRules"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

    int fruits() const { return collected; }
    int totalFruits() const { return total; }
    bool levelDone() const { return done; }

private:
    void updateHud(World& world);

    int collected = 0;
    int total = 0;
    bool done = false;
};
