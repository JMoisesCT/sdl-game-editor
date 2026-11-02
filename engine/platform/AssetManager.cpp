#include "AssetManager.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

AssetManager::AssetManager(SDL_Renderer* renderer)
    : renderer(renderer) {}

AssetManager::~AssetManager() {
    // Liberamos todas las texturas que cargamos.
    for (auto& [path, texture] : textures) {
        SDL_DestroyTexture(texture);
    }
    textures.clear();

    // Las imagenes de texto (antes que sus fuentes).
    clearTextTextures();

    // Y todas las fuentes.
    for (auto& [key, font] : fonts) {
        TTF_CloseFont(font);
    }
    fonts.clear();
}

SDL_Texture* AssetManager::loadTexture(const std::string& path) {
    // Si ya esta cargada, la devolvemos directo (sin recargar).
    auto it = textures.find(path);
    if (it != textures.end()) {
        return it->second;
    }
    // Ya fallo antes: no se reintenta en cada frame (ni se llena el log).
    if (failedTextures.count(path)) return nullptr;

    // Primera vez: cargar desde disco.
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_Log("No se pudo cargar la imagen '%s': %s", path.c_str(), SDL_GetError());
        failedTextures.insert(path);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_Log("No se pudo crear la textura '%s': %s", path.c_str(), SDL_GetError());
        failedTextures.insert(path);
        return nullptr;
    }

    // Muestreo por vecino mas cercano (no bilineal): coherencia de pixel art y
    // evita el sangrado de bordes al escalar (tilesets y sprites por igual).
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Guardar en cache y devolver.
    textures[path] = texture;
    return texture;
}

TTF_Font* AssetManager::loadFont(const std::string& path, int size) {
    // Clave compuesta (ruta + tamanio): la misma fuente a dos tamanios son dos
    // recursos distintos, cada uno en su entrada de cache.
    std::string key = path + "#" + std::to_string(size);

    // Si ya esta abierta a ese tamanio, la devolvemos directo (sin reabrir).
    auto it = fonts.find(key);
    if (it != fonts.end()) {
        return it->second;
    }

    // Ya fallo antes: no se reintenta en cada frame (ni se llena el log).
    if (failedFonts.count(key)) return nullptr;

    // Primera vez: abrir desde disco. En SDL3_ttf el tamanio en puntos es float.
    TTF_Font* font = TTF_OpenFont(path.c_str(), (float)size);
    if (!font) {
        SDL_Log("No se pudo abrir la fuente '%s' a %d pt: %s", path.c_str(), size, SDL_GetError());
        failedFonts.insert(key);
        return nullptr;
    }

    // Guardar en cache y devolver.
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

    // _Solid: sin suavizado, para que una fuente pixel quede nitida. En SDL3_ttf el
    // largo 0 significa "hasta el fin de la cadena".
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), 0, SDL_Color{ 255, 255, 255, 255 });
    if (!surface) {
        SDL_Log("No se pudo dibujar el texto '%s': %s", text.c_str(), SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        SDL_Log("No se pudo crear la textura del texto '%s': %s", text.c_str(), SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Sin limite, un cronometro llenaria la memoria de texturas que no se vuelven a
    // usar. Vaciar todo de vez en cuando es simple y alcanza: lo que se sigue
    // mostrando se vuelve a crear en el frame siguiente.
    if (textTextures.size() >= MAX_TEXT_TEXTURES) clearTextTextures();
    textTextures[key] = texture;
    return texture;
}

void AssetManager::clearTextTextures() {
    for (auto& [key, texture] : textTextures) SDL_DestroyTexture(texture);
    textTextures.clear();
}
