#include "components/PhysicsComponents.h"

#include "core/Reflection.h"

void registerPhysicsComponents(ComponentRegistry& registry) {
    registry.add<RigidBody2D>("RigidBody2D")
        .field("velocity", &RigidBody2D::velocity).speed(1.0f).tooltip("px/s; the saved value is the initial velocity")
        .field("gravity", &RigidBody2D::gravity).speed(5.0f).tooltip("px/s^2 downward (0 = no gravity)");

    registry.add<BoxCollider>("BoxCollider")
        .field("size", &BoxCollider::size).range(0.0f, 4096.0f).speed(0.5f)
            .tooltip("Width and height in image px (multiplied by the Transform scale)")
        .field("offset", &BoxCollider::offset).speed(0.5f).tooltip("Shift of the box center (image px; mirrored when the Sprite is flipped)")
        .field("isTrigger", &BoxCollider::isTrigger).tooltip("Only overlaps: neither blocks nor gets blocked")
        .field("oneWay", &BoxCollider::oneWay)
            .tooltip("One-way platform: only stops what falls from above (you can jump up through it)");

    registry.add<PlatformerMotor>("PlatformerMotor")
        .field("maxSpeed", &PlatformerMotor::maxSpeed).range(0.0f, 5000.0f).speed(1.0f).tooltip("px/s when running")
        .field("accel", &PlatformerMotor::accel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Acceleration on the ground (px/s^2)")
        .field("decel", &PlatformerMotor::decel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Deceleration on the ground when released (px/s^2)")
        .field("airAccel", &PlatformerMotor::airAccel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Acceleration in the air (px/s^2)")
        .field("airDecel", &PlatformerMotor::airDecel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Deceleration in the air (px/s^2)")
        .field("jumpHeight", &PlatformerMotor::jumpHeight).range(0.0f, 2000.0f).speed(1.0f).tooltip("Height of a full jump (px)")
        .field("coyoteTime", &PlatformerMotor::coyoteTime).range(0.0f, 1.0f).speed(0.01f).tooltip("Seconds you can still jump after leaving a ledge")
        .field("jumpBufferTime", &PlatformerMotor::jumpBufferTime).range(0.0f, 1.0f).speed(0.01f).tooltip("Seconds a jump pressed before landing is remembered")
        .field("cutJumpMultiplier", &PlatformerMotor::cutJumpMultiplier).range(0.0f, 1.0f).speed(0.01f).tooltip("Releasing jump while rising multiplies the speed by this (1 = no short jump)")
        .field("maxFallSpeed", &PlatformerMotor::maxFallSpeed).range(0.0f, 5000.0f).speed(5.0f).tooltip("Maximum fall speed (0 = no limit)")
        .field("maxAirJumps", &PlatformerMotor::maxAirJumps).range(0.0f, 10.0f).tooltip("Extra jumps in the air (1 = double jump)")
        .field("dropThroughTime", &PlatformerMotor::dropThroughTime).range(0.0f, 2.0f).speed(0.01f)
            .tooltip("Seconds it passes through one-way platforms when dropping with down + jump (0 = cannot drop)");
}
