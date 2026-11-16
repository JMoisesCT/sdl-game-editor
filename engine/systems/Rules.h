#pragma once
#include "core/Entity.h"
#include "systems/System.h"

class World;

// RULE systems (components in components/RuleComponents.h). They all work the same
// way: they look for contacts between the rule's box (the hazard, the fruit, the
// flag) and the boxes of the entities with the tag they care about, and act.
//
// They run AFTER PhysicsSystem (they look at the resolved positions) and in this
// order (see main.cpp):
//   HealthSystem      -> counts invulnerability, blinks, and clears the previous
//                        frame's one-frame flags (justDamaged, justDied)
//   HazardSystem      -> damage + knockback
//   KillZoneSystem    -> instant death
//   CollectibleSystem -> collecting
//   CheckpointSystem  -> setting the respawn point
//   RespawnSystem     -> waiting and respawning the dead
// HealthSystem goes FIRST because it is the one that clears the flags: that way a hit
// from this frame stays flagged until the end of the frame, for the game systems.

// Contact margin (px): two solid boxes that physics leaves edge to edge (a solid
// enemy against the player) also count as touching.
constexpr float RULE_CONTACT_MARGIN = 1.0f;

// Takes health respecting invulnerability. true if the hit counted. If health reaches
// 0 the entity dies (see Health). Without Health it does nothing.
bool damageEntity(World& world, Entity target, int amount);
// Kills on the spot, even when invulnerable (falling into the void). Without Health it does nothing.
void killEntity(World& world, Entity target);
// Restores health, without going over maxHP. It does not heal the dead.
void healEntity(World& world, Entity target, int amount);
// true if it has Health and its health reached 0 (waiting to respawn, or without Respawn).
bool isDead(World& world, Entity e);

class HealthSystem : public System {
public:
    const char* name() const override { return "Health"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class HazardSystem : public System {
public:
    const char* name() const override { return "Hazard"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class KillZoneSystem : public System {
public:
    const char* name() const override { return "KillZone"; }
    void update(World& world, float dt) override;
};

class CollectibleSystem : public System {
public:
    const char* name() const override { return "Collectible"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class CheckpointSystem : public System {
public:
    const char* name() const override { return "Checkpoint"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class RespawnSystem : public System {
public:
    const char* name() const override { return "Respawn"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
