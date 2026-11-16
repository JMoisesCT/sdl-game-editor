#pragma once
#include "core/Entity.h"
#include "core/Math.h"

class World;
class AssetManager;
struct SDL_Renderer; // forward declaration: SDL only appears in the .cpp

// A view of the world: which point is at the center, with what zoom, and on a target
// of what size (in pixels). Used by the game view (from a Camera) and by the editor
// view (which moves on its own, without touching any entity).
struct View {
    Vec2 center;          // world point that lands at the center of the target
    float zoom = 1.0f;
    int width = 0;        // size of the target in pixels
    int height = 0;

    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;
};

// Builds the view of the FIRST entity with Camera + Transform. false if there is no
// camera (the view stays centered on the origin with zoom 1). background receives the
// camera's color, if passed.
bool findCameraView(World& world, int width, int height, View& view, Color* background = nullptr);

// Like findCameraView, for a target that shows the game screen enlarged or shrunk
// by 'scale' (the target already measures width x height): the camera zoom is
// multiplied by scale. With pixelPerfect it is also rounded so that one art pixel
// (Camera::pixelSize world px) measures a WHOLE number of px (at least 1): a bit
// more or less of the world is visible, but the pixel art stays even and crisp.
bool findScaledCameraView(World& world, int width, int height, float scale, bool pixelPerfect,
                          View& view, Color* background = nullptr);

// Size of that entity's sprite in the world (crop or texture times the scale).
// false if it has no Transform + Sprite or the texture does not load.
bool spriteWorldSize(World& world, AssetManager& assets, Entity e, Vec2& size);

// Rectangle of a Text in ITS space (the world, or the game screen if it is
// screenSpace): center (shifted according to align) and size. false if it has no
// Transform + Text or it cannot be drawn (no font or no text).
bool textBounds(World& world, AssetManager& assets, Entity e, Vec2& center, Vec2& size);

// Draws the WORLD: the visible Sprites, Tilemaps, ParallaxBackgrounds and Texts (the
// non-screenSpace ones), from lowest to highest order (on a tie, by id), on the
// renderer's CURRENT render target. Of each Tilemap only the cells inside the view are
// drawn; a ParallaxBackground fills the whole view.
void renderWorld(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view);

// The view that leaves the game screen as is (px of the game's resolution): the one
// passed to renderScreenTexts when drawing the game.
View screenView(int width, int height);

// Draws the HUD: the Texts with screenSpace, by order, on top of whatever is there.
// 'hud' maps game screen px to the target: in the game it is screenView(); the
// editor builds it to show the HUD over the camera frame in the Scene view.
void renderScreenTexts(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& hud);
