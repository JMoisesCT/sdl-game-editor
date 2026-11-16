#pragma once

class World;
class AssetManager;
class GameCanvas;
struct SDL_Renderer;  // forward declarations: SDL only appears in the .cpp
struct SDL_Texture;

// How the game (its resolution, e.g. 1280x720) is shown in an area of another size:
// the editor's "Game" tab or the window with --play.
//
// The problem: pixel art only looks right if each art pixel measures a WHOLE number
// of screen pixels. In platformer it measures 3 (scale 2 x zoom 1.5); if the area is
// 70% of the game, it would measure 2.1: with smoothing it looks blurry, and with
// nearest neighbor, distorted (columns of 2 and 3 px).
enum class GameScale {
    Fit,          // "Fit": the whole game image, scaled to fit (crisp when
                  // enlarging; when shrinking it gets smoothed and the pixel art turns blurry)
    PixelPerfect, // "Pixel perfect": drawn straight at the area's size, with the zoom
                  // rounded (Camera::pixelSize) so that each art pixel measures a whole
                  // number of px. Slightly more or less of the world is visible than in the game.
    Actual,       // "100%": at its resolution, centered; cropped if it does not fit
};

// Name to display ("Fit", "Pixel perfect", "100%") and for the command line
// ("fit", "pixel", "100"). parseGameScale: false if the text is none of them.
const char* gameScaleLabel(GameScale scale);
bool parseGameScale(const char* text, GameScale& scale);

// What to show: 'texture' (its src part) in the dst rectangle, measured inside the
// area (whole numbers; what is left around it is black bars).
struct GameScreen {
    SDL_Texture* texture = nullptr;
    float textureW = 0, textureH = 0;
    float srcX = 0, srcY = 0, srcW = 0, srcH = 0;
    float dstX = 0, dstY = 0, dstW = 0, dstH = 0;
    float scale = 0.0f;       // displayed size / game resolution
    float pixel = 0.0f;       // screen px per art pixel (Pixel perfect; otherwise 0)
    bool hasCamera = false;
};

// Draws the game (world + HUD) on the canvas for an area of areaW x areaH px. Leaves
// the render target on the window. false if the area is empty or there is no texture.
bool drawGameScreen(World& world, AssetManager& assets, SDL_Renderer* renderer, GameCanvas& canvas,
                    int gameWidth, int gameHeight, GameScale mode, float areaW, float areaH, GameScreen& out);
