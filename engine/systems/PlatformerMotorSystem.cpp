#include "systems/PlatformerMotorSystem.h"

#include <cmath>

#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "systems/Physics.h"

namespace {
    // Moves 'current' toward 'target' by at most 'maxDelta', without overshooting:
    // acceleration and braking without writing the same math twice.
    float moveToward(float current, float target, float maxDelta) {
        if (current < target) return std::fmin(current + maxDelta, target);
        if (current > target) return std::fmax(current - maxDelta, target);
        return target;
    }
}

float platformerJumpSpeed(const PlatformerMotor& motor, const RigidBody2D& rb) {
    if (rb.gravity <= 0.0f || motor.jumpHeight <= 0.0f) return 0.0f;
    return std::sqrt(2.0f * rb.gravity * motor.jumpHeight);
}

void PlatformerMotorSystem::onPlay(World& world) {
    // Clean state at the start: no jumps or windows from the previous session.
    world.each<PlatformerMotor>([](Entity, PlatformerMotor& m) {
        m.moveInput = 0.0f;
        m.jumpPressed = m.jumpHeld = m.dropHeld = false;
        m.grounded = m.justLanded = m.justJumped = m.justDropped = false;
        m.coyote = m.buffer = 0.0f;
        m.airJumpsLeft = 0;
        m.jumping = m.wasGrounded = false;
    });
}

void PlatformerMotorSystem::update(World& world, float dt) {
    world.each<PlatformerMotor, RigidBody2D>([&](Entity e, PlatformerMotor& m, RigidBody2D& rb) {
        // --- Grounding ---------------------------------------------------------------
        bool groundedNow = rb.grounded;
        m.justLanded = groundedNow && !m.wasGrounded;
        m.wasGrounded = groundedNow;
        if (groundedNow) {
            m.coyote = m.coyoteTime;       // refills the window while grounded
            m.airJumpsLeft = m.maxAirJumps; // and gives back the air jumps
            m.jumping = false;
        } else if (m.coyote > 0.0f) {
            m.coyote -= dt;
        }

        // --- Jump button memory -----------------------------------------------------
        // Pressed shortly BEFORE landing, the jump is not lost: it waits in the buffer
        // until there is ground (or until the window runs out).
        if (m.jumpPressed) m.buffer = m.jumpBufferTime;
        else if (m.buffer > 0.0f) m.buffer -= dt;

        // --- Horizontal: accelerate or brake toward the desired speed --------------
        // Without this the character starts and stops abruptly (velocity = input * maxSpeed),
        // which is exactly what makes a platformer feel stiff.
        bool wantsToMove = m.moveInput != 0.0f;
        float rate = groundedNow ? (wantsToMove ? m.accel : m.decel)
                                 : (wantsToMove ? m.airAccel : m.airDecel);
        rb.velocity.x = moveToward(rb.velocity.x, m.moveInput * m.maxSpeed, rate * dt);

        // --- Dropping off a one-way platform: down + jump ---------------------------
        // Only with a press from THIS frame (not the buffered one: holding down while
        // falling and jumping right before landing must not pass through the floor) and
        // only if there is nothing solid below; if there is, it is a normal jump.
        m.justJumped = false;
        m.justDropped = false;
        if (m.jumpPressed && m.dropHeld && groundedNow && m.dropThroughTime > 0.0f &&
            isOnOneWayOnly(world, e)) {
            rb.ignoreOneWay = m.dropThroughTime;
            m.buffer = 0.0f;  // the button was used to drop
            m.coyote = 0.0f;  // and no "coyote" jump is left in the air
            m.justDropped = true;
        }

        // --- Jump --------------------------------------------------------------------
        if (m.buffer > 0.0f) {
            bool fromGround = m.coyote > 0.0f;
            bool fromAir = !fromGround && m.airJumpsLeft > 0;
            if (fromGround || fromAir) {
                rb.velocity.y = -platformerJumpSpeed(m, rb);
                m.buffer = 0.0f; // consumed
                m.coyote = 0.0f; // and the window too, so two jumps do not chain
                if (fromAir) --m.airJumpsLeft;
                m.jumping = true;
                m.justJumped = true;
            }
        }

        // --- Variable height: releasing the button cuts the rise --------------------
        if (m.jumping && !m.jumpHeld && rb.velocity.y < 0.0f) {
            rb.velocity.y *= m.cutJumpMultiplier;
            m.jumping = false; // already cut: do not cut this jump again
        }
        if (m.jumping && rb.velocity.y >= 0.0f) m.jumping = false; // it already started falling

        // --- Maximum fall speed ------------------------------------------------------
        // Before physics adds this frame's gravity: the limit arrives one frame late,
        // irrelevant at 60 fps, and the RigidBody2D stays generic.
        if (m.maxFallSpeed > 0.0f && rb.velocity.y > m.maxFallSpeed) rb.velocity.y = m.maxFallSpeed;

        // Smoothed grounding for whoever asks (animation): ignores micro-gaps.
        m.grounded = m.coyote > 0.0f;
    });
}
