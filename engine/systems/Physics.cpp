#include "systems/Physics.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "core/World.h"
#include "systems/Tilemaps.h"

namespace {
    // Margen que se le quita a una caja al buscar que celdas o cajas toca. Sin el, un
    // cuerpo apoyado EXACTAMENTE sobre el suelo contaria tambien la fila de abajo (sus
    // bordes coinciden) y se resolverian choques que no existen.
    const float SKIN = 0.01f;

    void boxOf(const BoxCollider& c, const Transform& t, Vec2& min, Vec2& max) {
        float sx = std::fabs(t.scale.x), sy = std::fabs(t.scale.y);
        float cx = t.position.x + c.offset.x * sx;
        float cy = t.position.y + c.offset.y * sy;
        float hw = std::fabs(c.size.x) * sx * 0.5f;
        float hh = std::fabs(c.size.y) * sy * 0.5f;
        min = { cx - hw, cy - hh };
        max = { cx + hw, cy + hh };
    }

    bool boxesOverlap(Vec2 aMin, Vec2 aMax, Vec2 bMin, Vec2 bMax) {
        return aMin.x < bMax.x && bMin.x < aMax.x && aMin.y < bMax.y && bMin.y < aMax.y;
    }

    // Celdas del mapa que toca el rectangulo (con el margen SKIN), recortadas a la
    // grilla. false si no toca ninguna.
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

    // Cuanto mover la caja en X para sacarla de las celdas solidas que toca. 'dir' es
    // hacia donde se movio en X (>0, <0, o 0 si no se movio).
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
                if (dir > 0.0f) push = cellLeft - max.x;       // venia hacia la derecha
                else if (dir < 0.0f) push = cellRight - min.x; // venia hacia la izquierda
                else {
                    // No se movio en X y aun asi se superpone (aparecio dentro de una
                    // pared, se movio el mapa...): sale por el lado mas corto.
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
                if (dir > 0.0f) push = cellTop - max.y;         // caia: sube
                else if (dir < 0.0f) push = cellBottom - min.y; // subia: baja
                else {
                    float toUp = cellTop - max.y, toDown = cellBottom - min.y;
                    push = std::fabs(toUp) < std::fabs(toDown) ? toUp : toDown;
                }
                if (std::fabs(push) > std::fabs(best)) best = push;
            }
        }
        return best;
    }

    // Anula la velocidad de un eje solo si apunta CONTRA el empujon (si ya se aleja
    // del obstaculo, se le deja seguir).
    void stopAgainst(float& velocity, float push) {
        if ((push < 0.0f && velocity > 0.0f) || (push > 0.0f && velocity < 0.0f)) velocity = 0.0f;
    }

    void resolveAgainstTilemap(const Tilemap& map, const Transform& mt, const BoxCollider& box,
                               Transform& t, RigidBody2D& rb) {
        Vec2 min, max;
        float newY = t.position.y;
        float dx = t.position.x - rb.previous.x; // cuanto se movio en cada eje
        float dy = newY - rb.previous.y;

        // Pasada X, con la Y de ANTES de moverse: a esa altura lo unico que puede haber
        // delante es una pared, nunca el suelo que se esta pisando.
        t.position.y = rb.previous.y;
        boxOf(box, t, min, max);
        float pushX = pushOutX(map, mt, min, max, dx);
        if (pushX != 0.0f) {
            t.position.x += pushX;
            stopAgainst(rb.velocity.x, pushX);
        }

        // Pasada Y, ya con la X corregida.
        t.position.y = newY;
        boxOf(box, t, min, max);
        float pushY = pushOutY(map, mt, min, max, dy);
        if (pushY != 0.0f) {
            t.position.y += pushY;
            stopAgainst(rb.velocity.y, pushY);
            if (pushY < 0.0f) rb.grounded = true; // empujado hacia ARRIBA: esta en el piso
        }
    }

    // Separa dos cajas solidas que se superponen, por el eje de MENOR penetracion. Si
    // las dos son dinamicas se reparten el empujon; un obstaculo fijo no se mueve.
    void resolvePair(World& world, Entity a, Entity b) {
        Vec2 aMin, aMax, bMin, bMax;
        if (!colliderBounds(world, a, aMin, aMax) || !colliderBounds(world, b, bMin, bMax)) return;
        if (!boxesOverlap(aMin, aMax, bMin, bMax)) return;

        RigidBody2D* arb = world.get<RigidBody2D>(a);
        RigidBody2D* brb = world.get<RigidBody2D>(b);
        if (!arb && !brb) return; // dos obstaculos fijos: nada que mover
        float aShare = arb ? (brb ? 0.5f : 1.0f) : 0.0f;
        float bShare = 1.0f - aShare;

        Transform& at = *world.get<Transform>(a);
        Transform& bt = *world.get<Transform>(b);
        float dx = (bMin.x + bMax.x) * 0.5f - (aMin.x + aMax.x) * 0.5f;
        float dy = (bMin.y + bMax.y) * 0.5f - (aMin.y + aMax.y) * 0.5f;
        float px = std::min(aMax.x, bMax.x) - std::max(aMin.x, bMin.x); // penetracion en X
        float py = std::min(aMax.y, bMax.y) - std::max(aMin.y, bMin.y); // ...y en Y

        if (px < py) {
            float pushA = dx > 0.0f ? -px : px; // 'a' se aleja de 'b'
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
            // El que queda ENCIMA (y = hacia abajo) queda apoyado.
            if (dy > 0.0f) { if (arb) arb->grounded = true; }
            else if (brb) brb->grounded = true;
        }
    }
}

