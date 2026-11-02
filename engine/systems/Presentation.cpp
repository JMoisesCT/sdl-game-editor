#include "systems/Presentation.h"

#include <algorithm>

#include "components/CoreComponents.h"
#include "components/PresentationComponents.h"
#include "core/World.h"
#include "systems/Tilemaps.h"

namespace {
    // La primera entidad viva con esa etiqueta y Transform (NullEntity si no hay).
    Entity findTagged(World& world, const std::string& tag) {
        Entity found = NullEntity;
        if (tag.empty()) return found;
        world.each<EntityInfo, Transform>([&](Entity e, EntityInfo& info, Transform&) {
            if (found == NullEntity && info.tag == tag) found = e;
        });
        return found;
    }

    // Rectangulo que cubren todos los Tilemap visibles. false si no hay ninguno.
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

    // Deja el centro de la camara donde el rectangulo visible (media vista a cada lado)
    // quepa en [min, max]. Si el mapa es mas chico que la vista en un eje, en ese eje
    // la camara se centra en el mapa (si no, los dos topes se pelearian).
    float clampAxis(float center, float half, float min, float max) {
        if (max - min <= half * 2.0f) return (min + max) * 0.5f;
        return std::clamp(center, min + half, max - half);
    }

    // Acerca 'current' a 'target' una fraccion que depende de la rapidez y del dt
    // (0 = de golpe).
    float approach(float current, float target, float rate, float dt) {
        if (rate <= 0.0f) return target;
        float k = std::min(1.0f, rate * dt);
        return current + (target - current) * k;
    }
}

// --- Camara que sigue --------------------------------------------------------------------

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
        // El objetivo se guarda como Entity y se vuelve a buscar si desaparece.
        if (!world.isAlive(f.target)) f.target = findTagged(world, f.targetTag);
        const Transform* target = world.get<Transform>(f.target);
        if (!target) return;
        const Vec2 tp = target->position;

        // Adelanto de la vista: hacia donde se MOVIO el objetivo en x. Quieto, se
        // mantiene; al dar la vuelta cambia de a poco, para que la vista no salte.
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
            // Primer frame: directo sobre el objetivo, sin recorrer el nivel.
            next = focus;
            f.started = true;
        } else {
            // Zona muerta: la camara solo se mueve si el foco sale del rectangulo, y lo
            // justo para dejarlo otra vez sobre su borde.
            Vec2 wanted = cam.position;
            const float hx = f.deadZone.x * 0.5f, hy = f.deadZone.y * 0.5f;
            if (focus.x > cam.position.x + hx) wanted.x = focus.x - hx;
            else if (focus.x < cam.position.x - hx) wanted.x = focus.x + hx;
            if (focus.y > cam.position.y + hy) wanted.y = focus.y - hy;
            else if (focus.y < cam.position.y - hy) wanted.y = focus.y + hy;
            next.x = approach(cam.position.x, wanted.x, f.smoothing, dt);
            next.y = approach(cam.position.y, wanted.y, f.smoothing, dt);
        }

        // El recorte va al final: se calcule como se calcule el destino, la vista no
        // se sale del mapa.
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
