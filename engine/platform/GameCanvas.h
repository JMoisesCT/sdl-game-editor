#pragma once

struct SDL_Renderer; // forward declarations: SDL only appears in the .cpp
struct SDL_Texture;

// The game's CANVAS: the game is drawn at ITS resolution (the AppConfig one) on a
// texture, and then shown scaled wherever there is room (the window with --play, the
// editor's "Game" tab).
//
// Why not scale directly with nearest neighbor: at a NON-integer factor, each pixel
// takes more or fewer screen pixels depending on where it lands. With 3 px per art
// pixel (sprites at scale 2 and a camera with zoom 1.5), at 1.25x there are columns
// of 3 and 4 pixels: when the character "breathes" and shifts a pixel, it changes
// shape and seems to jump. With linear smoothing,
// on the other hand, everything looks blurry.
//
// SHARP scaling (the emulators' "sharp bilinear"): first enlarge with nearest
// neighbor to the next integer (2x: each pixel, an exact 2x2 block) and then shrink
// that result with linear smoothing to the final size. All the pixels stay the same
// size and only the border between two of them blends into one line.
class GameCanvas {
public:
    explicit GameCanvas(SDL_Renderer* renderer) : renderer(renderer) {}
    ~GameCanvas();
    GameCanvas(const GameCanvas&) = delete;
    GameCanvas& operator=(const GameCanvas&) = delete;

    // Makes the canvas (width x height) the render target; creates it the first time or
    // if the size changes. false if it could not be created.
    bool begin(int width, int height);

    // The texture to show the canvas with in a rectangle of displayWidth x
    // displayHeight screen pixels (the caller draws it there). Leaves the render target
    // on the window. nullptr if there is no canvas.
    SDL_Texture* scaledFor(float displayWidth, float displayHeight);

    int width() const { return canvasW; }
    int height() const { return canvasH; }

private:
    SDL_Renderer* renderer;
    SDL_Texture* canvas = nullptr;   // at the game's resolution
    int canvasW = 0, canvasH = 0;
    SDL_Texture* upscaled = nullptr; // the canvas enlarged by an integer factor
    int upscaledFactor = 0;
};
