#include "systems/Physics.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "systems/Tilemaps.h"

namespace {
    // Margin taken off a box when looking for the cells or boxes it touches. Without it,
    // a body resting EXACTLY on the ground would also count the row below (their edges
    // coincide) and collisions that do not exist would be resolved.
    const float SKIN = 0.01f;

    // Which way the entity's drawing is flipped, with the same rule as RenderSystem:
    // the Sprite's flip and the sign of the scale are combined.
    struct Mirror { bool x = false, y = false; };

    Mirror mirrorOf(World& world, Entity e, const Transform& t) {
        const Sprite* s = world.get<Sprite>(e);
        return { (s && s->flipX) != (t.scale.x < 0.0f), (s && s->flipY) != (t.scale.y < 0.0f) };
    }

    // Box in the world. The offset is mirrored with the drawing (see BoxCollider).
    void boxOf(const BoxCollider& c, const Transform& t, Mirror m, Vec2& min, Vec2& max) {
        float sx = std::fabs(t.scale.x), sy = std::fabs(t.scale.y);
        float cx = t.position.x + c.offset.x * sx * (m.x ? -1.0f : 1.0f);
        float cy = t.position.y + c.offset.y * sy * (m.y ? -1.0f : 1.0f);
        float hw = std::fabs(c.size.x) * sx * 0.5f;
        float hh = std::fabs(c.size.y) * sy * 0.5f;
        min = { cx - hw, cy - hh };
        max = { cx + hw, cy + hh };
    }

    bool boxesOverlap(Vec2 aMin, Vec2 aMax, Vec2 bMin, Vec2 bMax) {
        return aMin.x < bMax.x && bMin.x < aMax.x && aMin.y < bMax.y && bMin.y < aMax.y;
    }

    // Map cells the rectangle touches (with the SKIN margin), clipped to the grid.
    // false if it touches none.
    bool cellRange(const Tilemap& map, const Transform& mt, Vec2 min, Vec2 max,
                   int& c0, int& r0, int& c1, int& r1) {
        tilemapWorldToCell(map, mt, { min.x + SKIN, min.y + SKIN }, c0, r0);
        tilemapWorldToCell(map, mt, { max.x - SKIN, max.y - SKIN }, c1, r1);
        c0 = std::max(c0, 0);
        r0 = std::max(r0, 0);
        c1 = std::min(c1, map.cells.width - 1);
        r1 = std::min(r1, map.cells.height - 1);
        return c0 <= c1 && r0 <= r1;
    }

