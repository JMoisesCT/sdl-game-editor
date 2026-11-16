#pragma once
#include <memory>
#include <string>
#include <vector>

#include "app/GameScreen.h"
#include "core/Prefabs.h"
#include "core/Reflection.h"
#include "core/SceneSerializer.h"
#include "core/World.h"
#include "systems/Conditions.h"
#include "systems/System.h"

struct SDL_Window;   // forward declarations: SDL only appears in the .cpp
struct SDL_Renderer;
class AssetManager;
class Audio;
class GameCanvas;
class Editor;

struct AppConfig {
    std::string title = "SDL Game Editor";
    int gameWidth = 1280;   // game resolution: what the player sees (and the editor's
    int gameHeight = 720;   // "Game" view, which scales it to fit)
    bool editor = true;     // false: opens the scene and plays directly, without panels
    // How the game is shown if the window (or the Game tab) is not the size of its
    // resolution. See app/GameScreen.h; the editor changes it in the Game tab.
    GameScale scale = GameScale::PixelPerfect;
};

// Application modes. While editing, the scene stands still (no system runs); while
// playing, the systems run in order; when paused, the frozen game is shown.
enum class PlayMode { Edit, Play, Paused };

// The application OWNS THE LOOP: it creates the window, the renderer, the World and
// the editor, and every frame it reads input, runs the systems (if playing) and draws.
// The game does not write its own loop: it registers its components and systems.
//
//     int main(int, char**) {
//         Application app;
//         if (!app.init({ "My Game", 1280, 720 })) return 1;
//         registerMyGame(app);           // the game's components + systems
//         app.openScene("assets/scenes/level1.scene.json");
//         return app.run();
//     }
class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // SDL, SDL_ttf, window, renderer, audio and (if config.editor) the editor. It also
    // registers the engine's basic components. false if something fails (the reason goes to the log).
    bool init(const AppConfig& config);

    // The game adds its components here (before opening the scene, so they get loaded)
    // and its systems (they run in the order they are added).
    ComponentRegistry& registry() { return components; }
    // The prefabs (.prefab.json) the scene uses; see core/Prefabs.h.
    PrefabLibrary& prefabs() { return prefabLibrary; }
    // The named conditions (see systems/Conditions.h) used by the animation and sound
    // rules. It comes with the engine's; the game adds its own.
    ConditionRegistry& conditions() { return conditionRegistry; }
    void addSystem(std::unique_ptr<System> system);

    // Loads the scene. If the file does not exist, it starts an empty scene that will
    // be saved to that path. false if it exists but could not be read.
    bool openScene(const std::string& path);
    bool saveScene(); // only while editing (while playing, the scene is "lent" to the game)

    // The scene in memory as JSON (same as the file: prefab instances only with their
    // overrides) and back. Used by Play/Stop and the editor's undo.
    Json snapshot();
    bool restore(const Json& scene, std::string& error);

    // Main loop until the window closes. Returns the exit code.
    int run();

    // --- Play / Stop -----------------------------------------------------------------
    // Play saves a copy of the scene; Stop restores it exactly as it was, so whatever
    // happens while playing (or is changed in the Inspector) does not stay in the scene.
    void play();
    void stop();
    void setPaused(bool paused);
    PlayMode mode() const { return playMode; }
    bool isEditing() const { return playMode == PlayMode::Edit; }

    // --- Access for the editor and the systems --------------------------------------
    World& world() { return sceneWorld; }
    AssetManager& assets() { return *assetManager; }
    Audio& audio() { return *audioDevice; }
    SDL_Renderer* renderer() const { return sdlRenderer; }
    SDL_Window* window() const { return sdlWindow; }
    const AppConfig& config() const { return appConfig; }
    void setGameScale(GameScale scale) { appConfig.scale = scale; }
    const std::string& scenePath() const { return currentScenePath; }
    const std::vector<std::unique_ptr<System>>& systems() const { return systemList; }

private:
    void shutdown();
    void renderGameDirect(); // without the editor: the game view fills the window

    AppConfig appConfig;
    SDL_Window* sdlWindow = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    bool ttfReady = false;

    std::unique_ptr<AssetManager> assetManager;
    std::unique_ptr<Audio> audioDevice;
    std::unique_ptr<GameCanvas> gameCanvas; // without the editor: the game at its resolution, scaled to the window
    ComponentRegistry components;
    PrefabLibrary prefabLibrary{ components };
    ConditionRegistry conditionRegistry;
    World sceneWorld;
    std::vector<std::unique_ptr<System>> systemList;
    std::unique_ptr<Editor> editor;

    std::string currentScenePath;
    PlayMode playMode = PlayMode::Edit;
    Json playSnapshot; // the scene as it was when Play was pressed
};
