#pragma once
#include "systems/System.h"

class World;

// MECHANISM systems (components in components/MotionComponents.h). They go like this
// in the frame (see main.cpp):
//   PathMoverSystem, FallingPlatformSystem -> BEFORE PhysicsSystem: they move the
//       platforms and their passengers (moveCarrying), and physics then resolves
//       collisions with everything already in place.
//   BouncerSystem -> AFTER PhysicsSystem: it checks who fell onto the trampoline with
//       the positions already resolved, and its new velocity applies on the next
//       frame.

// Moves the entities with a PathMover along their path.
class PathMoverSystem : public System {
public:
    const char* name() const override { return "PathMover"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Makes stepped-on FallingPlatforms fall and returns them to their place.
class FallingPlatformSystem : public System {
public:
    const char* name() const override { return "FallingPlatform"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Launches upward the bodies that fall onto a Bouncer.
class BouncerSystem : public System {
public:
    const char* name() const override { return "Bouncer"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