    // How much to move the box in X to get it out of the solid cells it touches. 'dir' is
    // which way it moved in X (>0, <0, or 0 if it did not move).
    float pushOutX(const Tilemap& map, const Transform& mt, Vec2 min, Vec2 max, float dir) {
        int c0, r0, c1, r1;
        if (!cellRange(map, mt, min, max, c0, r0, c1, r1)) return 0.0f;
        float best = 0.0f;
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                if (!isSolidCell(map, col, row)) continue;
                float cellLeft = tilemapCellOrigin(map, mt, col, row).x;
                float cellRight = tilemapCellOrigin(map, mt, col + 1, row).x;
                float push;
                if (dir > 0.0f) push = cellLeft - max.x;       // it was moving right
                else if (dir < 0.0f) push = cellRight - min.x; // it was moving left
                else {
                    // It did not move in X and still overlaps (it appeared inside a wall, the
                    // map moved...): it goes out the shortest way.
                    float toLeft = cellLeft - max.x, toRight = cellRight - min.x;
                    push = std::fabs(toLeft) < std::fabs(toRight) ? toLeft : toRight;
                }
                if (std::fabs(push) > std::fabs(best)) best = push;
            }
        }
        return best;
    }

    float pushOutY(const Tilemap& map, const Transform& mt, Vec2 min, Vec2 max, float dir) {
        int c0, r0, c1, r1;
        if (!cellRange(map, mt, min, max, c0, r0, c1, r1)) return 0.0f;
        float best = 0.0f;
        for (int row = r0; row <= r1; ++row) {
            for (int col = c0; col <= c1; ++col) {
                if (!isSolidCell(map, col, row)) continue;
                float cellTop = tilemapCellOrigin(map, mt, col, row).y;
                float cellBottom = tilemapCellOrigin(map, mt, col, row + 1).y;
                float push;
                if (dir > 0.0f) push = cellTop - max.y;         // it was falling: goes up
                else if (dir < 0.0f) push = cellBottom - min.y; // it was rising: goes down
                else {
                    float toUp = cellTop - max.y, toDown = cellBottom - min.y;
                    push = std::fabs(toUp) < std::fabs(toDown) ? toUp : toDown;
                }
                if (std::fabs(push) > std::fabs(best)) best = push;
            }
        }
        return best;
    }

    // Cancels an axis' velocity only if it points AGAINST the push (if it is already
    // moving away from the obstacle, it is allowed to continue).
    void stopAgainst(float& velocity, float push) {
        if ((push < 0.0f && velocity > 0.0f) || (push > 0.0f && velocity < 0.0f)) velocity = 0.0f;
    }

    void resolveAgainstTilemap(const Tilemap& map, const Transform& mt, const BoxCollider& box,
                               Mirror mirror, Transform& t, RigidBody2D& rb) {
        Vec2 min, max;
        float newY = t.position.y;
        float dx = t.position.x - rb.previous.x; // how much it moved on each axis
        float dy = newY - rb.previous.y;

        // X pass, with the Y from BEFORE moving: at that height the only thing that can be
        // ahead is a wall, never the ground being stood on.
        t.position.y = rb.previous.y;
        boxOf(box, t, mirror, min, max);
        float pushX = pushOutX(map, mt, min, max, dx);
        if (pushX != 0.0f) {
            t.position.x += pushX;
            stopAgainst(rb.velocity.x, pushX);
        }

        // Y pass, with X already corrected.
        t.position.y = newY;
        boxOf(box, t, mirror, min, max);
        float pushY = pushOutY(map, mt, min, max, dy);
        if (pushY != 0.0f) {
            t.position.y += pushY;
            stopAgainst(rb.velocity.y, pushY);
            if (pushY < 0.0f) rb.grounded = true; // pushed UP: it is on the floor
        }
    }

    // One-way platform (its top edge at pMin.y) against the body 'body': it only lifts
    // it on top if it is falling and its feet were above the edge in the previous step.
    // The platform never moves (even if it has a RigidBody2D). A body that is dropping
    // off one (ignoreOneWay) passes through all of them.
    void resolveOneWay(World& world, Entity body, Vec2 pMin, Vec2 bMax) {
        RigidBody2D* rb = world.get<RigidBody2D>(body);
        if (!rb || rb->velocity.y < 0.0f || rb->ignoreOneWay > 0.0f) return; // rising: passes through it
        Transform& t = *world.get<Transform>(body);
        float previousBottom = bMax.y - (t.position.y - rb->previous.y);
        if (previousBottom > pMin.y + PhysicsSystem::ONE_WAY_TOLERANCE) return; // it came from below or from the side
        t.position.y -= bMax.y - pMin.y;
        stopAgainst(rb->velocity.y, -1.0f);
        rb->grounded = true;
    }

    // Separates two overlapping solid boxes along the axis of LEAST penetration. If
    // both are dynamic they share the push; a fixed obstacle does not move.
    void resolvePair(World& world, Entity a, Entity b) {
        Vec2 aMin, aMax, bMin, bMax;
        if (!colliderBounds(world, a, aMin, aMax) || !colliderBounds(world, b, bMin, bMax)) return;
        if (!boxesOverlap(aMin, aMax, bMin, bMax)) return;

        // With a one-way platform there is no axis of least penetration: you either get
        // on top or pass through it. Two one-way platforms do not touch.
        bool aOneWay = world.get<BoxCollider>(a)->oneWay;
        bool bOneWay = world.get<BoxCollider>(b)->oneWay;
        if (aOneWay && bOneWay) return;
        if (aOneWay) { resolveOneWay(world, b, aMin, bMax); return; }
        if (bOneWay) { resolveOneWay(world, a, bMin, aMax); return; }

        RigidBody2D* arb = world.get<RigidBody2D>(a);
        RigidBody2D* brb = world.get<RigidBody2D>(b);
        if (!arb && !brb) return; // two fixed obstacles: nothing to move
        float aShare = arb ? (brb ? 0.5f : 1.0f) : 0.0f;
        float bShare = 1.0f - aShare;

        Transform& at = *world.get<Transform>(a);
        Transform& bt = *world.get<Transform>(b);
        float dx = (bMin.x + bMax.x) * 0.5f - (aMin.x + aMax.x) * 0.5f;
        float dy = (bMin.y + bMax.y) * 0.5f - (aMin.y + aMax.y) * 0.5f;
        float px = std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x); // penetration in X
        float py = std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y); // ...and in Y

        if (px < py) {
            float pushA = dx > 0.0f ? -px : px; // 'a' moves away from 'b'
            at.position.x += pushA * aShare;
            bt.position.x -= pushA * bShare;
            if (arb) stopAgainst(arb->velocity.x, pushA);
            if (brb) stopAgainst(brb->velocity.x, -pushA);
        } else {
            float pushA = dy > 0.0f ? -py : py;
            at.position.y += pushA * aShare;
            bt.position.y -= pushA * bShare;
            if (arb) stopAgainst(arb->velocity.y, pushA);
            if (brb) stopAgainst(brb->velocity.y, -pushA);
            // The one left ON TOP (y = downward) is grounded.
            if (dy > 0.0f) { if (arb) arb->grounded = true; }
            else if (brb) brb->grounded = true;
        }
    }
}

