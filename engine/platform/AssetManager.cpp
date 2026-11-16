#include "AssetManager.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

AssetManager::AssetManager(SDL_Renderer* renderer)
    : renderer(renderer) {}

AssetManager::~AssetManager() {
    // Release every texture we loaded.
    for (auto& [path, texture] : textures) {
        SDL_DestroyTexture(texture);
    }
    textures.clear();

    // The text images (before their fonts).
    clearTextTextures();

    // And every font.
    for (auto& [key, font] : fonts) {
        TTF_CloseFont(font);
    }
    fonts.clear();
}

SDL_Texture* AssetManager::loadTexture(const std::string& path) {
    // If it is already loaded, return it directly (no reload).
    auto it = textures.find(path);
    if (it != textures.end()) {
        return it->second;
    }
    // It already failed: it is not retried every frame (and does not flood the log).
    if (failedTextures.count(path)) return nullptr;

    // First time: load from disk.
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_Log("Could not load the image '%s': %s", path.c_str(), SDL_GetError());
        failedTextures.insert(path);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_Log("Could not create the texture '%s': %s", path.c_str(), SDL_GetError());
        failedTextures.insert(path);
        return nullptr;
    }

    // Nearest-neighbor sampling (not bilinear): consistent with pixel art, and it
    // avoids edge bleeding when scaling (tilesets and sprites alike).
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Cache and return it.
    textures[path] = texture;
    return texture;
}

TTF_Font* AssetManager::loadFont(const std::string& path, int size) {
    // Composite key (path + size): the same font at two sizes is two different
    // resources, each one with its own cache entry.
    std::string key = path + "#" + std::to_string(size);

    // If it is already open at that size, return it directly (no reopening).
    auto it = fonts.find(key);
    if (it != fonts.end()) {
        return it->second;
    }

    // It already failed: it is not retried every frame (and does not flood the log).
    if (failedFonts.count(key)) return nullptr;

    // First time: open from disk. In SDL3_ttf the size in points is a float.
    TTF_Font* font = TTF_OpenFont(path.c_str(), (float)size);
    if (!font) {
        SDL_Log("Could not open the font '%s' at %d pt: %s", path.c_str(), size, SDL_GetError());
        failedFonts.insert(key);
        return nullptr;
    }

    // Cache and return it.
    fonts[key] = font;
    return font;
}

SDL_Texture* AssetManager::textTexture(const std::string& fontPath, int size, const std::string& text) {
    if (text.empty() || size <= 0) return nullptr;
    std::string key = fontPath + "#" + std::to_string(size) + "#" + text;
    auto it = textTextures.find(key);
    if (it != textTextures.end()) return it->second;

    TTF_Font* font = loadFont(fontPath, size);
    if (!font) return nullptr;

    // _Solid: no antialiasing, so a pixel font stays crisp. In SDL3_ttf a length
    // of 0 means "up to the end of the string".
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), 0, SDL_Color{ 255, 255, 255, 255 });
    if (!surface) {
        SDL_Log("Could not render the text '%s': %s", text.c_str(), SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        SDL_Log("Could not create the texture for the text '%s': %s", text.c_str(), SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Without a limit, a stopwatch would fill memory with textures that are never used
    // again. Clearing everything now and then is simple and good enough: whatever is
    // still shown is created again on the next frame.
    if (textTextures.size() >= MAX_TEXT_TEXTURES) clearTextTextures();
    textTextures[key] = texture;
    return texture;
}

void AssetManager::clearTextTextures() {
    for (auto& [key, texture] : textTextures) SDL_DestroyTexture(texture);
    textTextures.clear();
}
