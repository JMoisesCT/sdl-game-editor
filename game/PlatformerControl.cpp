#include "PlatformerControl.h"

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "platform/Input.h"
#include "systems/Rules.h"

static const char* TAG_PLAYER = "Player";

void PlatformerControlSystem::update(World& world, float) {
    float move = Input::axis(Key::Left, Key::Right) + Input::axis(Key::A, Key::D);
    if (move > 1.0f) move = 1.0f;
    if (move < -1.0f) move = -1.0f;
    bool jumpPressed = Input::wasPressed(Key::Space) || Input::wasPressed(Key::W) || Input::wasPressed(Key::Up);
    bool jumpHeld = Input::isDown(Key::Space) || Input::isDown(Key::W) || Input::isDown(Key::Up);

    // Se filtra por tag, nunca por nombre: el nombre es para las personas.
    world.each<PlatformerMotor, EntityInfo>([&](Entity e, PlatformerMotor& motor, EntityInfo& info) {
        if (info.tag != TAG_PLAYER) return;
        // Muerto no se controla: sin intencion, y el Respawn hace el resto.
        if (isDead(world, e)) {
            motor.moveInput = 0.0f;
            motor.jumpPressed = motor.jumpHeld = false;
            return;
        }
        motor.moveInput = move;
        motor.jumpPressed = jumpPressed; // flanco: un solo frame
        motor.jumpHeld = jumpHeld;       // para el salto corto

        // Hacia donde mira el sprite. Es presentacion, no fisica: por eso va aqui y
        // no en el motor.
        if (Sprite* sprite = world.get<Sprite>(e)) {
            if (move < 0.0f) sprite->flipX = true;
            else if (move > 0.0f) sprite->flipX = false;
        }
    });
}