bool colliderBounds(World& world, Entity e, Vec2& min, Vec2& max) {
    const BoxCollider* c = world.get<BoxCollider>(e);
    const Transform* t = world.get<Transform>(e);
    if (!c || !t) return false;
    boxOf(*c, *t, mirrorOf(world, e, *t), min, max);
    return true;
}

bool collidersOverlap(World& world, Entity a, Entity b, float margin) {
    Vec2 aMin, aMax, bMin, bMax;
    if (a == b || !colliderBounds(world, a, aMin, aMax) || !colliderBounds(world, b, bMin, bMax)) return false;
    aMin = { aMin.x - margin, aMin.y - margin };
    aMax = { aMax.x + margin, aMax.y + margin };
    return boxesOverlap(aMin, aMax, bMin, bMax);
}

bool isSolidArea(World& world, Vec2 min, Vec2 max) {
    bool solid = false;
    world.each<Tilemap, Transform>([&](Entity, Tilemap& map, Transform& mt) {
        int c0, r0, c1, r1;
        if (solid || !map.collision || !cellRange(map, mt, min, max, c0, r0, c1, r1)) return;
        for (int row = r0; row <= r1 && !solid; ++row)
            for (int col = c0; col <= c1 && !solid; ++col)
                solid = isSolidCell(map, col, row);
    });
    return solid;
}

namespace {
    // The solid boxes (the ones that block) of the World.
    std::vector<Entity> solidBoxes(World& world) {
        std::vector<Entity> solids;
        world.each<BoxCollider, Transform>([&](Entity e, BoxCollider& c, Transform&) {
            if (!c.isTrigger) solids.push_back(e);
        });
        return solids;
    }

    // What a body is standing on: nothing, only one-way platforms, or something solid
    // (a solid tile or a regular box; that one wins if there are both).
    enum class Ground { None, OneWayOnly, Solid };

    // Grounding probe: a thin strip under the body's feet, against the tilemaps and the
    // solid boxes. It is not checked while rising: brushing the edge of a platform while
    // jumping must not count as landing. While dropping off a one-way platform
    // (ignoreOneWay), those do not hold it up.
    Ground groundUnder(World& world, Entity e, const RigidBody2D& rb, const std::vector<Entity>& solids) {
        Vec2 min, max;
        const BoxCollider* box = world.get<BoxCollider>(e);
        if (rb.velocity.y < 0.0f || !box || box->isTrigger || !colliderBounds(world, e, min, max))
            return Ground::None;
        Vec2 probeMin{ min.x, max.y };
        Vec2 probeMax{ max.x, max.y + PhysicsSystem::GROUND_PROBE };
        if (isSolidArea(world, probeMin, probeMax)) return Ground::Solid;
        Ground found = Ground::None;
        for (Entity other : solids) {
            Vec2 oMin, oMax;
            if (other == e || !colliderBounds(world, other, oMin, oMax) ||
                !boxesOverlap({ probeMin.x + SKIN, probeMin.y }, { probeMax.x - SKIN, probeMax.y }, oMin, oMax))
                continue;
            if (!world.get<BoxCollider>(other)->oneWay) return Ground::Solid;
            // A one-way platform only holds if the feet are on its edge, not halfway
            // through it.
            if (rb.ignoreOneWay > 0.0f || max.y > oMin.y + 0.5f) continue;
            found = Ground::OneWayOnly;
        }
        return found;
    }

