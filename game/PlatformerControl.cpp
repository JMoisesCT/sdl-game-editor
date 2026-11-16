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
    bool dropHeld = Input::isDown(Key::Down) || Input::isDown(Key::S); // + jump: drop off a platform

    // Filter by tag, never by name: the name is for people.
    world.each<PlatformerMotor, EntityInfo>([&](Entity e, PlatformerMotor& motor, EntityInfo& info) {
        if (info.tag != TAG_PLAYER) return;
        // Dead, no control: no intent, and Respawn does the rest.
        if (isDead(world, e)) {
            motor.moveInput = 0.0f;
            motor.jumpPressed = motor.jumpHeld = motor.dropHeld = false;
            return;
        }
        motor.moveInput = move;
        motor.jumpPressed = jumpPressed; // edge: a single frame
        motor.jumpHeld = jumpHeld;       // for the short jump
        motor.dropHeld = dropHeld;

        // Which way the sprite faces. It is presentation, not physics: that is why it goes
        // here and not in the motor.
        if (Sprite* sprite = world.get<Sprite>(e)) {
            if (move < 0.0f) sprite->flipX = true;
            else if (move > 0.0f) sprite->flipX = false;
        }
    });
}
