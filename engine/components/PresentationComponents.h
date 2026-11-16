#pragma once
#include <string>
#include <vector>

#include "core/Entity.h"
#include "core/Math.h"

class ComponentRegistry;
class ConditionRegistry;

// PRESENTATION components: animation, follow camera, parallax backgrounds and text.
// Like all of them, they are ONLY data; the systems in systems/Animation.h and
// systems/Presentation.h drive them, and RenderSystem draws them.

// --- Animation ---------------------------------------------------------------------------

// A clip: a sequence of frames cropped from a grid image. Frames are numbered by
// rows from the top left, like the tiles of a Tilemap (with 4 columns, 4 is the
// first one of the second row).
//
// It covers the three usual ways of organizing an animation:
//   - one strip per file (Pixel Adventure: "Run (32x32).png"): texture + empty frames;
//   - a row or column of a sheet (Ninja Adventure: column 0 faces down):
//     frames = the cells of that line ("0, 4, 8, 12");
//   - individual cells of the Sprite's image: empty texture + frames.
struct AnimationClip {
    std::string name = "clip";
    std::string texture;      // empty = the texture the Sprite had when play started
    int frameWidth = 32;      // size of a frame in the image (px)
    int frameHeight = 32;
    std::vector<int> frames;  // cells in order; empty = all of the image's
    float fps = 20.0f;
    bool loop = true;
    std::string next;         // without loop: when it ends, switches to this clip (empty = stays on the last frame)
    bool destroyOnEnd = false; // without loop: when it ends, destroys the entity (effects, collected fruits)
};

// Plays clips frame by frame. It does NOT draw: it writes the texture and crop of the
// Sprite of the same entity, and RenderSystem draws the Sprite as usual.
//
// Who picks the clip: the entity's AnimatorStateMachine (if it has one), or any
// system with playAnimation (Animation.h). With nobody picking, startClip plays.
//
// One-frame flags (like justLanded): justFinished, when a clip without loop ends.
struct SpriteAnimator {
    std::string startClip;    // clip when play starts (empty = the first one)
    float speed = 1.0f;       // 2 = twice as fast; 0 = frozen
    std::vector<AnimationClip> clips;

    // --- Runtime state (written by SpriteAnimatorSystem) ---
    std::string current;      // clip playing
    int frame = 0;            // position within the clip's frames
    float timer = 0.0f;       // time accumulated on the current frame
    bool finished = false;    // clip without loop that already reached its last frame
    bool justFinished = false; // ...and reached it this frame
    std::string baseTexture;  // the Sprite's texture at the start (for clips without a texture)
    bool started = false;
};

// An AnimatorStateMachine rule: if the condition holds, that clip plays.
struct AnimationRule {
    std::string clip;
    std::string condition;    // name of a registered condition (ConditionRegistry)
};

// Picks which clip plays from RULES instead of a cascade of ifs. Every frame they
// are walked IN ORDER and the FIRST one whose condition holds wins; if none holds,
// defaultClip plays. The order IS the priority:
//     hit  if dead
//     jump if rising
//     fall if airborne
//     run  if moving
//     (otherwise, idle)
// Conditions are named C++ functions (the logic stays in C++): the engine provides
// the basic ones (grounded, rising, dead...) and the game adds its own. See
// systems/Conditions.h.
//
// It only switches clips when the winning rule changes: that way a clip can chain
// into its 'next' (flag coming out -> flag waving) without the machine restarting it.
//
// Without a defaultClip ("none"), when no rule wins, the clip that is playing keeps
// going until the end: that is how to animate a one-frame EVENT (the trampoline
// bouncing: jump if bounced, and the clip returns to idle by itself with its 'next').
// The next time the rule wins, its clip starts again.
struct AnimatorStateMachine {
    std::string defaultClip = "idle";
    std::vector<AnimationRule> rules;

    // --- Runtime state ---
    std::string state;        // clip picked in the last frame
};

// --- Camera ------------------------------------------------------------------------------

// Goes on the Camera entity: it moves it to follow the first entity with the tag
// targetTag, but ONLY when the target leaves the dead zone (a rectangle centered on
// the camera). While the target moves inside it, the camera stays still: that way
// it does not shake with every step or every small jump.
struct FollowCamera {
    std::string targetTag = "Player";
    Vec2 deadZone{ 160.0f, 120.0f }; // width and height of the dead zone (world px)
    float smoothing = 6.0f;          // how fast it reaches its destination (0 = instantly)
    float lookAhead = 0.0f;          // px the view leads toward where the target is heading
    float lookAheadSpeed = 3.0f;     // how fast that lead changes
    bool clampToTilemaps = true;     // the view stays inside the scene's Tilemaps

    // --- Runtime state ---
    Entity target = NullEntity;
    float lookOffset = 0.0f;   // current lead
    float lastTargetX = 0.0f;
    bool started = false;      // already placed on the target
};

// --- Background --------------------------------------------------------------------------

// An image that repeats to fill the view and only partly follows the camera: it
// gives a sense of depth. The image must be TILEABLE (the Pixel Adventure ones
// tile with themselves). It does not use the Transform: it always fills the whole view.
//
// factor: 0 = never moves (infinitely far), 0.3 = background mountains,
// 1 = moves just like the world. Several layers with different factors give the
// full effect.
struct ParallaxBackground {
    std::string texture;
    Vec2 factor{ 0.3f, 0.3f };
    float scale = 1.0f;      // tile size (4 with a 64 px image = 256 px)
    Vec2 scroll;             // its own scrolling in px/s (clouds, water); only while playing
    Color tint;
    bool visible = true;
    int order = -100;        // draw layer, on the same scale as Sprite::order

    // --- Runtime state ---
    Vec2 offset;             // how far it has moved because of 'scroll'
};

// --- Text --------------------------------------------------------------------------------

// A line of text with a TTF font. The Transform gives the position (and its scale
// enlarges the text); rotation does not apply.
//
// screenSpace:
//   false -> the text is in the WORLD: it goes through the camera (a sign in the level).
//   true  -> the text is on the game SCREEN (HUD): the position is in pixels of the
//            game's resolution, from the top left corner, and it does not move
//            with the camera. It is drawn on top of the whole world.
//
// align says where the position falls relative to the text, horizontally. A counter
// whose length changes ("FRUITS: 9/10") wants "left": with "center" it would
// re-center and seem to jump. Vertically, the position is always the line's center.
//
// Crisp pixel art: the text is rasterized without antialiasing (pixel fonts) and
// drawn at whole coordinates. The game changes the text from a system (see
// game/PlatformerRules); drawing caches the image of each text.
struct Text {
    std::string text = "Texto";
    std::string font = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
    int size = 16;             // points
    Color color;
    std::string align = "center"; // left, center or right
    bool screenSpace = false;
    bool visible = true;
    int order = 100;           // draw layer (among the HUD texts, or with the sprites)
};

// Declares the components above to the reflection. Application calls it at startup,
// after registerRuleComponents. 'conditions' provides the list of conditions the
// Inspector offers in the AnimatorStateMachine rules.
void registerPresentationComponents(ComponentRegistry& registry, const ConditionRegistry& conditions);
