#include "platform/GameCanvas.h"

#include <SDL3/SDL.h>

#include <cmath>

GameCanvas::~GameCanvas() {
    if (canvas) SDL_DestroyTexture(canvas);
    if (upscaled) SDL_DestroyTexture(upscaled);
}

bool GameCanvas::begin(int width, int height) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (!canvas || canvasW != width || canvasH != height) {
        if (canvas) SDL_DestroyTexture(canvas);
        canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
        if (!canvas) {
            SDL_Log("GameCanvas: no se pudo crear el lienzo: %s", SDL_GetError());
            canvasW = canvasH = 0;
            return false;
        }
        canvasW = width;
        canvasH = height;
        upscaledFactor = 0; // el agrandado tambien cambia de tamano
    }
    SDL_SetRenderTarget(renderer, canvas);
    return true;
}

SDL_Texture* GameCanvas::scaledFor(float displayWidth, float displayHeight) {
    SDL_SetRenderTarget(renderer, nullptr);
    if (!canvas) return nullptr;
    const float k = std::fmin(displayWidth / canvasW, displayHeight / canvasH);

    // Factor entero (100%, 200%...): vecino mas cercano directo, ya es exacto. Mas
    // chico que el lienzo: suavizado lineal (con vecino mas cercano se perderian filas
    // enteras de pixeles).
    if (k <= 1.0f || std::fabs(k - std::round(k)) < 0.001f) {
        SDL_SetTextureScaleMode(canvas, k < 1.0f ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        return canvas;
    }

    // Factor no entero: agrandar al entero de arriba con vecino mas cercano y dejar que
    // el dibujo final lo achique con suavizado lineal.
    const int factor = (int)std::ceil(k);
    if (!upscaled || upscaledFactor != factor) {
        if (upscaled) SDL_DestroyTexture(upscaled);
        upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     canvasW * factor, canvasH * factor);
        if (!upscaled) {
            SDL_Log("GameCanvas: no se pudo crear el lienzo agrandado: %s", SDL_GetError());
            upscaledFactor = 0;
            SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST);
            return canvas; // mejor deformado que nada
        }
        SDL_SetTextureScaleMode(upscaled, SDL_SCALEMODE_LINEAR);
        upscaledFactor = factor;
    }
    SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(renderer, upscaled);
    SDL_RenderTexture(renderer, canvas, nullptr, nullptr);
    SDL_SetRenderTarget(renderer, nullptr);
    return upscaled;
}
