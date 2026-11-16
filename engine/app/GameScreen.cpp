#include "app/GameScreen.h"

#include <SDL3/SDL.h>

#include <cmath>

#include "components/CoreComponents.h"
#include "core/World.h"
#include "platform/GameCanvas.h"
#include "systems/RenderSystem.h"

const char* gameScaleLabel(GameScale scale) {
    switch (scale) {
    case GameScale::Fit: return "Ajustar";
    case GameScale::PixelPerfect: return "Pixel perfecto";
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
    // Limpia con el color de la camara y dibuja el mundo y despues el HUD.
    void drawFrame(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view,
                   const Color& bg, const View& hud) {
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderClear(renderer);
        renderWorld(world, assets, renderer, view);
        renderScreenTexts(world, assets, renderer, hud);
    }

    // Zoom y pixelSize de la camara que usa el juego (la primera, como findCameraView).
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
    // Tamano de la imagen al ajustarla al area (Ajustar y Pixel perfecto), en px enteros:
    // es el que se va a dibujar de verdad.
    const float fitW = std::fmax(1.0f, std::round(gameWidth * fit));
    const float fitH = std::fmax(1.0f, std::round(gameHeight * fit));

    View view;
    Color bg;
    if (mode == GameScale::PixelPerfect) {
        // Directo al tamano mostrado, con el zoom redondeado a pixeles enteros del dibujo.
        // Si al redondear hacia abajo entra MAS mundo que en el juego, el lienzo se achica
        // a la misma porcion de mundo (bandas un poco mas anchas): si no, se veria lo que
        // la camara esconde, como lo que hay mas alla del borde del nivel. Si entra menos,
        // el lienzo ocupa el area y se ve un poco menos de mundo.
        float camZoom = 1.0f, pixel = 1.0f;
        cameraSettings(world, camZoom, pixel);
        out.hasCamera = findScaledCameraView(world, (int)fitW, (int)fitH, fit, true, view, &bg);
        const float w = std::fmin(fitW, std::fmax(1.0f, std::round(gameWidth / camZoom * view.zoom)));
        const float h = std::fmin(fitH, std::fmax(1.0f, std::round(gameHeight / camZoom * view.zoom)));
        if (!canvas.begin((int)w, (int)h)) return false;
        view.width = (int)w;
        view.height = (int)h;
        // El HUD (en px de la pantalla del juego) ocupa el lienzo entero, como en el juego.
        View hud = screenView(gameWidth, gameHeight);
        hud.zoom = w / gameWidth;
        hud.width = (int)w;
        hud.height = (int)h;
        drawFrame(world, assets, renderer, view, bg, hud);
        out.texture = canvas.scaledFor(w, h); // mismo tamano: vecino mas cercano, 1:1
        out.pixel = view.zoom * pixel;
        out.textureW = out.srcW = out.dstW = w;
        out.textureH = out.srcH = out.dstH = h;
        out.scale = w / gameWidth;
    } else {
        // A la resolucion del juego; despues, escalado (Ajustar) o 1:1 (100%).
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
            // Lo que no cabe se recorta, parejo por los dos lados.
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
