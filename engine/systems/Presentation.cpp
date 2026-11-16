#include "systems/Presentation.h"

#include <algorithm>

#include "components/CoreComponents.h"
#include "components/PresentationComponents.h"
#include "core/World.h"
#include "systems/Tilemaps.h"

namespace {
    // The first living entity with that tag and a Transform (NullEntity if there is none).
    Entity findTagged(World& world, const std::string& tag) {
        Entity found = NullEntity;
        if (tag.empty()) return found;
        world.each<EntityInfo, Transform>([&](Entity e, EntityInfo& info, Transform&) {
            if (found == NullEntity && info.tag == tag) found = e;
        });
        return found;
    }

    // Rectangle covered by all the visible Tilemaps. false if there are none.
    bool tilemapsBounds(World& world, Vec2& min, Vec2& max) {
        bool any = false;
        world.each<Tilemap, Transform>([&](Entity, Tilemap& map, Transform& t) {
            if (!map.visible || map.cells.width == 0) return;
            Vec2 a, b;
            tilemapBounds(map, t, a, b);
            if (!any) { min = a; max = b; any = true; return; }
            min = { std::min(min.x, a.x), std::min(min.y, a.y) };
            max = { std::max(max.x, b.x), std::max(max.y, b.y) };
        });
        return any;
    }

    // Puts the camera center where the visible rectangle (half a view on each side)
    // fits within [min, max]. If the map is smaller than the view on an axis, on that axis
    // the camera centers on the map (otherwise the two limits would fight).
    float clampAxis(float center, float half, float min, float max) {
        if (max - min <= half * 2.0f) return (min + max) * 0.5f;
        return std::clamp(center, min + half, max - half);
    }

    // Moves 'current' toward 'target' by a fraction that depends on the speed and dt
    // (0 = instantly).
    float approach(float current, float target, float rate, float dt) {
        if (rate <= 0.0f) return target;
        float k = std::min(1.0f, rate * dt);
        return current + (target - current) * k;
    }
}

// --- Follow camera -----------------------------------------------------------------------

void FollowCameraSystem::onPlay(World& world) {
    world.each<FollowCamera>([](Entity, FollowCamera& f) {
        f.target = NullEntity;
        f.lookOffset = 0.0f;
        f.started = false;
    });
}

void FollowCameraSystem::update(World& world, float dt) {
    Vec2 mapMin, mapMax;
    const bool hasMap = tilemapsBounds(world, mapMin, mapMax);

    world.each<FollowCamera, Transform>([&](Entity self, FollowCamera& f, Transform& cam) {
        // The target is stored as an Entity and looked up again if it disappears.
        if (!world.isAlive(f.target)) f.target = findTagged(world, f.targetTag);
        const Transform* target = world.get<Transform>(f.target);
        if (!target) return;
        const Vec2 tp = target->position;

        // View lead: toward where the target MOVED in x. Standing still, it is
        // kept; when turning around it changes gradually, so the view does not jump.
        if (f.lookAhead != 0.0f && f.started) {
            float moved = tp.x - f.lastTargetX;
            float wanted = f.lookOffset;
            if (moved > 0.01f) wanted = f.lookAhead;
            else if (moved < -0.01f) wanted = -f.lookAhead;
            f.lookOffset = approach(f.lookOffset, wanted, f.lookAheadSpeed, dt);
        } else if (f.lookAhead == 0.0f) {
            f.lookOffset = 0.0f;
        }
        f.lastTargetX = tp.x;
        const Vec2 focus{ tp.x + f.lookOffset, tp.y };

        Vec2 next = cam.position;
        if (!f.started) {
            // First frame: straight onto the target, without sweeping across the level.
            next = focus;
            f.started = true;
        } else {
            // Dead zone: the camera only moves if the focus leaves the rectangle, and just
            // enough to put it back on its edge.
            Vec2 wanted = cam.position;
            const float hx = f.deadZone.x * 0.5f, hy = f.deadZone.y * 0.5f;
            if (focus.x > cam.position.x + hx) wanted.x = focus.x - hx;
            else if (focus.x < cam.position.x - hx) wanted.x = focus.x + hx;
            if (focus.y > cam.position.y + hy) wanted.y = focus.y - hy;
            else if (focus.y < cam.position.y - hy) wanted.y = focus.y + hy;
            next.x = approach(cam.position.x, wanted.x, f.smoothing, dt);
            next.y = approach(cam.position.y, wanted.y, f.smoothing, dt);
        }

        // The clamp goes last: however the destination is computed, the view does not
        // leave the map.
        if (f.clampToTilemaps && hasMap) {
            const Camera* c = world.get<Camera>(self);
            const float zoom = c && c->zoom > 0.0f ? c->zoom : 1.0f;
            next.x = clampAxis(next.x, viewWidth * 0.5f / zoom, mapMin.x, mapMax.x);
            next.y = clampAxis(next.y, viewHeight * 0.5f / zoom, mapMin.y, mapMax.y);
        }
        cam.position = next;
    });
}

// --- Parallax ----------------------------------------------------------------------------

void ParallaxSystem::onPlay(World& world) {
    world.each<ParallaxBackground>([](Entity, ParallaxBackground& p) { p.offset = { 0.0f, 0.0f }; });
}

void ParallaxSystem::update(World& world, float dt) {
    world.each<ParallaxBackground>([dt](Entity, ParallaxBackground& p) {
        p.offset.x += p.scroll.x * dt;
        p.offset.y += p.scroll.y * dt;
    });
}
