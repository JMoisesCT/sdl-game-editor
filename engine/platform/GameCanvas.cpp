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
            SDL_Log("GameCanvas: could not create the canvas: %s", SDL_GetError());
            canvasW = canvasH = 0;
            return false;
        }
        canvasW = width;
        canvasH = height;
        upscaledFactor = 0; // the enlarged one also changes size
    }
    SDL_SetRenderTarget(renderer, canvas);
    return true;
}

SDL_Texture* GameCanvas::scaledFor(float displayWidth, float displayHeight) {
    SDL_SetRenderTarget(renderer, nullptr);
    if (!canvas) return nullptr;
    const float k = std::fmin(displayWidth / canvasW, displayHeight / canvasH);

    // Integer factor (100%, 200%...): straight nearest neighbor, already exact. Smaller
    // than the canvas: linear smoothing (with nearest neighbor whole rows of pixels
    // would be lost).
    if (k <= 1.0f || std::fabs(k - std::round(k)) < 0.001f) {
        SDL_SetTextureScaleMode(canvas, k < 1.0f ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        return canvas;
    }

    // Non-integer factor: enlarge to the next integer with nearest neighbor and let
    // the final draw shrink it with linear smoothing.
    const int factor = (int)std::ceil(k);
    if (!upscaled || upscaledFactor != factor) {
        if (upscaled) SDL_DestroyTexture(upscaled);
        upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                     canvasW * factor, canvasH * factor);
        if (!upscaled) {
            SDL_Log("GameCanvas: could not create the upscaled canvas: %s", SDL_GetError());
            upscaledFactor = 0;
            SDL_SetTextureScaleMode(canvas, SDL_SCALEMODE_NEAREST);
            return canvas; // better distorted than nothing
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
