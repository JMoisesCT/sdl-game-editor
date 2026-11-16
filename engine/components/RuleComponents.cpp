#include "components/RuleComponents.h"

#include "core/Reflection.h"

void registerRuleComponents(ComponentRegistry& registry) {
    registry.add<Health>("Health")
        .field("maxHP", &Health::maxHP).range(1.0f, 9999.0f).tooltip("Health at start and on respawn")
        .field("invulnerabilityTime", &Health::invulnerabilityTime).range(0.0f, 10.0f).speed(0.01f)
            .tooltip("Seconds without taking damage after a hit (the sprite blinks)")
        .field("blinkInterval", &Health::blinkInterval).range(0.01f, 1.0f).speed(0.01f).tooltip("Seconds between blinks")
        .field("destroyOnDeath", &Health::destroyOnDeath).tooltip("Destroyed on death (enemies). Do not use with Respawn");

    registry.add<Hazard>("Hazard")
        .field("damage", &Hazard::damage).range(0.0f, 9999.0f).tooltip("Health each hit takes away")
        .field("targetTag", &Hazard::targetTag).tooltip("Tag of whom it damages")
        .field("knockback", &Hazard::knockback).speed(1.0f).tooltip("Knockback (px/s): x away from it, y upward")
        .field("repeat", &Hazard::repeat).tooltip("Unchecked: damages only once");

    registry.add<KillZone>("KillZone")
        .field("targetTag", &KillZone::targetTag).tooltip("Tag of whom it kills (ignores invulnerability)");

    registry.add<Collectible>("Collectible")
        .field("collectorTag", &Collectible::collectorTag).tooltip("Tag of who can collect it")
        .field("destroyOnCollect", &Collectible::destroyOnCollect).tooltip("Disappears when collected");

    registry.add<Checkpoint>("Checkpoint")
        .field("targetTag", &Checkpoint::targetTag).tooltip("Tag of who activates it (needs Respawn)")
        .field("offset", &Checkpoint::offset).speed(0.5f).tooltip("Respawn point relative to the center (image px)")
        .field("activeTexture", &Checkpoint::activeTexture).texture().tooltip("Sprite image once activated (empty = unchanged)");

    registry.add<Respawn>("Respawn")
        .field("delay", &Respawn::delay).range(0.0f, 10.0f).speed(0.01f).tooltip("Seconds between death and respawn");
}
