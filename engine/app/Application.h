#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/Reflection.h"
#include "core/SceneSerializer.h"
#include "core/World.h"
#include "systems/System.h"

struct SDL_Window;   // declaraciones adelantadas: SDL solo aparece en el .cpp
struct SDL_Renderer;
class AssetManager;
class Editor;

struct AppConfig {
    std::string title = "SDL Game Editor";
    int gameWidth = 1280;   // resolucion del juego: lo que ve el jugador (y la vista
    int gameHeight = 720;   // "Juego" del editor, que la escala para que quepa)
    bool editor = true;     // false: abre la escena y juega directo, sin paneles
};

// Modos de la aplicacion. Editando, la escena esta quieta (no corre ningun sistema);
// jugando, corren los sistemas en orden; en pausa, se ve el juego congelado.
enum class PlayMode { Edit, Play, Paused };

// La aplicacion es DUENA DEL BUCLE: crea la ventana, el renderer, el World y el
// editor, y en cada frame lee la entrada, corre los sistemas (si se juega) y dibuja.
// El juego no escribe su propio bucle: registra sus componentes y sistemas.
//
//     int main(int, char**) {
//         Application app;
//         if (!app.init({ "Mi juego", 1280, 720 })) return 1;
//         registerMiJuego(app);          // componentes + sistemas del juego
//         app.openScene("assets/scenes/nivel1.scene.json");
//         return app.run();
//     }
class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // SDL, SDL_ttf, ventana, renderer y (si config.editor) el editor. Tambien registra
    // los componentes basicos del motor. false si algo falla (el motivo va al log).
    bool init(const AppConfig& config);

    // El juego agrega aqui sus componentes (antes de abrir la escena, para que se
    // carguen) y sus sistemas (corren en el orden en que se agregan).
    ComponentRegistry& registry() { return components; }
    void addSystem(std::unique_ptr<System> system);

    // Carga la escena. Si el archivo no existe, empieza una escena vacia que se
    // guardara en esa ruta. false si existe pero no se pudo leer.
    bool openScene(const std::string& path);
    bool saveScene(); // solo editando (jugando, la escena esta "prestada" al juego)

    // Bucle principal hasta que se cierre la ventana. Devuelve el codigo de salida.
    int run();

    // --- Play / Stop -----------------------------------------------------------------
    // Play guarda una copia de la escena; Stop la restaura tal cual estaba, asi que lo
    // que pase jugando (o lo que se toque en el Inspector) no queda en la escena.
    void play();
    void stop();
    void setPaused(bool paused);
    PlayMode mode() const { return playMode; }
    bool isEditing() const { return playMode == PlayMode::Edit; }

    // --- Acceso para el editor y los sistemas ---------------------------------------
    World& world() { return sceneWorld; }
    AssetManager& assets() { return *assetManager; }
    SDL_Renderer* renderer() const { return sdlRenderer; }
    SDL_Window* window() const { return sdlWindow; }
    const AppConfig& config() const { return appConfig; }
    const std::string& scenePath() const { return currentScenePath; }
    const std::vector<std::unique_ptr<System>>& systems() const { return systemList; }

private:
    void shutdown();
    void renderGameDirect(); // sin editor: la vista del juego ocupa la ventana

    AppConfig appConfig;
    SDL_Window* sdlWindow = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    bool ttfReady = false;

    std::unique_ptr<AssetManager> assetManager;
    ComponentRegistry components;
    World sceneWorld;
    std::vector<std::unique_ptr<System>> systemList;
    std::unique_ptr<Editor> editor;

    std::string currentScenePath;
    PlayMode playMode = PlayMode::Edit;
    Json playSnapshot; // la escena tal como estaba al pulsar Play
};
