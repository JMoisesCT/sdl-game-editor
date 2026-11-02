#include "systems/Rules.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "components/RuleComponents.h"
#include "core/World.h"
#include "systems/Physics.h"

namespace {
    // Las entidades con BoxCollider y cierta etiqueta, buscadas una vez por etiqueta y
    // por frame (y no una vez por cada pincho o cada fruta).
    class TaggedBoxes {
    public:
        explicit TaggedBoxes(World& world) : world(world) {}

        const std::vector<Entity>& get(const std::string& tag) {
            auto it = byTag.find(tag);
            if (it != byTag.end()) return it->second;
            std::vector<Entity>& list = byTag[tag];
            if (!tag.empty())
                world.each<BoxCollider, EntityInfo>([&](Entity e, BoxCollider&, EntityInfo& info) {
                    if (info.tag == tag) list.push_back(e);
                });
            return list;
        }

    private:
        World& world;
        std::unordered_map<std::string, std::vector<Entity>> byTag;
    };

    // Llena la vida la primera vez que se la mira (tambien vale para una entidad
    // creada jugando, que no paso por onPlay).
    void startHealth(Health& h) {
        if (h.started) return;
        h.hp = h.maxHP;
        h.started = true;
    }

    void setSpriteVisible(World& world, Entity e, bool visible) {
        if (Sprite* s = world.get<Sprite>(e)) s->visible = visible;
    }

    void die(World& world, Entity e, Health& h) {
        h.hp = 0;
        h.invulnerable = 0.0f;
        h.justDied = true;
        setSpriteVisible(world, e, true); // que no quede apagado a mitad de un parpadeo
        if (h.destroyOnDeath && !world.has<Respawn>(e)) world.destroy(e);
    }
}

// --- Vida --------------------------------------------------------------------------------

bool damageEntity(World& world, Entity target, int amount) {
    Health* h = world.get<Health>(target);
    if (!h || amount <= 0) return false;
    startHealth(*h);
    if (h->hp <= 0 || h->invulnerable > 0.0f) return false; // muerto o en periodo de gracia

    h->hp = amount >= h->hp ? 0 : h->hp - amount;
    h->justDamaged = true;
    if (h->hp == 0) {
        die(world, target, *h);
    } else {
        h->invulnerable = h->invulnerabilityTime;
        h->blinkTimer = 0.0f;
    }
    return true;
}

void killEntity(World& world, Entity target) {
    Health* h = world.get<Health>(target);
    if (!h) return;
    startHealth(*h);
    if (h->hp > 0) die(world, target, *h);
}

void healEntity(World& world, Entity target, int amount) {
    Health* h = world.get<Health>(target);
    if (!h || amount <= 0) return;
    startHealth(*h);
    if (h->hp <= 0) return;
    h->hp = h->hp + amount > h->maxHP ? h->maxHP : h->hp + amount;
}

bool isDead(World& world, Entity e) {
    const Health* h = world.get<Health>(e);
    return h && h->started && h->hp <= 0;
}

void HealthSystem::onPlay(World& world) {
    world.each<Health>([](Entity, Health& h) {
        h.started = false;
        startHealth(h);
        h.invulnerable = h.blinkTimer = 0.0f;
        h.justDamaged = h.justDied = false;
    });
}

void HealthSystem::update(World& world, float dt) {
    world.each<Health>([&](Entity e, Health& h) {
        startHealth(h);
        h.justDamaged = h.justDied = false; // los avisos duran un frame
        if (h.invulnerable <= 0.0f) return;

        h.invulnerable -= dt;
        if (h.invulnerable <= 0.0f) {
            h.invulnerable = 0.0f;
            setSpriteVisible(world, e, true); // al acabar la gracia, siempre visible
            return;
        }
        // Parpadeo: apagar y encender el sprite a intervalos fijos mientras dure.
        h.blinkTimer += dt;
        if (h.blinkTimer >= h.blinkInterval) {
            h.blinkTimer -= h.blinkInterval;
            if (Sprite* s = world.get<Sprite>(e)) s->visible = !s->visible;
        }
    });
}

// --- Peligros ----------------------------------------------------------------------------

void HazardSystem::onPlay(World& world) {
    world.each<Hazard>([](Entity, Hazard& hz) { hz.used = false; });
}

void HazardSystem::update(World& world, float) {
    TaggedBoxes targets(world);
    world.each<Hazard, BoxCollider>([&](Entity self, Hazard& hz, BoxCollider&) {
        if (hz.used && !hz.repeat) return;
        for (Entity t : targets.get(hz.targetTag)) {
            if (!collidersOverlap(world, self, t, RULE_CONTACT_MARGIN)) continue;
            // damageEntity ya descarta a los invulnerables: el empujon solo va con un
            // golpe que cuenta (si no, el jugador saldria rebotado cada frame).
            if (!damageEntity(world, t, hz.damage)) continue;
            hz.used = true;

            // Empujon hacia el lado contrario al peligro. Justo encima (misma x), hacia
            // la derecha, para no dejarlo caer en vertical sobre el mismo peligro.
            RigidBody2D* rb = world.get<RigidBody2D>(t);
            const Transform* tt = world.get<Transform>(t);
            const Transform* st = world.get<Transform>(self);
            if (rb && tt && st) {
                float dir = tt->position.x >= st->position.x ? 1.0f : -1.0f;
                rb->velocity = { dir * hz.knockback.x, -hz.knockback.y }; // -y = hacia arriba
            }
            if (!hz.repeat) break;
        }
    });
}

