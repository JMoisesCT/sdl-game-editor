#include "app/GameScreen.h"

#include <SDL3/SDL.h>

#include <cmath>

#include "components/CoreComponents.h"
#include "core/World.h"
#include "platform/GameCanvas.h"
#include "systems/RenderSystem.h"

const char* gameScaleLabel(GameScale scale) {
    switch (scale) {
    case GameScale::Fit: return "Fit";
    case GameScale::PixelPerfect: return "Pixel perfect";
    case GameScale::Actual: return "100%";
    }
    return "?";
}

bool parseGameScale(const char* text, GameScale& scale) {
    if (SDL_strcmp(text, "fit") == 0) scale = GameScale::Fit;
    else if (SDL_strcmp(text, "pixel") == 0) scale = GameScale::PixelPerfect;
    else if (SDL_strcmp(text, "100") == 0) scale = GameScale::Actual;
    else return false;
    return true;
}

namespace {
    // Clears with the camera's color and draws the world and then the HUD.
    void drawFrame(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view,
                   const Color& bg, const View& hud) {
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderClear(renderer);
        renderWorld(world, assets, renderer, view);
        renderScreenTexts(world, assets, renderer, hud);
    }

    // Zoom and pixelSize of the camera the game uses (the first one, like findCameraView).
    void cameraSettings(World& world, float& zoom, float& pixel) {
        zoom = pixel = 1.0f;
        bool found = false;
        world.each<Camera, Transform>([&](Entity, Camera& cam, Transform&) {
            if (found) return;
            found = true;
            if (cam.zoom > 0.0f) zoom = cam.zoom;
            if (cam.pixelSize > 0.0f) pixel = cam.pixelSize;
        });
    }
}

bool drawGameScreen(World& world, AssetManager& assets, SDL_Renderer* renderer, GameCanvas& canvas,
                    int gameWidth, int gameHeight, GameScale mode, float areaW, float areaH, GameScreen& out) {
    out = GameScreen{};
    const float fit = std::fmin(areaW / gameWidth, areaH / gameHeight);
    if (gameWidth < 1 || gameHeight < 1 || fit <= 0.0f) {
        SDL_SetRenderTarget(renderer, nullptr);
        return false;
    }
    // Size of the image when fitted to the area (Fit and Pixel perfect), in whole px:
    // it is the one that will actually be drawn.
    const float fitW = std::fmax(1.0f, std::round(gameWidth * fit));
    const float fitH = std::fmax(1.0f, std::round(gameHeight * fit));

    View view;
    Color bg;
    if (mode == GameScale::PixelPerfect) {
        // Straight at the displayed size, with the zoom rounded to whole art pixels.
        // If rounding down lets MORE of the world in than in the game, the canvas shrinks to
        // the same portion of the world (slightly wider bars): otherwise, what the camera
        // hides would show, such as what lies beyond the edge of the level. If less fits,
        // the canvas fills the area and slightly less of the world is visible.
        float camZoom = 1.0f, pixel = 1.0f;
        cameraSettings(world, camZoom, pixel);
        out.hasCamera = findScaledCameraView(world, (int)fitW, (int)fitH, fit, true, view, &bg);
        const float w = std::fmin(fitW, std::fmax(1.0f, std::round(gameWidth / camZoom * view.zoom)));
        const float h = std::fmin(fitH, std::fmax(1.0f, std::round(gameHeight / camZoom * view.zoom)));
        if (!canvas.begin((int)w, (int)h)) return false;
        view.width = (int)w;
        view.height = (int)h;
        // The HUD (in game screen px) covers the whole canvas, as in the game.
        View hud = screenView(gameWidth, gameHeight);
        hud.zoom = w / gameWidth;
        hud.width = (int)w;
        hud.height = (int)h;
        drawFrame(world, assets, renderer, view, bg, hud);
        out.texture = canvas.scaledFor(w, h); // same size: nearest neighbor, 1:1
        out.pixel = view.zoom * pixel;
        out.textureW = out.srcW = out.dstW = w;
        out.textureH = out.srcH = out.dstH = h;
        out.scale = w / gameWidth;
    } else {
        // At the game's resolution; then scaled (Fit) or 1:1 (100%).
        if (!canvas.begin(gameWidth, gameHeight)) return false;
        out.hasCamera = findCameraView(world, gameWidth, gameHeight, view, &bg);
        drawFrame(world, assets, renderer, view, bg, screenView(gameWidth, gameHeight));
        out.textureW = out.srcW = (float)gameWidth;
        out.textureH = out.srcH = (float)gameHeight;
        if (mode == GameScale::Fit) {
            out.texture = canvas.scaledFor(fitW, fitH);
            if (out.texture) SDL_GetTextureSize(out.texture, &out.textureW, &out.textureH);
            out.srcW = out.textureW;
            out.srcH = out.textureH;
            out.dstW = fitW;
            out.dstH = fitH;
            out.scale = fitW / gameWidth;
        } else {
            out.texture = canvas.scaledFor((float)gameWidth, (float)gameHeight);
            // Whatever does not fit is cropped, evenly on both sides.
            out.dstW = out.srcW = std::fmin((float)gameWidth, std::floor(areaW));
            out.dstH = out.srcH = std::fmin((float)gameHeight, std::floor(areaH));
            out.srcX = std::floor((gameWidth - out.srcW) * 0.5f);
            out.srcY = std::floor((gameHeight - out.srcH) * 0.5f);
            out.scale = 1.0f;
        }
    }
    out.dstX = std::round((areaW - out.dstW) * 0.5f);
    out.dstY = std::round((areaH - out.dstH) * 0.5f);
    return out.texture != nullptr;
}
