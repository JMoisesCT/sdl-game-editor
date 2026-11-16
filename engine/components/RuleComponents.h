#pragma once
#include <string>

#include "core/Entity.h"
#include "core/Math.h"

class ComponentRegistry;

// Gameplay RULE components: health, damage, zones that kill, collectible objects and
// respawn points. Like the others, they are ONLY data; the logic is in the systems
// of systems/Rules.h.
//
// They are all triggered by CONTACT between boxes (BoxCollider): the hazard, the fruit
// or the flag need their box (usually isTrigger, so it does not block) and so does
// whoever touches them. Whom they affect is decided by the TAG (EntityInfo::tag) of
// whoever touches them, never by their name.
//
// There are no callbacks as in v1 (onDeath, onCollect...): what happens in a frame is
// written on the component as a ONE-frame flag (justDamaged, justDied,
// justCollected...), just like justLanded on the PlatformerMotor. A game system
// running afterwards reads it and decides what it means (add points, a sound, the HUD).

// Hit points, damage and temporary invulnerability with blinking. It does not know
// who hits it: health is taken with damageEntity / killEntity (Rules.h).
//
// INVULNERABILITY: after a hit, it takes no other one for invulnerabilityTime. Without
// that, standing on some spikes would empty the health in a handful of frames, because
// the contact is detected EVERY frame it lasts. Meanwhile, the Sprite blinks.
//
// On reaching 0: if the entity has Respawn, it respawns; if not and destroyOnDeath is
// checked, it is destroyed (an enemy); otherwise, it stays dead (hp 0) and takes no more damage.
struct Health {
    int maxHP = 3;
    float invulnerabilityTime = 1.0f; // grace seconds after a hit (0 = no grace)
    float blinkInterval = 0.08f;      // seconds between turning the sprite off and on
    bool destroyOnDeath = false;      // destroyed on death (do not use together with Respawn)

    // --- Runtime state (written by the systems in Rules.h) ---
    int hp = 0;                // current health; starts at maxHP
    float invulnerable = 0.0f; // grace seconds left
    float blinkTimer = 0.0f;
    bool started = false;      // hp has already been filled with maxHP
    bool justDamaged = false;  // took damage this frame
    bool justDied = false;     // reached 0 this frame
};

// Damages whatever touches it, if it has the tag targetTag and a Health: spikes,
// saws, fire, an enemy that hurts on contact.
//
// KNOCKBACK: on a hit, it launches the target (if it has a RigidBody2D) away from the
// hazard and upward. It is not decoration: without it, the player stays on top of the
// hazard and gets hit again as soon as the invulnerability runs out.
struct Hazard {
    int damage = 1;
    std::string targetTag = "Player";
    Vec2 knockback{ 240.0f, 320.0f }; // px/s: x away from the hazard, y upward
    bool repeat = true;               // false: damages only once (a single-use trap)

    // --- Runtime state ---
    bool used = false;
};

// Kills whatever enters it on the spot (if it has targetTag and a Health), even when
// invulnerable: a strip below the level so that falling does not leave the player
// falling forever, lava, a pit. The box is usually large and a trigger.
struct KillZone {
    std::string targetTag = "Player";
};

// Collected on touch: coins, fruits, keys. The engine does NOT keep any count: what
// collecting it means is decided by a game system looking at justCollected (on that
// frame the entity still exists, even if it is already marked for destruction).
struct Collectible {
    std::string collectorTag = "Player"; // who can collect it
    bool destroyOnCollect = true;        // false: it stays (a switch you step on)

    // --- Runtime state ---
    bool collected = false;        // already collected (only once)
    bool justCollected = false;    // collected this frame
    Entity collector = NullEntity; // who collected it
};

// Flag: on touch, it sets the respawn point of whoever touches it (who needs a
// Respawn; the flag stores nothing). It activates only once.
struct Checkpoint {
    std::string targetTag = "Player";
    Vec2 offset;               // respawn point relative to the center (image px x scale)
    std::string activeTexture; // when activated, the Sprite switches to this image (empty = unchanged)

    // --- Runtime state ---
    bool activated = false;
    bool justActivated = false;
};

// Goes on the PLAYER: on death (Health at 0), it waits 'delay' seconds frozen and
// returns to the last respawn point with full health. The point starts as where it
// was when play started; a Checkpoint changes it.
struct Respawn {
    float delay = 0.6f; // seconds between death and respawn (lets the fall be seen)

    // --- Runtime state ---
    Vec2 point;                 // where it respawns
    bool started = false;       // point was already taken from the starting position
    bool dying = false;         // dead, waiting
    float timer = 0.0f;         // seconds left until respawning
    bool justRespawned = false; // respawned this frame
};

// Declares the components above to the reflection. Application calls it at startup,
// after registerPhysicsComponents.
void registerRuleComponents(ComponentRegistry& registry);
