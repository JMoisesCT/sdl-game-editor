#include "systems/Motion.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "components/CoreComponents.h"
#include "components/MotionComponents.h"
#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "systems/Physics.h"
#include "systems/Rules.h"

namespace {
    // Px the feet may already be INSIDE the trampoline in the previous step and still
    // count as "coming from above".
    const float BOUNCE_TOLERANCE = 4.0f;

    // Point i of the path: 0 is the origin, the others are measured from it.
    Vec2 pathPoint(const PathMover& m, int i) {
        if (i <= 0) return m.origin;
        const Vec2& o = m.points[i - 1].offset;
        return { m.origin.x + o.x, m.origin.y + o.y };
    }

    // Moves on to the next point: in a circuit, from the last to the first; otherwise,
    // it bounces at the ends.
    void advance(PathMover& m, int count) {
        if (m.loop) {
            m.target = (m.target + 1) % count;
            return;
        }
        if (m.target + m.step < 0 || m.target + m.step >= count) m.step = -m.step;
        m.target += m.step;
    }

    bool hasRider(World& world, Entity platform) {
        bool found = false;
        world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D&, Transform&) {
            if (!found && isStandingOn(world, e, platform)) found = true;
        });
        return found;
    }

    // Whether the box of 'e', moved to 'position', overlaps any body (so a platform
    // does not appear on top of the player).
    bool bodyAt(World& world, Entity e, Vec2 position) {
        Vec2 min, max;
        const Transform* t = world.get<Transform>(e);
        if (!t || !colliderBounds(world, e, min, max)) return false;
        Vec2 d{ position.x - t->position.x, position.y - t->position.y };
        min = { min.x + d.x, min.y + d.y };
        max = { max.x + d.x, max.y + d.y };
        bool found = false;
        world.each<RigidBody2D, BoxCollider>([&](Entity other, RigidBody2D&, BoxCollider&) {
            Vec2 oMin, oMax;
            if (found || other == e || !colliderBounds(world, other, oMin, oMax)) return;
            found = min.x < oMax.x && oMin.x < max.x && min.y < oMax.y && oMin.y < max.y;
        });
        return found;
    }
}

// --- Paths -------------------------------------------------------------------------------

void PathMoverSystem::onPlay(World& world) {
    world.each<PathMover>([](Entity, PathMover& m) { m.started = false; });
}

void PathMoverSystem::update(World& world, float dt) {
    world.each<PathMover, Transform>([&](Entity e, PathMover& m, Transform& t) {
        // Lazy start (also works for an entity created while playing).
        if (!m.started) {
            m.started = true;
            m.origin = t.position;
            m.target = 1;
            m.step = 1;
            m.wait = 0.0f;
        }
        const int count = (int)m.points.size() + 1; // with the origin
        if (count < 2 || m.speed <= 0.0f) return;
        if (m.target >= count) m.target = count - 1;

        // The pause consumes time; whatever is left over is already movement.
        float distance = m.speed * dt;
        if (m.wait > 0.0f) {
            m.wait -= dt;
            if (m.wait > 0.0f) return;
            distance = m.speed * -m.wait;
            m.wait = 0.0f;
        }

        // Advances 'distance' along the path; if it reaches a point with distance to spare
        // (no pause, or a slow frame), it continues toward the next one.
        Vec2 pos = t.position;
        for (int guard = 0; guard < 8 && distance > 0.0f; ++guard) {
            Vec2 goal = pathPoint(m, m.target);
            float dx = goal.x - pos.x, dy = goal.y - pos.y;
            float length = std::sqrt(dx * dx + dy * dy);
            if (length > distance) {
                pos.x += dx / length * distance;
                pos.y += dy / length * distance;
                break;
            }
            pos = goal;
            distance -= length;
            advance(m, count);
            if (m.waitTime > 0.0f) {
                m.wait = m.waitTime;
                break;
            }
        }
        moveCarrying(world, e, { pos.x - t.position.x, pos.y - t.position.y });
    });
}

