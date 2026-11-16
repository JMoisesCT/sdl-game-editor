#pragma once
#include <string>
#include <vector>

#include "core/Math.h"

class ComponentRegistry;

// Components for stage MECHANISMS: things that move on their own (platforms, saws),
// platforms that fall when stepped on, and trampolines. Like the others, they are
// ONLY data; the logic is in systems/Motion.h.
//
// The platforms these components move are KINEMATIC: they have no RigidBody2D
// (physics neither pushes them nor applies gravity), their system moves them, and they
// carry the bodies standing on top (moveCarrying, in Physics.h).

// A point of a PathMover's path, relative to where the entity starts.
struct PathPoint {
    Vec2 offset; // world px relative to the starting position
};

// Walks a list of points at constant speed, waiting a while at each one: a
// platform that goes back and forth, a saw that patrols. The first point of the
// path is the entity's starting position (it does not need to be in the list).
//
// With loop, when it reaches the last point it goes back to the first and repeats
// (a circuit); without loop, it walks the list there and back.
struct PathMover {
    float speed = 80.0f;   // px/s
    float waitTime = 0.5f; // seconds of pause at each point
    bool loop = false;     // true: closed circuit; false: back and forth
    std::vector<PathPoint> points;

    // --- Runtime state (written by PathMoverSystem) ---
    bool started = false;
    Vec2 origin;          // position at the start: the points are measured from here
    int target = 1;       // index of the point it is heading to (0 = the origin)
    int step = 1;         // +1 or -1 (back and forth)
    float wait = 0.0f;    // seconds of pause left
};

// A platform that FALLS when stepped on: when a body stands on it, it waits 'delay'
// seconds, falls with gravity (taking along whoever is still on it) and, respawnTime
// seconds after it started falling, returns to its place fading in (the alpha of the
// Sprite tint goes from 0 to the original over fadeInTime). It needs its solid
// BoxCollider (usually oneWay) and has no RigidBody2D.
struct FallingPlatform {
    float delay = 0.5f;          // seconds between being stepped on and falling
    float gravity = 980.0f;      // px/s^2 while falling
    float maxFallSpeed = 700.0f; // px/s
    float respawnTime = 3.0f;    // seconds from starting to fall until it returns (0 = never returns)
    float fadeInTime = 0.4f;     // seconds it takes to fade in when it returns (0 = instantly)

    // --- Runtime state (written by FallingPlatformSystem) ---
    bool started = false;
    Vec2 origin;               // where it returns to
    int baseAlpha = 255;       // original alpha of the Sprite (the end of the fade)
    float fade = 0.0f;         // seconds of fade-in left
    bool triggered = false;    // it was stepped on: about to fall or falling
    bool falling = false;
    float timer = 0.0f;        // seconds in the current state
    float speed = 0.0f;        // fall speed in px/s
};

// Trampoline: launches a body that FALLS onto it upward, to the requested height
// (in px, like the PlatformerMotor's jump). The box is usually a trigger: that way the
// trampoline is not "ground" and the motor does not mistake the bounce for its own jump.
struct Bouncer {
    float bounceHeight = 260.0f; // px the launched body rises
    std::string targetTag;       // whom it launches (empty = any RigidBody2D)

    // --- Runtime state (written by BouncerSystem) ---
    bool justBounced = false; // it launched someone this frame
};

// Declares the components above to the reflection. Application calls it at startup,
// after registerRuleComponents.
void registerMotionComponents(ComponentRegistry& registry);
