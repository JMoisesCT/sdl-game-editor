#include "systems/Animation.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <set>

#include "components/CoreComponents.h"
#include "components/PresentationComponents.h"
#include "core/World.h"
#include "platform/AssetManager.h"

// --- Playback ----------------------------------------------------------------------------

// Seconds a frame is forgiven to count it as done (see update). Larger than the
// typical jitter of the clock with VSync (~1 ms) and much smaller than a frame.
static const float FRAME_TIME_MARGIN = 0.002f;

namespace {
    const AnimationClip* findClip(const SpriteAnimator& a, const std::string& name) {
        for (const AnimationClip& clip : a.clips)
            if (clip.name == name) return &clip;
        return nullptr;
    }

    void startClip(SpriteAnimator& a, const std::string& name) {
        a.current = name;
        a.frame = 0;
        a.timer = 0.0f;
        a.finished = false;
    }

    // A clip's image and its grid (columns and rows of frames). false if it does not
    // load or the frame does not fit.
    bool clipGrid(AssetManager& assets, const AnimationClip& clip, const std::string& path,
                  int& columns, int& rows) {
        if (path.empty() || clip.frameWidth <= 0 || clip.frameHeight <= 0) return false;
        SDL_Texture* tex = assets.loadTexture(path);
        if (!tex) return false;
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        columns = (int)tw / clip.frameWidth;
        rows = (int)th / clip.frameHeight;
        return columns > 0 && rows > 0;
    }
}

bool playAnimation(World& world, Entity e, const std::string& clip, bool restart) {
    SpriteAnimator* a = world.get<SpriteAnimator>(e);
    if (!a || !findClip(*a, clip)) return false;
    if (a->current == clip && !restart) return true; // already playing: do not cut it
    startClip(*a, clip);
    return true;
}

void SpriteAnimatorSystem::onPlay(World& world) {
    world.each<SpriteAnimator>([](Entity, SpriteAnimator& a) {
        a.current.clear();
        a.frame = 0;
        a.timer = 0.0f;
        a.finished = a.justFinished = false;
        a.started = false;
    });
}

void SpriteAnimatorSystem::update(World& world, float dt) {
    world.each<SpriteAnimator, Sprite>([&](Entity e, SpriteAnimator& a, Sprite& sprite) {
        a.justFinished = false;
        // Lazy start (also works for an entity created while playing): the Sprite's image
        // is remembered, which is the one for clips without their own texture, and if
        // nobody requested another clip yet, the start clip plays.
        if (!a.started) {
            a.started = true;
            a.baseTexture = sprite.texture;
            if (a.current.empty() && !a.clips.empty()) {
                const AnimationClip* first = findClip(a, a.startClip);
                startClip(a, first ? first->name : a.clips.front().name);
            }
        }

        const AnimationClip* clip = findClip(a, a.current);
        if (!clip) return;
        const std::string& path = clip->texture.empty() ? a.baseTexture : clip->texture;
        int columns = 0, rows = 0;
        if (!clipGrid(assets, *clip, path, columns, rows)) return;
        const int count = clip->frames.empty() ? columns * rows : (int)clip->frames.size();

        // Advances as many frames as fit in the accumulated time (at low fps, or with a
        // slow frame, there can be several). With a MARGIN: at 20 fps a frame lasts 50 ms,
        // exactly 3 frames at 60 Hz, but the real clock jitters and 3 frames can add up
        // to 49.8 ms. Without a margin that frame would wait for a 4th frame and the next
        // would last 2: the animation would stutter (3, 4, 2, 3...). What runs ahead is
        // deducted from the next frame (timer goes negative), so the average speed does not change.
        if (!a.finished && clip->fps > 0.0f && a.speed > 0.0f) {
            const float frameTime = 1.0f / clip->fps;
            const float margin = std::fmin(FRAME_TIME_MARGIN, frameTime * 0.25f);
            a.timer += dt * a.speed;
            while (a.timer >= frameTime - margin) {
                a.timer -= frameTime;
                if (a.frame + 1 < count) {
                    ++a.frame;
                } else if (clip->loop) {
                    a.frame = 0;
                } else {
                    a.finished = a.justFinished = true; // stays on the last frame
                    a.timer = 0.0f;
                    break;
                }
            }
        }
        if (a.frame >= count) a.frame = count - 1; // the clip was edited to have fewer frames

        // The current frame goes to the Sprite: image and crop.
        const int cell = clip->frames.empty() ? a.frame : clip->frames[a.frame];
        const int col = cell % columns, row = cell / columns;
        sprite.texture = path;
        sprite.source = { col * clip->frameWidth, row * clip->frameHeight, clip->frameWidth, clip->frameHeight };

        // When a clip without loop ends: destroy (deferred) or chain. Goes last:
        // startClip changes a.current, and 'clip' and 'path' point to the animator's data.
        if (a.justFinished) {
            if (clip->destroyOnEnd) world.destroy(e);
            else if (!clip->next.empty() && findClip(a, clip->next)) startClip(a, std::string(clip->next));
        }
    });
}

// --- State machine -----------------------------------------------------------------------

void AnimatorStateMachineSystem::onPlay(World& world) {
    world.each<AnimatorStateMachine>([](Entity, AnimatorStateMachine& fsm) { fsm.state.clear(); });
}

void AnimatorStateMachineSystem::update(World& world, float) {
    // One warning per unknown name, not one per frame.
    static std::set<std::string> warned;
    world.each<AnimatorStateMachine>([&](Entity e, AnimatorStateMachine& fsm) {
        const std::string* chosen = &fsm.defaultClip;
        for (const AnimationRule& rule : fsm.rules) {
            const Condition* condition = conditions.find(rule.condition);
            if (!condition) {
                if (warned.insert(rule.condition).second)
                    SDL_Log("AnimatorStateMachine: condition '%s' does not exist (the rule never holds)",
                            rule.condition.c_str());
                continue;
            }
            if ((*condition)(world, e)) { chosen = &rule.clip; break; } // the first one wins
        }
        // Only when the rule changes: within the same state, the clip may have chained
        // into its 'next' and must not be restarted.
        // With no rule and no defaultClip: the clip keeps going as is, and the next rule
        // that wins (even the same one as before) starts it again.
        if (chosen->empty()) {
            fsm.state.clear();
            return;
        }
        if (*chosen == fsm.state) return;
        fsm.state = *chosen;
        playAnimation(world, e, fsm.state, true);
    });
}