    bool probeGround(World& world, Entity e, const RigidBody2D& rb, const std::vector<Entity>& solids) {
        return groundUnder(world, e, rb, solids) != Ground::None;
    }
}

bool isOnOneWayOnly(World& world, Entity body) {
    const RigidBody2D* rb = world.get<RigidBody2D>(body);
    return rb && groundUnder(world, body, *rb, solidBoxes(world)) == Ground::OneWayOnly;
}

bool isStandingOn(World& world, Entity body, Entity platform) {
    const RigidBody2D* rb = world.get<RigidBody2D>(body);
    const BoxCollider* box = world.get<BoxCollider>(body);
    const BoxCollider* pbox = world.get<BoxCollider>(platform);
    if (body == platform || !rb || !rb->grounded || rb->velocity.y < 0.0f || !box || box->isTrigger ||
        !pbox || pbox->isTrigger)
        return false;
    Vec2 bMin, bMax, pMin, pMax;
    if (!colliderBounds(world, body, bMin, bMax) || !colliderBounds(world, platform, pMin, pMax)) return false;
    return bMin.x + SKIN < pMax.x && pMin.x < bMax.x - SKIN &&
           std::fabs(bMax.y - pMin.y) <= PhysicsSystem::GROUND_PROBE;
}

void moveCarrying(World& world, Entity platform, Vec2 delta) {
    Transform* t = world.get<Transform>(platform);
    if (!t || (delta.x == 0.0f && delta.y == 0.0f)) return;
    // The passengers are found BEFORE moving the platform (afterwards they no longer touch it).
    std::vector<Entity> riders;
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D&, Transform&) {
        if (isStandingOn(world, e, platform)) riders.push_back(e);
    });
    t->position.x += delta.x;
    t->position.y += delta.y;
    for (Entity e : riders) {
        Transform& rt = *world.get<Transform>(e);
        rt.position.x += delta.x;
        rt.position.y += delta.y;
    }
}

void PhysicsSystem::onPlay(World& world) {
    // Grounding is measured right at the start: otherwise, on the first frame every body
    // "is in the air" (a frame of falling animation and a landing sound when pressing Play).
    const std::vector<Entity> solids = solidBoxes(world);
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
        rb.previous = t.position;
        rb.ignoreOneWay = 0.0f;
        rb.grounded = probeGround(world, e, rb, solids);
    });
}

void PhysicsSystem::update(World& world, float dt) {
    // 1. Move the bodies. 'grounded' is recomputed on every step.
    world.each<RigidBody2D, Transform>([dt](Entity, RigidBody2D& rb, Transform& t) {
        rb.previous = t.position;
        rb.grounded = false;
        if (rb.ignoreOneWay > 0.0f) rb.ignoreOneWay = std::max(rb.ignoreOneWay - dt, 0.0f);
        rb.velocity.y += rb.gravity * dt;
        t.position.x += rb.velocity.x * dt;
        t.position.y += rb.velocity.y * dt;
    });

    // 2. Bodies against tilemaps. A trigger passes through the level.
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
        const BoxCollider* box = world.get<BoxCollider>(e);
        if (!box || box->isTrigger) return;
        const Mirror mirror = mirrorOf(world, e, t);
        world.each<Tilemap, Transform>([&](Entity m, Tilemap& map, Transform& mt) {
            if (m != e && map.collision) resolveAgainstTilemap(map, mt, *box, mirror, t, rb);
        });
    });

    // 3. Pairs of solid boxes (brute force: each pair once).
    const std::vector<Entity> solids = solidBoxes(world);
    for (std::size_t i = 0; i < solids.size(); ++i)
        for (std::size_t j = i + 1; j < solids.size(); ++j)
            resolvePair(world, solids[i], solids[j]);

    // 4. Grounding probe (see probeGround) for those not grounded by the separation.
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform&) {
        if (!rb.grounded) rb.grounded = probeGround(world, e, rb, solids);
    });
}
