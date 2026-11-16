#pragma once
#include <string>

#include "core/Entity.h"
#include "systems/Conditions.h"
#include "systems/System.h"

class World;
class AssetManager;

// ANIMATION systems (components in components/PresentationComponents.h):
//   AnimatorStateMachineSystem -> picks the clip from each entity's rules
//   SpriteAnimatorSystem       -> advances the frames and writes the Sprite
// They run in that order and AFTER physics and rules (see main.cpp): the
// conditions (systems/Conditions.h) look at this frame's already resolved state.

// Plays that clip of the entity's SpriteAnimator. If it is already playing it does
// nothing (it can be called every frame without cutting it), except with restart = true
// (to repeat a hit). false if the entity has no SpriteAnimator or no such clip.
bool playAnimation(World& world, Entity e, const std::string& clip, bool restart = false);

class AnimatorStateMachineSystem : public System {
public:
    explicit AnimatorStateMachineSystem(const ConditionRegistry& conditions) : conditions(conditions) {}
    const char* name() const override { return "AnimatorStateMachine"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    const ConditionRegistry& conditions;
};

class SpriteAnimatorSystem : public System {
public:
    // Needs the images to know how many frames a clip without 'frames' has.
    explicit SpriteAnimatorSystem(AssetManager& assets) : assets(assets) {}
    const char* name() const override { return "SpriteAnimator"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    AssetManager& assets;
};