bool colliderBounds(World& world, Entity e, Vec2& min, Vec2& max) {
    const BoxCollider* c = world.get<BoxCollider>(e);
    const Transform* t = world.get<Transform>(e);
    if (!c || !t) return false;
    boxOf(*c, *t, min, max);
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
    // Las cajas solidas (las que frenan) del World.
    std::vector<Entity> solidBoxes(World& world) {
        std::vector<Entity> solids;
        world.each<BoxCollider, Transform>([&](Entity e, BoxCollider& c, Transform&) {
            if (!c.isTrigger) solids.push_back(e);
        });
        return solids;
    }

    // Sonda de apoyo: una franja fina bajo los pies del cuerpo, contra los tilemaps y
    // las cajas solidas. Mientras sube no se mira: rozar el borde de una plataforma al
    // saltar no debe contar como aterrizar.
    bool probeGround(World& world, Entity e, const RigidBody2D& rb, const std::vector<Entity>& solids) {
        Vec2 min, max;
        const BoxCollider* box = world.get<BoxCollider>(e);
        if (rb.velocity.y < 0.0f || !box || box->isTrigger || !colliderBounds(world, e, min, max)) return false;
        Vec2 probeMin{ min.x, max.y };
        Vec2 probeMax{ max.x, max.y + PhysicsSystem::GROUND_PROBE };
        if (isSolidArea(world, probeMin, probeMax)) return true;
        for (Entity other : solids) {
            Vec2 oMin, oMax;
            if (other != e && colliderBounds(world, other, oMin, oMax) &&
                boxesOverlap({ probeMin.x + SKIN, probeMin.y }, { probeMax.x - SKIN, probeMax.y }, oMin, oMax))
                return true;
        }
        return false;
    }
}

void PhysicsSystem::onPlay(World& world) {
    // El apoyo se mide ya al empezar: si no, el primer frame todo cuerpo "esta en el
    // aire" (un frame de animacion de caida y un aterrizaje que suena al pulsar Jugar).
    const std::vector<Entity> solids = solidBoxes(world);
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
        rb.previous = t.position;
        rb.grounded = probeGround(world, e, rb, solids);
    });
}

void PhysicsSystem::update(World& world, float dt) {
    // 1. Mover los cuerpos. 'grounded' se vuelve a calcular en cada paso.
    world.each<RigidBody2D, Transform>([dt](Entity, RigidBody2D& rb, Transform& t) {
        rb.previous = t.position;
        rb.grounded = false;
        rb.velocity.y += rb.gravity * dt;
        t.position.x += rb.velocity.x * dt;
        t.position.y += rb.velocity.y * dt;
    });

    // 2. Cuerpos contra tilemaps. Un trigger atraviesa el nivel.
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform& t) {
        const BoxCollider* box = world.get<BoxCollider>(e);
        if (!box || box->isTrigger) return;
        world.each<Tilemap, Transform>([&](Entity m, Tilemap& map, Transform& mt) {
            if (m != e && map.collision) resolveAgainstTilemap(map, mt, *box, t, rb);
        });
    });

    // 3. Pares de cajas solidas (fuerza bruta: cada par una vez).
    const std::vector<Entity> solids = solidBoxes(world);
    for (std::size_t i = 0; i < solids.size(); ++i)
        for (std::size_t j = i + 1; j < solids.size(); ++j)
            resolvePair(world, solids[i], solids[j]);

    // 4. Sonda de apoyo (ver probeGround) para los que no quedaron apoyados al separarlos.
    world.each<RigidBody2D, Transform>([&](Entity e, RigidBody2D& rb, Transform&) {
        if (!rb.grounded) rb.grounded = probeGround(world, e, rb, solids);
    });
}
