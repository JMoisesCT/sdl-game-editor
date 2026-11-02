#include "PlatformerRules.h"

#include <SDL3/SDL.h>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "components/RuleComponents.h"
#include "core/Reflection.h"
#include "core/World.h"
#include "systems/Physics.h"
#include "systems/Rules.h"

static const char* TAG_PICKUP = "Pickup";

void registerPlatformerComponents(ComponentRegistry& registry) {
    registry.add<LevelGoal>("LevelGoal")
        .field("targetTag", &LevelGoal::targetTag).tooltip("Etiqueta de quien completa el nivel al tocarla");
}

void PlatformerRulesSystem::onPlay(World& world) {
    collected = total = 0;
    done = false;
    world.each<Collectible, EntityInfo>([&](Entity, Collectible&, EntityInfo& info) {
        if (info.tag == TAG_PICKUP) ++total;
    });
    world.each<LevelGoal>([](Entity, LevelGoal& goal) { goal.reached = false; });
}

void PlatformerRulesSystem::update(World& world, float) {
    // Frutas recogidas en este frame (aun existen: se destruyen al final del frame).
    world.each<Collectible, EntityInfo>([&](Entity, Collectible& c, EntityInfo& info) {
        if (!c.justCollected || info.tag != TAG_PICKUP) return;
        ++collected;
        SDL_Log("Frutas: %d/%d", collected, total);
    });

    // Meta: la toca alguien vivo con la etiqueta pedida.
    world.each<LevelGoal, BoxCollider>([&](Entity self, LevelGoal& goal, BoxCollider&) {
        if (goal.reached) return;
        world.each<BoxCollider, EntityInfo>([&](Entity other, BoxCollider&, EntityInfo& info) {
            if (goal.reached || info.tag != goal.targetTag || isDead(world, other)) return;
            if (!collidersOverlap(world, self, other, RULE_CONTACT_MARGIN)) return;
            goal.reached = true;
            if (!done) SDL_Log("NIVEL COMPLETADO (frutas: %d/%d)", collected, total);
            done = true;
        });
    });
}
