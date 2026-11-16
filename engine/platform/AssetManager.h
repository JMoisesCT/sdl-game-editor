#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

struct SDL_Renderer;
struct SDL_Texture;
struct TTF_Font; // forward declaration: SDL3_ttf only appears in the .cpp

// Loads and caches textures and fonts. It OWNS all of them: if two objects request
// the same image (or the same font at one size), they share the same resource in
// memory.

class AssetManager {
public:
    explicit AssetManager(SDL_Renderer* renderer);
    ~AssetManager();

    // It owns resources: copying is not allowed.
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Returns the texture at that path. It loads it ONLY the first time;
    // after that it returns the cached one. nullptr if it fails.
    SDL_Texture* loadTexture(const std::string& path);

    // Returns the font at that path at that size in points. A font is opened per
    // (path, size): it is cached by that pair and reused. nullptr if it fails.
    TTF_Font* loadFont(const std::string& path, int size);

    // The image of a text with that font and size, in WHITE (the color is set when
    // drawing, with SDL_SetTextureColorMod: that way the same text in two colors is a
    // single texture). No antialiasing, for pixel fonts. Created only the first time;
    // nullptr if the font does not open or the text is empty. The cache is cleared
    // entirely when it exceeds MAX_TEXT_TEXTURES (a changing counter creates one per value).
    SDL_Texture* textTexture(const std::string& fontPath, int size, const std::string& text);

    // Forgets the paths that failed, so they are tried again (e.g. after adding the
    // missing image to assets/).
    void forgetFailures() { failedTextures.clear(); failedFonts.clear(); }

private:
    static const std::size_t MAX_TEXT_TEXTURES = 256;
    void clearTextTextures();

    SDL_Renderer* renderer = nullptr;                       // not owned
    std::unordered_map<std::string, SDL_Texture*> textures; // owned
    std::unordered_set<std::string> failedTextures;         // paths that failed to load
    std::unordered_map<std::string, TTF_Font*> fonts;       // owned (key: path#size)
    std::unordered_set<std::string> failedFonts;            // path#size that failed to open
    std::unordered_map<std::string, SDL_Texture*> textTextures; // owned (key: font#size#text)
};
