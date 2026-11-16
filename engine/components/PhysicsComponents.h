#pragma once
#include "core/Math.h"

class ComponentRegistry;

// Physics and character movement components. Like the basic ones, they are ONLY
// data: PhysicsSystem moves them (RigidBody2D, BoxCollider) and PlatformerMotorSystem
// (PlatformerMotor). Fields not registered in PhysicsComponents.cpp are runtime
// state: they are not saved or shown in the Inspector, and the systems write them.

// DYNAMIC body: physics moves it with its velocity and gravity. An entity without a
// RigidBody2D is static (a wall, a platform): if it has a BoxCollider, it blocks
// bodies, but it never moves on its own.
struct RigidBody2D {
    Vec2 velocity;          // px/s; the saved value is the velocity when play starts
    float gravity = 980.0f; // px/s^2 downward; 0 = no gravity (top-down, shmups)

    // --- Runtime state (written by PhysicsSystem) ---
    bool grounded = false;  // resting on something solid in the last physics step
    Vec2 previous;          // position before moving in the last step
    float ignoreOneWay = 0.0f; // seconds it still passes through one-way platforms (dropping down)
};

// Collision rectangle (AABB) centered on the Transform. Like the Sprite, it is measured
// in IMAGE pixels and multiplied by the Transform's scale: if the character grows,
// so does its box. Rotation does not apply (the box never rotates). The offset
// follows the drawing: with the Sprite flipped (flipX/flipY, or a negative scale) the
// shift is mirrored on that axis, so the box does not end up on the wrong side when
// the character turns around.
//
// Without a RigidBody2D the box is a fixed obstacle; with a RigidBody2D, physics
// separates it from solid tilemaps and from other boxes. A trigger neither blocks nor
// gets blocked: it only overlaps (to collect, damage, activate...).
//
// ONE-WAY (oneWay): a thin platform that only blocks from ABOVE. You can pass through
// it jumping from below or from the side, and when you fall onto it you stand on it.
// A body with RigidBody2D::ignoreOneWay > 0 also passes through it downward: that is
// how you drop off it (see PlatformerMotor::dropThroughTime).
struct BoxCollider {
    Vec2 size{ 16.0f, 16.0f }; // width and height (image px)
    Vec2 offset;               // shift of the box center relative to the Transform
    bool isTrigger = false;
    bool oneWay = false;       // only blocks what falls from above
};

// PLATFORMER character movement: acceleration and braking, a jump with a height in
// pixels and variable (releasing the button cuts it), coyote time, jump buffer and air
// jumps. Needs a RigidBody2D on the same entity, with gravity.
//
// It does not read the keyboard: every frame someone writes the INTENT (moveInput,
// jumpPressed, jumpHeld, dropHeld) and PlatformerMotorSystem turns it into velocity. That someone is a
// game system (see game/PlatformerControl), an AI or a replay: the motor does not change.
struct PlatformerMotor {
    // --- Settings ("game feel"); saved and shown in the Inspector ---
    float maxSpeed = 250.0f;   // px/s when running
    float accel = 2200.0f;     // px/s^2 to reach maxSpeed on the ground
    float decel = 2600.0f;     // px/s^2 to brake on the ground when released
    float airAccel = 1400.0f;  // the same in the air (less control = more weight)
    float airDecel = 700.0f;
    float jumpHeight = 150.0f; // height of a full jump in px (not a speed)
    float coyoteTime = 0.10f;  // seconds you can still jump after leaving a ledge
    float jumpBufferTime = 0.12f; // seconds a jump pressed before landing is remembered
    float cutJumpMultiplier = 0.45f; // when released while rising, the speed is multiplied by this
    float maxFallSpeed = 900.0f; // maximum fall speed (0 = no limit)
    int maxAirJumps = 0;       // extra jumps in the air (1 = double jump)
    float dropThroughTime = 0.25f; // seconds it passes through one-way platforms when
                                   // dropping off them with down + jump (0 = cannot)

    // --- Intent: written by the game EVERY frame ---
    float moveInput = 0.0f;    // -1 left, 0 still, +1 right (values in between are fine)
    bool jumpPressed = false;  // the jump button was JUST pressed this frame
    bool jumpHeld = false;     // the jump button is still held
    bool dropHeld = false;     // "down" held: together with jumpPressed, on a one-way
                               // platform, drops off it instead of jumping

    // --- Result: written by PlatformerMotorSystem (useful for animation) ---
    bool grounded = false;     // grounded, smoothed by coyote time (ignores micro-gaps)
    bool justLanded = false;   // only on the frame it touches the ground
    bool justJumped = false;   // only on the takeoff frame
    bool justDropped = false;  // only on the frame it drops off a platform

    // --- Internal state of PlatformerMotorSystem ---
    float coyote = 0.0f;       // time left in the coyote window
    float buffer = 0.0f;       // time left of the buffered jump
    int airJumpsLeft = 0;
    bool jumping = false;      // jump in progress whose height can still be cut
    bool wasGrounded = false;
};

// Declares the components above to the reflection. Application calls it at startup,
// after registerCoreComponents.
void registerPhysicsComponents(ComponentRegistry& registry);
