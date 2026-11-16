#include "PlatformerRules.h"

#include <SDL3/SDL.h>

#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "components/PresentationComponents.h"
#include "components/RuleComponents.h"
#include "core/Reflection.h"
#include "core/World.h"
#include "systems/Conditions.h"
#include "systems/Physics.h"
#include "systems/Rules.h"

static const char* TAG_PLAYER = "Player";
static const char* TAG_PICKUP = "Pickup";
static const char* TAG_HUD_LIVES = "HudLives";
static const char* TAG_HUD_FRUITS = "HudFruits";
static const char* TAG_HUD_BANNER = "HudBanner";

void registerPlatformerComponents(ComponentRegistry& registry) {
    registry.add<LevelGoal>("LevelGoal")
        .field("targetTag", &LevelGoal::targetTag).tooltip("Tag of whoever completes the level by touching it");
}

void registerPlatformerConditions(ConditionRegistry& conditions) {
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
    // Fruits collected this frame (they still exist: they are destroyed at the end of
    // the frame, or when their animation ends).
    world.each<Collectible, EntityInfo>([&](Entity, Collectible& c, EntityInfo& info) {
        if (!c.justCollected || info.tag != TAG_PICKUP) return;
        ++collected;
        SDL_Log("Fruits: %d/%d", collected, total);
    });

    // Goal: touched by someone alive with the requested tag.
    world.each<LevelGoal, BoxCollider>([&](Entity self, LevelGoal& goal, BoxCollider&) {
        if (goal.reached) return;
        world.each<BoxCollider, EntityInfo>([&](Entity other, BoxCollider&, EntityInfo& info) {
            if (goal.reached || info.tag != goal.targetTag || isDead(world, other)) return;
            if (!collidersOverlap(world, self, other, RULE_CONTACT_MARGIN)) return;
            goal.reached = true;
            if (!done) SDL_Log("LEVEL COMPLETE (fruits: %d/%d)", collected, total);
            done = true;
        });
    });

    updateHud(world);
}

void PlatformerRulesSystem::updateHud(World& world) {
    // The health of the first player with Health.
    const Health* hp = nullptr;
    world.each<Health, EntityInfo>([&](Entity, Health& h, EntityInfo& info) {
        if (!hp && info.tag == TAG_PLAYER) hp = &h;
    });

    // Each value in its own Text: if they were a single line, a change in length would
    // shift everything else. Assigning the same text costs nothing: the image of each
    // text is cached when drawing. Two spaces after ':' because in NormalFont.ttf the
    // space is very narrow (4 px at 32 pt).
    world.each<Text, EntityInfo>([&](Entity, Text& text, EntityInfo& info) {
        if (info.tag == TAG_HUD_LIVES && hp)
            text.text = "HP:  " + std::to_string(hp->hp) + "/" + std::to_string(hp->maxHP);
        else if (info.tag == TAG_HUD_FRUITS)
            text.text = "FRUITS:  " + std::to_string(collected) + "/" + std::to_string(total);
        else if (info.tag == TAG_HUD_BANNER)
            text.visible = done;
    });
}
