#include "PlatformerRules.h"

#include <SDL3/SDL.h>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "components/PresentationComponents.h"
#include "components/RuleComponents.h"
#include "core/Reflection.h"
#include "core/World.h"
#include "systems/Animation.h"
#include "systems/Physics.h"
#include "systems/Rules.h"

static const char* TAG_PLAYER = "Player";
static const char* TAG_PICKUP = "Pickup";
static const char* TAG_HUD_LIVES = "HudLives";
static const char* TAG_HUD_FRUITS = "HudFruits";
static const char* TAG_HUD_BANNER = "HudBanner";

void registerPlatformerComponents(ComponentRegistry& registry) {
    registry.add<LevelGoal>("LevelGoal")
        .field("targetTag", &LevelGoal::targetTag).tooltip("Etiqueta de quien completa el nivel al tocarla");
}

void registerPlatformerConditions(AnimationConditions& conditions) {
    conditions.add("goalReached", [](World& w, Entity e) {
        const LevelGoal* goal = w.get<LevelGoal>(e);
        return goal && goal->reached;
    });
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
    // Frutas recogidas en este frame (aun existen: se destruyen al final del frame, o
    // al terminar su animacion).
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

    updateHud(world);
}

void PlatformerRulesSystem::updateHud(World& world) {
    // La vida del primer jugador con Health.
    const Health* hp = nullptr;
    world.each<Health, EntityInfo>([&](Entity, Health& h, EntityInfo& info) {
        if (!hp && info.tag == TAG_PLAYER) hp = &h;
    });

    // Cada dato en su propio Text: si fueran una sola linea, al cambiar de largo se
    // correria todo lo demas. Asignar el mismo texto no cuesta nada: la imagen de cada
    // texto se cachea al dibujar. Dos espacios tras ':' porque en NormalFont.ttf el
    // espacio es muy angosto (4 px a 32 pt).
    world.each<Text, EntityInfo>([&](Entity, Text& text, EntityInfo& info) {
        if (info.tag == TAG_HUD_LIVES && hp)
            text.text = "VIDA:  " + std::to_string(hp->hp) + "/" + std::to_string(hp->maxHP);
        else if (info.tag == TAG_HUD_FRUITS)
            text.text = "FRUTAS:  " + std::to_string(collected) + "/" + std::to_string(total);
        else if (info.tag == TAG_HUD_BANNER)
            text.visible = done;
    });
}