// --- Falling platforms -------------------------------------------------------------------

void FallingPlatformSystem::onPlay(World& world) {
    world.each<FallingPlatform>([](Entity, FallingPlatform& f) {
        f.started = f.triggered = f.falling = false;
        f.timer = f.speed = f.fade = 0.0f;
    });
}

void FallingPlatformSystem::update(World& world, float dt) {
    world.each<FallingPlatform, Transform>([&](Entity e, FallingPlatform& f, Transform& t) {
        Sprite* sprite = world.get<Sprite>(e);
        if (!f.started) {
            f.started = true;
            f.origin = t.position;
            f.baseAlpha = sprite ? sprite->tint.a : 255;
        }

        // Fade-in after returning to its place. It is already solid while appearing:
        // it can be seen, so it can be stepped on.
        if (f.fade > 0.0f) {
            f.fade = std::max(f.fade - dt, 0.0f);
            float k = f.fadeInTime > 0.0f ? 1.0f - f.fade / f.fadeInTime : 1.0f;
            if (sprite) sprite->tint.a = (std::uint8_t)std::lround(f.baseAlpha * k);
        }
        // Still until someone stands on it.
        if (!f.triggered) {
            if (hasRider(world, e)) {
                f.triggered = true;
                f.timer = 0.0f;
            }
            return;
        }

        f.timer += dt;
        if (!f.falling) {
            if (f.timer < f.delay) return; // flag: its propeller turns off
            f.falling = true;
            f.timer = 0.0f;
            f.speed = 0.0f;
        }

        // Back to its place, if nobody is there (if someone is, it waits below).
        if (f.respawnTime > 0.0f && f.timer >= f.respawnTime) {
            if (bodyAt(world, e, f.origin)) return;
            t.position = f.origin;
            f.triggered = f.falling = false;
            f.timer = f.speed = 0.0f;
            if (f.fadeInTime > 0.0f) {
                f.fade = f.fadeInTime;
                if (sprite) sprite->tint.a = 0; // starts invisible
            }
            return;
        }

        f.speed = std::min(f.speed + f.gravity * dt, f.maxFallSpeed);
        moveCarrying(world, e, { 0.0f, f.speed * dt });
    });
}

// --- Trampolines -------------------------------------------------------------------------

void BouncerSystem::onPlay(World& world) {
    world.each<Bouncer>([](Entity, Bouncer& b) { b.justBounced = false; });
}

void BouncerSystem::update(World& world, float) {
    world.each<Bouncer, BoxCollider>([&](Entity self, Bouncer& b, BoxCollider&) {
        b.justBounced = false; // the flag lasts one frame
        Vec2 sMin, sMax;
        if (!colliderBounds(world, self, sMin, sMax)) return;

        world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
            if (e == self || rb.gravity <= 0.0f || isDead(world, e)) return;
            if (!b.targetTag.empty()) {
                const EntityInfo* info = world.get<EntityInfo>(e);
                if (!info || info->tag != b.targetTag) return;
            }
            // Only what went DOWN in this step and came from above: entering from the side
            // or standing still inside does not launch.
            Vec2 bMin, bMax;
            if (t.position.y <= rb.previous.y || !collidersOverlap(world, self, e) ||
                !colliderBounds(world, e, bMin, bMax))
                return;
            float previousBottom = bMax.y - (t.position.y - rb.previous.y);
            if (previousBottom > sMin.y + BOUNCE_TOLERANCE) return;

            // Height in px -> velocity, like the PlatformerMotor's jump.
            rb.velocity.y = -std::sqrt(2.0f * rb.gravity * b.bounceHeight);
            rb.grounded = false;
            // A jump stored in the buffer or the coyote time must not override the bounce
            // (the body's own jump is lower), and releasing the button does not cut it.
            if (PlatformerMotor* m = world.get<PlatformerMotor>(e)) {
                m->coyote = 0.0f;
                m->jumping = false;
                m->airJumpsLeft = m->maxAirJumps;
            }
            b.justBounced = true;
        });
    });
}
