#include "app/Application.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <filesystem>
#include <system_error>
#include <vector>

#include "components/AudioComponents.h"
#include "components/CoreComponents.h"
#include "components/MotionComponents.h"
#include "components/PhysicsComponents.h"
#include "components/PresentationComponents.h"
#include "components/RuleComponents.h"
#include "editor/Editor.h"
#include "platform/AssetManager.h"
#include "platform/Audio.h"
#include "platform/GameCanvas.h"
#include "platform/Input.h"
#include "platform/Screenshot.h"
#include "systems/RenderSystem.h"

// Cap on the time step. If a frame takes longer than this (dragging the window,
// stopping in the debugger), it is treated as 50 ms: better a moment in slow motion
// than a jump of hundreds of pixels that goes through walls.
static const float MAX_DT = 0.05f;

// Every path in a scene is relative to the folder that holds assets/. If the program was
// started from somewhere else (a double click, an IDE that runs it from the build folder),
// it moves to the first folder that has one: the executable's, or else the repo's (the
// CMake build defines SGE_SOURCE_DIR, so Save writes to the repo and not to a copy).
static void findAssetsFolder() {
    std::error_code error;
    if (std::filesystem::is_directory("assets", error)) return;

    std::vector<std::filesystem::path> candidates;
    if (const char* base = SDL_GetBasePath()) candidates.push_back(std::filesystem::u8path(base));
#ifdef SGE_SOURCE_DIR
    candidates.push_back(std::filesystem::u8path(SGE_SOURCE_DIR));
#endif
    for (const std::filesystem::path& folder : candidates) {
        if (!std::filesystem::is_directory(folder / "assets", error)) continue;
        std::filesystem::current_path(folder, error);
        if (error) continue;
        SDL_Log("Using the assets/ folder in %s", folder.u8string().c_str());
        return;
    }
    SDL_Log("No assets/ folder found (here, next to the executable or in the project)");
}

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::init(const AppConfig& config) {
    appConfig = config;
    findAssetsFolder();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }
    // SDL3_ttf must be initialized once (SDL3_image does not require it).
    if (!TTF_Init()) {
        SDL_Log("Could not initialize SDL_ttf: %s", SDL_GetError());
        shutdown();
        return false;
    }
    ttfReady = true;

    // With the editor, a large resizable window (maximized); without the editor, the
    // size of the game.
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (config.editor) flags |= SDL_WINDOW_MAXIMIZED;
    int w = config.editor ? 1600 : config.gameWidth;
    int h = config.editor ? 900 : config.gameHeight;
    sdlWindow = SDL_CreateWindow(config.title.c_str(), w, h, flags);
    if (!sdlWindow) {
        SDL_Log("Could not create the window: %s", SDL_GetError());
        shutdown();
        return false;
    }
    sdlRenderer = SDL_CreateRenderer(sdlWindow, nullptr);
    if (!sdlRenderer) {
        SDL_Log("Could not create the renderer: %s", SDL_GetError());
        shutdown();
        return false;
    }
    // VSync: the editor does not need 2000 fps burning the CPU.
    SDL_SetRenderVSync(sdlRenderer, 1);

    // Without the editor, the game is drawn at its resolution on a canvas that is scaled
    // to the window (with black bars if the aspect ratio does not match; see GameCanvas).
    gameCanvas = std::make_unique<GameCanvas>(sdlRenderer);

    // Without an audio device everything goes on, muted (the reason goes to the log).
    audioDevice = std::make_unique<Audio>();
    audioDevice->init();

    assetManager = std::make_unique<AssetManager>(sdlRenderer);
    registerCoreComponents(components);
    registerPhysicsComponents(components);
    registerRuleComponents(components);
    registerMotionComponents(components);
    registerEngineConditions(conditionRegistry);
    registerPresentationComponents(components, conditionRegistry);
    registerAudioComponents(components, conditionRegistry);

    if (config.editor) {
        editor = std::make_unique<Editor>(*this);
        if (!editor->init()) editor.reset(); // no panels means no editor: play directly
    }
    return true;
}

void Application::addSystem(std::unique_ptr<System> system) {
    systemList.push_back(std::move(system));
}

bool Application::openScene(const std::string& path) {
    if (playMode != PlayMode::Edit) stop();
    currentScenePath = path;
    sceneWorld.clear(true);
    prefabLibrary.clear(); // the prefabs are read again: they may have changed on disk

    bool ok = true;
    if (!SDL_GetPathInfo(path.c_str(), nullptr)) {
        SDL_Log("Scene '%s' does not exist: starting empty (Save creates it).", path.c_str());
    } else {
        Json scene;
        std::string error;
        if (!loadSceneFile(path, scene, error) || !restore(scene, error)) {
            SDL_Log("Could not load the scene '%s': %s", path.c_str(), error.c_str());
            ok = false;
        }
    }

    if (editor) editor->onSceneLoaded();
    else play(); // without the editor, play directly
    return ok;
}

