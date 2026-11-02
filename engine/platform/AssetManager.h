#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

struct SDL_Renderer;
struct SDL_Texture;
struct TTF_Font; // declaracion adelantada: SDL3_ttf solo aparece en el .cpp

// Carga y guarda en cache las texturas y las fuentes. Es el DUENO de todas ellas:
// si dos objetos piden la misma imagen (o la misma fuente a un tamanio), comparten
// el mismo recurso en memoria.

class AssetManager {
public:
    explicit AssetManager(SDL_Renderer* renderer);
    ~AssetManager();

    // Es dueno de recursos: no permitimos copiarlo.
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Devuelve la textura de esa ruta. La carga SOLO la primera vez;
    // las siguientes devuelve la que ya esta en cache. nullptr si falla.
    SDL_Texture* loadTexture(const std::string& path);

    // Devuelve la fuente de esa ruta a ese tamanio en puntos. Una fuente se abre por
    // (ruta, tamanio): la cachea por esa pareja y la reusa. nullptr si falla.
    TTF_Font* loadFont(const std::string& path, int size);

    // Olvida las rutas que fallaron, para que se vuelvan a intentar (p. ej. tras
    // agregar la imagen que faltaba a assets/).
    void forgetFailures() { failedTextures.clear(); }

private:
    SDL_Renderer* renderer = nullptr;                       // no somos dueno
    std::unordered_map<std::string, SDL_Texture*> textures; // SI somos dueno
    std::unordered_set<std::string> failedTextures;         // rutas que no cargaron
    std::unordered_map<std::string, TTF_Font*> fonts;       // SI somos dueno (clave: ruta#tamanio)
};