void KillZoneSystem::update(World& world, float) {
    TaggedBoxes targets(world);
    world.each<KillZone, BoxCollider>([&](Entity self, KillZone& kz, BoxCollider&) {
        for (Entity t : targets.get(kz.targetTag))
            if (collidersOverlap(world, self, t, RULE_CONTACT_MARGIN)) killEntity(world, t);
    });
}

// --- Objetos que se recogen --------------------------------------------------------------

void CollectibleSystem::onPlay(World& world) {
    world.each<Collectible>([](Entity, Collectible& c) {
        c.collected = c.justCollected = false;
        c.collector = NullEntity;
    });
}

void CollectibleSystem::update(World& world, float) {
    TaggedBoxes collectors(world);
    world.each<Collectible, BoxCollider>([&](Entity self, Collectible& c, BoxCollider&) {
        c.justCollected = false;
        if (c.collected) return;
        for (Entity t : collectors.get(c.collectorTag)) {
            if (isDead(world, t) || !collidersOverlap(world, self, t, RULE_CONTACT_MARGIN)) continue;
            c.collected = c.justCollected = true;
            c.collector = t;
            // Destruir es diferido: el resto de los sistemas de este frame aun ven la
            // entidad (y su justCollected).
            if (c.destroyOnCollect) world.destroy(self);
            break;
        }
    });
}

// --- Checkpoints y reaparicion -----------------------------------------------------------

void CheckpointSystem::onPlay(World& world) {
    world.each<Checkpoint>([](Entity, Checkpoint& cp) { cp.activated = cp.justActivated = false; });
}

void CheckpointSystem::update(World& world, float) {
    TaggedBoxes targets(world);
    world.each<Checkpoint, BoxCollider>([&](Entity self, Checkpoint& cp, BoxCollider&) {
        cp.justActivated = false;
        const Transform* st = world.get<Transform>(self);
        if (cp.activated || !st) return;
        for (Entity t : targets.get(cp.targetTag)) {
            Respawn* r = world.get<Respawn>(t);
            if (!r || isDead(world, t) || !collidersOverlap(world, self, t, RULE_CONTACT_MARGIN)) continue;
            cp.activated = cp.justActivated = true;
            // El offset, como el del BoxCollider, va en px de la imagen x escala.
            r->point = { st->position.x + cp.offset.x * std::fabs(st->scale.x),
                         st->position.y + cp.offset.y * std::fabs(st->scale.y) };
            r->started = true; // que RespawnSystem no lo pise con la posicion inicial
            if (!cp.activeTexture.empty())
                if (Sprite* s = world.get<Sprite>(self)) s->texture = cp.activeTexture;
            break;
        }
    });
}

void RespawnSystem::onPlay(World& world) {
    world.each<Respawn, Transform>([](Entity, Respawn& r, Transform& t) {
        r.point = t.position; // sin checkpoints, reaparece donde empezo
        r.started = true;
        r.dying = r.justRespawned = false;
        r.timer = 0.0f;
    });
}

void RespawnSystem::update(World& world, float dt) {
    world.each<Respawn, Transform>([&](Entity e, Respawn& r, Transform& t) {
        r.justRespawned = false;
        if (!r.started) {
            r.point = t.position;
            r.started = true;
        }
        if (!r.dying && isDead(world, e)) {
            r.dying = true;
            r.timer = r.delay;
        }
        if (!r.dying) return;

        // MUERTE EN DOS TIEMPOS: congelado mientras corre la cuenta (se ve la caida o
        // el golpe), y despues de vuelta al punto.
        RigidBody2D* rb = world.get<RigidBody2D>(e);
        if (rb) rb->velocity = { 0.0f, 0.0f };
        r.timer -= dt;
        if (r.timer > 0.0f) return;

        t.position = r.point;
        if (rb) {
            // La posicion anterior tambien: si no, la fisica creeria que el cuerpo hizo
            // un viaje enorme en un frame y lo separaria mal de las paredes.
            rb->previous = r.point;
            rb->grounded = false;
        }
        if (Health* h = world.get<Health>(e)) {
            h->hp = h->maxHP;
            h->started = true;
            h->invulnerable = h->blinkTimer = 0.0f;
        }
        setSpriteVisible(world, e, true);
        r.dying = false;
        r.justRespawned = true;
    });
}
