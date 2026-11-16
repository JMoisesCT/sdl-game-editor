#pragma once
#include "systems/System.h"

struct PlatformerMotor;
struct RigidBody2D;

// Turns the INTENT of each PlatformerMotor (moveInput, jumpPressed, jumpHeld,
// dropHeld) into the velocity of its RigidBody2D: accelerates and brakes toward the
// desired speed, jumps (with coyote time and jump buffer), cuts the jump on release,
// limits the fall and, with down + jump on a one-way platform, drops off it
// (RigidBody2D::ignoreOneWay).
//
// ORDER of the systems in each frame:
//     game control (keys -> intent)
//       -> PlatformerMotorSystem (intent -> velocity)
//         -> PhysicsSystem (velocity -> position, collisions and 'grounded')
// The motor reads the 'grounded' physics left in the previous frame. If the control
// ran after the motor, the key would arrive one frame late.
class PlatformerMotorSystem : public System {
public:
    const char* name() const override { return "PlatformerMotor"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Initial velocity of a full jump, derived from jumpHeight and the body's gravity:
// to rise h px with gravity g you must start at sqrt(2 * g * h). That is why the
// setting is a HEIGHT: if gravity changes, the jump still reaches the same height.
float platformerJumpSpeed(const PlatformerMotor& motor, const RigidBody2D& rb);