bool Application::saveScene() {
    if (playMode != PlayMode::Edit || currentScenePath.empty()) return false;
    return saveSceneFile(currentScenePath, snapshot());
}

Json Application::snapshot() {
    return serializeWorld(sceneWorld, components, &prefabLibrary);
}

bool Application::restore(const Json& scene, std::string& error) {
    return deserializeWorld(sceneWorld, components, &prefabLibrary, scene, error);
}

void Application::play() {
    if (playMode != PlayMode::Edit) return;
    playSnapshot = snapshot();
    playMode = PlayMode::Play;
    // The key or click that started the game must not count as just pressed.
    Input::reset();
    for (auto& s : systemList) s->onPlay(sceneWorld);
}

void Application::stop() {
    if (playMode == PlayMode::Edit) return;
    std::string error;
    if (!restore(playSnapshot, error))
        SDL_Log("Stop: could not restore the scene: %s", error.c_str());
    playSnapshot = Json();
    playMode = PlayMode::Edit;
    // What was playing belonged to the game: stopping cuts it (and leaves any pause).
    audioDevice->stopAll();
    audioDevice->setPaused(false);
}

void Application::setPaused(bool paused) {
    if (playMode == PlayMode::Edit) return;
    playMode = paused ? PlayMode::Paused : PlayMode::Play;
    audioDevice->setPaused(paused);
}

int Application::run() {
    if (!sdlRenderer) return 1;

    // Clock in NANOseconds: with SDL_GetTicks() (milliseconds), at a high framerate there
    // are frames with dt == 0 and physics stops detecting grounding.
    Uint64 lastTime = SDL_GetTicksNS();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - lastTime) / 1000000000.0f;
        lastTime = now;
        if (dt > MAX_DT) dt = MAX_DT;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Closing with unsaved changes: ask first.
            if (event.type == SDL_EVENT_QUIT && (!editor || editor->confirmDiscard("quitting")))
                running = false;
            Input::processEvent(event); // the mouse wheel only arrives as an event
            if (editor) editor->processEvent(event);
        }

        // Keyboard and mouse state for THIS frame, the same for every system. Goes
        // after SDL_PollEvent (which is what refreshes SDL's state). While typing in an
        // editor field, the game does not see the keyboard (what ImGui said in the previous
        // frame applies, which is when the panels were built).
        Input::setKeyboardEnabled(!editor || !editor->isTypingText());
        Input::update();

        if (playMode == PlayMode::Play) {
            for (auto& s : systemList) s->update(sceneWorld, dt);
            sceneWorld.flushDestroyed();
        }

        if (editor) {
            editor->frame(dt); // panels + scene and game views (in textures)
            SDL_SetRenderTarget(sdlRenderer, nullptr);
            SDL_SetRenderDrawColor(sdlRenderer, 20, 20, 24, 255);
            SDL_RenderClear(sdlRenderer);
            editor->present();
        } else {
            renderGameDirect();
        }

        // F9: capture with everything already drawn and BEFORE Present (afterwards, the
        // backbuffer is not guaranteed). Not F12: with the VS debugger, Windows uses it.
        if (Input::wasPressed(Key::F9)) Screenshot::save(sdlRenderer);

        SDL_RenderPresent(sdlRenderer);
    }
    return 0;
}

void Application::renderGameDirect() {
    // The game on the canvas, according to the chosen scale, and the canvas centered in
    // the window (with black bars).
    int winW = 0, winH = 0;
    SDL_GetRenderOutputSize(sdlRenderer, &winW, &winH);
    GameScreen screen;
    bool ok = drawGameScreen(sceneWorld, *assetManager, sdlRenderer, *gameCanvas, appConfig.gameWidth,
                             appConfig.gameHeight, appConfig.scale, (float)winW, (float)winH, screen);
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    if (!ok) return;
    SDL_FRect src{ screen.srcX, screen.srcY, screen.srcW, screen.srcH };
    SDL_FRect dst{ screen.dstX, screen.dstY, screen.dstW, screen.dstH };
    SDL_RenderTexture(sdlRenderer, screen.texture, &src, &dst);
}

void Application::shutdown() {
    // Order: the editor (ImGui and its textures) and the assets are released while the
    // renderer and the libraries are still alive.
    if (editor) {
        editor->shutdown();
        editor.reset();
    }
    sceneWorld.clear(true);
    assetManager.reset();
    gameCanvas.reset();
    if (audioDevice) { audioDevice->shutdown(); audioDevice.reset(); }
    if (sdlRenderer) { SDL_DestroyRenderer(sdlRenderer); sdlRenderer = nullptr; }
    if (sdlWindow) { SDL_DestroyWindow(sdlWindow); sdlWindow = nullptr; }
    if (ttfReady) { TTF_Quit(); ttfReady = false; }
    if (SDL_WasInit(0)) SDL_Quit();
}
