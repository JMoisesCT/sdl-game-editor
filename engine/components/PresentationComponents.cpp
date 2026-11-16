#include "components/PresentationComponents.h"

#include "core/Reflection.h"
#include "systems/Conditions.h"

// The names of the clips of that entity's SpriteAnimator (to pick them in the
// Inspector). With 'none', first "" = none.
static std::vector<std::string> clipNames(const World& world, Entity e, bool none) {
    std::vector<std::string> names;
    if (none) names.push_back("");
    if (const SpriteAnimator* a = world.get<SpriteAnimator>(e))
        for (const AnimationClip& clip : a->clips) names.push_back(clip.name);
    return names;
}

void registerPresentationComponents(ComponentRegistry& registry, const ConditionRegistry& conditions) {
    // The lists go last: in the file they take one line per element.
    registry.add<SpriteAnimator>("SpriteAnimator")
        .field("startClip", &SpriteAnimator::startClip)
            .choices([](const World& w, Entity e) { return clipNames(w, e, true); })
            .tooltip("Clip when play starts (none = the first one). An AnimatorStateMachine changes it")
        .field("speed", &SpriteAnimator::speed).range(0.0f, 10.0f).speed(0.01f).tooltip("Multiplies the fps of every clip")
        .list("clips", &SpriteAnimator::clips, [](FieldsBuilder<AnimationClip>& clip) {
            clip.field("name", &AnimationClip::name).tooltip("Rules and code ask for it by this name")
                .field("texture", &AnimationClip::texture).texture()
                    .tooltip("Image with the frames (empty = the Sprite's image at start)")
                .field("frameWidth", &AnimationClip::frameWidth).range(1.0f, 2048.0f).tooltip("Frame width (image px)")
                .field("frameHeight", &AnimationClip::frameHeight).range(1.0f, 2048.0f).tooltip("Frame height (image px)")
                .field("frames", &AnimationClip::frames)
                    .tooltip("Cells in order, counting by rows (empty = all). A column of 4: \"0, 4, 8, 12\"")
                .field("fps", &AnimationClip::fps).range(0.0f, 120.0f).speed(0.1f).tooltip("Frames per second (0 = still)")
                .field("loop", &AnimationClip::loop).tooltip("Unchecked: stays on the last frame when it ends")
                .field("next", &AnimationClip::next)
                    .choices([](const World& w, Entity e) { return clipNames(w, e, true); })
                    .tooltip("Without loop: clip that follows when it ends")
                .field("destroyOnEnd", &AnimationClip::destroyOnEnd).tooltip("Without loop: destroys the entity when it ends");
        });

    const ConditionRegistry* conds = &conditions; // lives in Application, like the registry
    registry.add<AnimatorStateMachine>("AnimatorStateMachine")
        .field("defaultClip", &AnimatorStateMachine::defaultClip)
            .choices([](const World& w, Entity e) { return clipNames(w, e, true); })
            .tooltip("Clip when no rule holds (none = lets the last rule's clip finish)")
        .list("rules", &AnimatorStateMachine::rules, [conds](FieldsBuilder<AnimationRule>& rule) {
            rule.field("clip", &AnimationRule::clip)
                    .choices([](const World& w, Entity e) { return clipNames(w, e, false); })
                .field("condition", &AnimationRule::condition)
                    .choices([conds](const World&, Entity) { return conds->names(); })
                    .tooltip("The FIRST rule whose condition holds wins");
        });

    registry.add<FollowCamera>("FollowCamera")
        .field("targetTag", &FollowCamera::targetTag).tooltip("Tag of whom it follows")
        .field("deadZone", &FollowCamera::deadZone).range(0.0f, 4096.0f).speed(1.0f)
            .tooltip("Rectangle (world px) where the target moves without moving the camera")
        .field("smoothing", &FollowCamera::smoothing).range(0.0f, 60.0f).speed(0.1f).tooltip("Camera speed (0 = instant)")
        .field("lookAhead", &FollowCamera::lookAhead).range(0.0f, 1000.0f).speed(1.0f)
            .tooltip("Px the view leads toward where the target is heading")
        .field("lookAheadSpeed", &FollowCamera::lookAheadSpeed).range(0.0f, 60.0f).speed(0.1f)
        .field("clampToTilemaps", &FollowCamera::clampToTilemaps).tooltip("The view stays inside the scene's Tilemaps");

    registry.add<ParallaxBackground>("ParallaxBackground")
        .field("texture", &ParallaxBackground::texture).texture().tooltip("Repeating image (tileable)")
        .field("factor", &ParallaxBackground::factor).range(0.0f, 2.0f).speed(0.01f)
            .tooltip("How much it follows the camera: 0 = still, 1 = like the world")
        .field("scale", &ParallaxBackground::scale).range(0.05f, 64.0f).speed(0.05f)
        .field("scroll", &ParallaxBackground::scroll).speed(0.5f).tooltip("Its own scrolling (px/s), only while playing")
        .field("tint", &ParallaxBackground::tint)
        .field("visible", &ParallaxBackground::visible)
        .field("order", &ParallaxBackground::order).tooltip("Draw layer: lower = further back");

    registry.add<Text>("Text")
        .field("text", &Text::text)
        .field("font", &Text::font).tooltip(".ttf file (path from the executable's folder)")
        .field("size", &Text::size).range(1.0f, 512.0f).tooltip("Points")
        .field("color", &Text::color)
        .field("align", &Text::align).choices({ "left", "center", "right" })
            .tooltip("Where the position falls relative to the text (for a counter: left)")
        .field("screenSpace", &Text::screenSpace)
            .tooltip("HUD: the position is in game screen px and does not follow the camera")
        .field("visible", &Text::visible)
        .field("order", &Text::order).tooltip("Draw layer");
}
