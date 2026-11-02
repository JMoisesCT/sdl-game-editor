#include "systems/PlatformerMotorSystem.h"

#include <cmath>

#include "components/PhysicsComponents.h"
#include "core/World.h"

namespace {
    // Acerca 'current' a 'target' como mucho 'maxDelta', sin pasarse: la aceleracion y
    // el frenado sin escribir dos veces la misma cuenta.
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
    // Estado limpio al empezar: nada de saltos ni ventanas de la partida anterior.
    world.each<PlatformerMotor>([](Entity, PlatformerMotor& m) {
        m.moveInput = 0.0f;
        m.jumpPressed = m.jumpHeld = false;
        m.grounded = m.justLanded = m.justJumped = false;
        m.coyote = m.buffer = 0.0f;
        m.airJumpsLeft = 0;
        m.jumping = m.wasGrounded = false;
    });
}

void PlatformerMotorSystem::update(World& world, float dt) {
    world.each<PlatformerMotor, RigidBody2D>([dt](Entity, PlatformerMotor& m, RigidBody2D& rb) {
        // --- Apoyo -------------------------------------------------------------------
        bool groundedNow = rb.grounded;
        m.justLanded = groundedNow && !m.wasGrounded;
        m.wasGrounded = groundedNow;
        if (groundedNow) {
            m.coyote = m.coyoteTime;       // recarga la ventana mientras este apoyado
            m.airJumpsLeft = m.maxAirJumps; // y devuelve los saltos de aire
            m.jumping = false;
        } else if (m.coyote > 0.0f) {
            m.coyote -= dt;
        }

        // --- Memoria del boton de salto ---------------------------------------------
        // Pulsado poco ANTES de aterrizar, el salto no se pierde: espera en el buffer
        // hasta que haya suelo (o hasta que se acabe la ventana).
        if (m.jumpPressed) m.buffer = m.jumpBufferTime;
        else if (m.buffer > 0.0f) m.buffer -= dt;

        // --- Horizontal: acelerar o frenar hacia la velocidad deseada --------------
        // Sin esto el personaje arranca y para de golpe (velocidad = input * maxSpeed),
        // que es justo lo que hace que un plataformas se sienta rigido.
        bool wantsToMove = m.moveInput != 0.0f;
        float rate = groundedNow ? (wantsToMove ? m.accel : m.decel)
                                 : (wantsToMove ? m.airAccel : m.airDecel);
        rb.velocity.x = moveToward(rb.velocity.x, m.moveInput * m.maxSpeed, rate * dt);

        // --- Salto -------------------------------------------------------------------
        m.justJumped = false;
        if (m.buffer > 0.0f) {
            bool fromGround = m.coyote > 0.0f;
            bool fromAir = !fromGround && m.airJumpsLeft > 0;
            if (fromGround || fromAir) {
                rb.velocity.y = -platformerJumpSpeed(m, rb);
                m.buffer = 0.0f; // consumido
                m.coyote = 0.0f; // y la ventana tambien, para no encadenar dos saltos
                if (fromAir) --m.airJumpsLeft;
                m.jumping = true;
                m.justJumped = true;
            }
        }

        // --- Altura variable: soltar el boton corta la subida ------------------------
        if (m.jumping && !m.jumpHeld && rb.velocity.y < 0.0f) {
            rb.velocity.y *= m.cutJumpMultiplier;
            m.jumping = false; // ya se corto: no volver a cortar este salto
        }
        if (m.jumping && rb.velocity.y >= 0.0f) m.jumping = false; // ya empezo a caer

        // --- Velocidad maxima de caida -----------------------------------------------
        // Antes de que la fisica sume la gravedad de este frame: el limite llega con un
        // frame de retraso, irrelevante a 60 fps, y el RigidBody2D sigue siendo generico.
        if (m.maxFallSpeed > 0.0f && rb.velocity.y > m.maxFallSpeed) rb.velocity.y = m.maxFallSpeed;

        // Apoyo suavizado para quien lo consulte (animacion): ignora microcortes.
        m.grounded = m.coyote > 0.0f;
    });
}
