#include "systems/Motion.h"

#include <algorithm>
#include <cmath>

#include "components/CoreComponents.h"
#include "components/MotionComponents.h"
#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "systems/Physics.h"
#include "systems/Rules.h"

namespace {
    // Px que los pies pueden estar ya DENTRO del trampolin en el paso anterior y aun
    // contar como "venia de arriba".
    const float BOUNCE_TOLERANCE = 4.0f;

    // Punto i del recorrido: 0 es el origen, los demas se miden desde el.
    Vec2 pathPoint(const PathMover& m, int i) {
        if (i <= 0) return m.origin;
        const Vec2& o = m.points[i - 1].offset;
        return { m.origin.x + o.x, m.origin.y + o.y };
    }

    // Pasa al punto siguiente: en circuito, del ultimo al primero; si no, rebota en
    // los extremos.
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

    // Si la caja de 'e', llevada a 'position', se superpone con algun cuerpo (para no
    // hacer aparecer una plataforma encima del jugador).
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

// --- Recorridos --------------------------------------------------------------------------

void PathMoverSystem::onPlay(World& world) {
    world.each<PathMover>([](Entity, PathMover& m) { m.started = false; });
}

void PathMoverSystem::update(World& world, float dt) {
    world.each<PathMover, Transform>([&](Entity e, PathMover& m, Transform& t) {
        // Arranque perezoso (vale tambien para una entidad creada jugando).
        if (!m.started) {
            m.started = true;
            m.origin = t.position;
            m.target = 1;
            m.step = 1;
            m.wait = 0.0f;
        }
        const int count = (int)m.points.size() + 1; // con el origen
        if (count < 2 || m.speed <= 0.0f) return;
        if (m.target >= count) m.target = count - 1;

        // La pausa consume tiempo; lo que sobra de ella ya es camino.
        float distance = m.speed * dt;
        if (m.wait > 0.0f) {
            m.wait -= dt;
            if (m.wait > 0.0f) return;
            distance = m.speed * -m.wait;
            m.wait = 0.0f;
        }

        // Avanza 'distance' por el recorrido; si llega a un punto con camino de sobra
        // (sin pausa, o un frame lento), sigue hacia el siguiente.
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

// --- Plataformas que caen ----------------------------------------------------------------

void FallingPlatformSystem::onPlay(World& world) {
    world.each<FallingPlatform>([](Entity, FallingPlatform& f) {
        f.started = f.triggered = f.falling = false;
        f.timer = f.speed = 0.0f;
    });
}

void FallingPlatformSystem::update(World& world, float dt) {
    world.each<FallingPlatform, Transform>([&](Entity e, FallingPlatform& f, Transform& t) {
        if (!f.started) {
            f.started = true;
            f.origin = t.position;
        }
        // Quieta hasta que alguien se pare encima.
        if (!f.triggered) {
            if (hasRider(world, e)) {
                f.triggered = true;
                f.timer = 0.0f;
            }
            return;
        }

        f.timer += dt;
        if (!f.falling) {
            if (f.timer < f.delay) return; // aviso: se le apaga la helice
            f.falling = true;
            f.timer = 0.0f;
            f.speed = 0.0f;
        }

        // De vuelta a su lugar, si no hay nadie ahi (si lo hay, espera abajo).
        if (f.respawnTime > 0.0f && f.timer >= f.respawnTime) {
            if (bodyAt(world, e, f.origin)) return;
            t.position = f.origin;
            f.triggered = f.falling = false;
            f.timer = f.speed = 0.0f;
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
        b.justBounced = false; // el aviso dura un frame
        Vec2 sMin, sMax;
        if (!colliderBounds(world, self, sMin, sMax)) return;

        world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
            if (e == self || rb.gravity <= 0.0f || isDead(world, e)) return;
            if (!b.targetTag.empty()) {
                const EntityInfo* info = world.get<EntityInfo>(e);
                if (!info || info->tag != b.targetTag) return;
            }
            // Solo lo que BAJO en este paso y venia de arriba: entrar de costado o
            // quedarse parado dentro no lanza.
            Vec2 bMin, bMax;
            if (t.position.y <= rb.previous.y || !collidersOverlap(world, self, e) ||
                !colliderBounds(world, e, bMin, bMax))
                return;
            float previousBottom = bMax.y - (t.position.y - rb.previous.y);
            if (previousBottom > sMin.y + BOUNCE_TOLERANCE) return;

            // Altura en px -> velocidad, como el salto del PlatformerMotor.
            rb.velocity.y = -std::sqrt(2.0f * rb.gravity * b.bounceHeight);
            rb.grounded = false;
            // Un salto guardado en el buffer o el coyote time no deben pisar el rebote
            // (el salto propio es mas bajo), y soltar el boton no lo corta.
            if (PlatformerMotor* m = world.get<PlatformerMotor>(e)) {
                m->coyote = 0.0f;
                m->jumping = false;
                m->airJumpsLeft = m->maxAirJumps;
            }
            b.justBounced = true;
        });
    });
}
