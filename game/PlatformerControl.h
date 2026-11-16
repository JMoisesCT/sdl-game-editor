#pragma once
#include "systems/System.h"

// GAME code (not engine code): translates KEYS into INTENT for the PlatformerMotor
// of the entities tagged "Player". That is all it does: it knows nothing about gravity,
// coyote time or double jumps; the motor takes care of that, and it works for any
// platformer. Changing the controls (or plugging in a gamepad, or an AI) means changing
// only this system.
//
// Controls: arrows or A/D to move; Space, W or the up arrow to jump; down (arrow
// or S) + jump to drop off a one-way platform.
//
// Runs BEFORE PlatformerMotorSystem (see main.cpp): if it ran after, the key would
// reach the motor one frame late.
class PlatformerControlSystem : public System {
public:
    const char* name() const override { return "PlatformerControl"; }
    void update(World& world, float dt) override;
};
