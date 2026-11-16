#include "systems/Conditions.h"

#include <cmath>

#include "components/MotionComponents.h"
#include "components/PhysicsComponents.h"
#include "components/RuleComponents.h"
#include "core/World.h"
#include "systems/Rules.h"

const Condition* ConditionRegistry::find(const std::string& name) const {
    auto it = conditions.find(name);
    return it == conditions.end() ? nullptr : &it->second;
}

std::vector<std::string> ConditionRegistry::names() const {
    std::vector<std::string> out;
    for (const auto& [name, condition] : conditions) out.push_back(name);
    return out; // std::map already keeps them sorted
}

namespace {
    // In the air: it has a body with gravity and is not grounded. A body without gravity
    // (top-down, a ship) is never "in the air". The PlatformerMotor's grounding, if
    // any, is the one smoothed by coyote time: it does not flicker at the edges.
    bool isAirborne(World& w, Entity e) {
        const RigidBody2D* rb = w.get<RigidBody2D>(e);
        if (!rb || rb->gravity == 0.0f) return false;
        if (const PlatformerMotor* m = w.get<PlatformerMotor>(e)) return !m->grounded;
        return !rb->grounded;
    }

    float verticalSpeed(World& w, Entity e) {
        const RigidBody2D* rb = w.get<RigidBody2D>(e);
        return rb ? rb->velocity.y : 0.0f;
    }
}

void registerEngineConditions(ConditionRegistry& c) {
    // --- States ---
    c.add("grounded", [](World& w, Entity e) { return !isAirborne(w, e); });
    c.add("airborne", [](World& w, Entity e) { return isAirborne(w, e); });
    // y grows downward: going up is a negative velocity.
    c.add("rising", [](World& w, Entity e) { return isAirborne(w, e) && verticalSpeed(w, e) < 0.0f; });
    c.add("falling", [](World& w, Entity e) { return isAirborne(w, e) && verticalSpeed(w, e) > 0.0f; });
    c.add("moving", [](World& w, Entity e) {
        const RigidBody2D* rb = w.get<RigidBody2D>(e);
        return rb && std::hypot(rb->velocity.x, rb->velocity.y) > 1.0f;
    });
    c.add("dead", [](World& w, Entity e) { return isDead(w, e); });
    c.add("hurt", [](World& w, Entity e) {
        const Health* h = w.get<Health>(e);
        return h && h->hp > 0 && h->invulnerable > 0.0f;
    });
    c.add("collected", [](World& w, Entity e) {
        const Collectible* col = w.get<Collectible>(e);
        return col && col->collected;
    });
    c.add("activated", [](World& w, Entity e) {
        const Checkpoint* cp = w.get<Checkpoint>(e);
        return cp && cp->activated;
    });
    // A FallingPlatform that was stepped on: about to fall or falling.
    c.add("crumbling", [](World& w, Entity e) {
        const FallingPlatform* f = w.get<FallingPlatform>(e);
        return f && f->triggered;
    });

    // --- One-frame events ---
    c.add("jumped", [](World& w, Entity e) {
        const PlatformerMotor* m = w.get<PlatformerMotor>(e);
        return m && m->justJumped;
    });
    c.add("landed", [](World& w, Entity e) {
        const PlatformerMotor* m = w.get<PlatformerMotor>(e);
        return m && m->justLanded;
    });
    c.add("dropped", [](World& w, Entity e) {
        const PlatformerMotor* m = w.get<PlatformerMotor>(e);
        return m && m->justDropped;
    });
    c.add("damaged", [](World& w, Entity e) {
        const Health* h = w.get<Health>(e);
        return h && h->justDamaged;
    });
    c.add("respawned", [](World& w, Entity e) {
        const Respawn* r = w.get<Respawn>(e);
        return r && r->justRespawned;
    });
    c.add("bounced", [](World& w, Entity e) {
        const Bouncer* b = w.get<Bouncer>(e);
        return b && b->justBounced;
    });
}
