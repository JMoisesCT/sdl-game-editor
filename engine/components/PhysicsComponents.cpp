#include "components/PhysicsComponents.h"

#include "core/Reflection.h"

void registerPhysicsComponents(ComponentRegistry& registry) {
    registry.add<RigidBody2D>("RigidBody2D")
        .field("velocity", &RigidBody2D::velocity).speed(1.0f).tooltip("px/seg; el valor guardado es la velocidad inicial")
        .field("gravity", &RigidBody2D::gravity).speed(5.0f).tooltip("px/seg^2 hacia abajo (0 = sin gravedad)");

    registry.add<BoxCollider>("BoxCollider")
        .field("size", &BoxCollider::size).range(0.0f, 4096.0f).speed(0.5f)
            .tooltip("Ancho y alto en px de la imagen (se multiplican por la escala del Transform)")
        .field("offset", &BoxCollider::offset).speed(0.5f).tooltip("Corrimiento del centro de la caja (px de la imagen)")
        .field("isTrigger", &BoxCollider::isTrigger).tooltip("Solo se superpone: no frena ni es frenado");

    registry.add<PlatformerMotor>("PlatformerMotor")
        .field("maxSpeed", &PlatformerMotor::maxSpeed).range(0.0f, 5000.0f).speed(1.0f).tooltip("px/seg al correr")
        .field("accel", &PlatformerMotor::accel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Arranque en el suelo (px/seg^2)")
        .field("decel", &PlatformerMotor::decel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Frenado en el suelo al soltar (px/seg^2)")
        .field("airAccel", &PlatformerMotor::airAccel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Arranque en el aire (px/seg^2)")
        .field("airDecel", &PlatformerMotor::airDecel).range(0.0f, 50000.0f).speed(10.0f).tooltip("Frenado en el aire (px/seg^2)")
        .field("jumpHeight", &PlatformerMotor::jumpHeight).range(0.0f, 2000.0f).speed(1.0f).tooltip("Altura del salto completo (px)")
        .field("coyoteTime", &PlatformerMotor::coyoteTime).range(0.0f, 1.0f).speed(0.01f).tooltip("Seg para saltar despues de dejar el borde")
        .field("jumpBufferTime", &PlatformerMotor::jumpBufferTime).range(0.0f, 1.0f).speed(0.01f).tooltip("Seg que se recuerda un salto pulsado antes de aterrizar")
        .field("cutJumpMultiplier", &PlatformerMotor::cutJumpMultiplier).range(0.0f, 1.0f).speed(0.01f).tooltip("Soltar el salto subiendo multiplica la velocidad por esto (1 = sin salto corto)")
        .field("maxFallSpeed", &PlatformerMotor::maxFallSpeed).range(0.0f, 5000.0f).speed(5.0f).tooltip("Velocidad maxima de caida (0 = sin limite)")
        .field("maxAirJumps", &PlatformerMotor::maxAirJumps).range(0.0f, 10.0f).tooltip("Saltos extra en el aire (1 = doble salto)");
}
