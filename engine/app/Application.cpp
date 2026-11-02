#include "app/Application.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>

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

// Techo del paso de tiempo. Si un frame tarda mas que esto (arrastrar la ventana,
// parar en el depurador), se trata como 50 ms: mejor un instante en camara lenta que
// un salto de cientos de pixeles que atraviese las paredes.
static const float MAX_DT = 0.05f;

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::init(const AppConfig& config) {
    appConfig = config;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
        return false;
    }
    // SDL3_ttf necesita inicializarse una vez (SDL3_image no lo requiere).
    if (!TTF_Init()) {
        SDL_Log("Error al inicializar SDL_ttf: %s", SDL_GetError());
        shutdown();
        return false;
    }
    ttfReady = true;

    // Con editor, ventana grande y redimensionable (se maximiza); sin editor, del
    // tamano del juego.
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (config.editor) flags |= SDL_WINDOW_MAXIMIZED;
    int w = config.editor ? 1600 : config.gameWidth;
    int h = config.editor ? 900 : config.gameHeight;
    sdlWindow = SDL_CreateWindow(config.title.c_str(), w, h, flags);
    if (!sdlWindow) {
        SDL_Log("Error al crear la ventana: %s", SDL_GetError());
        shutdown();
        return false;
    }
    sdlRenderer = SDL_CreateRenderer(sdlWindow, nullptr);
    if (!sdlRenderer) {
        SDL_Log("Error al crear el renderer: %s", SDL_GetError());
        shutdown();
        return false;
    }
    // VSync: el editor no necesita 2000 fps quemando la CPU.
    SDL_SetRenderVSync(sdlRenderer, 1);

    // Sin editor, el juego se dibuja a su resolucion en un lienzo que se escala a la
    // ventana (con bandas negras si la proporcion no coincide; ver GameCanvas).
    gameCanvas = std::make_unique<GameCanvas>(sdlRenderer);

    // Sin dispositivo de audio se sigue igual, mudo (el motivo va al log).
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
        if (!editor->init()) editor.reset(); // sin paneles no hay editor: se juega directo
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
    prefabLibrary.clear(); // los prefabs se vuelven a leer: pueden haber cambiado en disco

    bool ok = true;
    if (!SDL_GetPathInfo(path.c_str(), nullptr)) {
        SDL_Log("Escena '%s' no existe: se empieza vacia (Guardar la crea).", path.c_str());
    } else {
        Json scene;
        std::string error;
        if (!loadSceneFile(path, scene, error) || !restore(scene, error)) {
            SDL_Log("No se pudo cargar la escena '%s': %s", path.c_str(), error.c_str());
            ok = false;
        }
    }

    if (editor) editor->onSceneLoaded();
    else play(); // sin editor se juega directo
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
    // La tecla o el clic que inicio el juego no debe contar como recien pulsado.
    Input::reset();
    for (auto& s : systemList) s->onPlay(sceneWorld);
}

void Application::stop() {
    if (playMode == PlayMode::Edit) return;
    std::string error;
    if (!restore(playSnapshot, error))
        SDL_Log("Stop: no se pudo restaurar la escena: %s", error.c_str());
    playSnapshot = Json();
    playMode = PlayMode::Edit;
    // Lo que sonaba era del juego: al detener se corta (y se sale de una pausa).
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

    // Reloj en NANOsegundos: con SDL_GetTicks() (milisegundos), a framerate alto salen
    // frames con dt == 0 y la fisica deja de detectar el apoyo.
    Uint64 lastTime = SDL_GetTicksNS();
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - lastTime) / 1000000000.0f;
        lastTime = now;
        if (dt > MAX_DT) dt = MAX_DT;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Cerrar con cambios sin guardar: primero se pregunta.
            if (event.type == SDL_EVENT_QUIT && (!editor || editor->confirmDiscard("salir")))
                running = false;
            Input::processEvent(event); // la rueda del raton solo llega como evento
            if (editor) editor->processEvent(event);
        }

        // Estado de teclado y raton de ESTE frame, igual para todos los sistemas. Va
        // despues de SDL_PollEvent (que es lo que refresca el estado de SDL).
        Input::update();

        if (playMode == PlayMode::Play) {
            for (auto& s : systemList) s->update(sceneWorld, dt);
            sceneWorld.flushDestroyed();
        }

        if (editor) {
            editor->frame(dt); // paneles + vistas de escena y juego (en texturas)
            SDL_SetRenderTarget(sdlRenderer, nullptr);
            SDL_SetRenderDrawColor(sdlRenderer, 20, 20, 24, 255);
            SDL_RenderClear(sdlRenderer);
            editor->present();
        } else {
            renderGameDirect();
        }

        // F9: captura con todo ya dibujado y ANTES del Present (despues, el backbuffer
        // no esta garantizado). No F12: con el depurador de VS, Windows la usa.
        if (Input::wasPressed(Key::F9)) Screenshot::save(sdlRenderer);

        SDL_RenderPresent(sdlRenderer);
    }
    return 0;
}

void Application::renderGameDirect() {
    // El juego, a su resolucion, en el lienzo.
    if (!gameCanvas->begin(appConfig.gameWidth, appConfig.gameHeight)) return;
    View view;
    Color bg;
    findCameraView(sceneWorld, appConfig.gameWidth, appConfig.gameHeight, view, &bg);
    SDL_SetRenderDrawColor(sdlRenderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderClear(sdlRenderer);
    renderWorld(sceneWorld, *assetManager, sdlRenderer, view);
    renderScreenTexts(sceneWorld, *assetManager, sdlRenderer, screenView(appConfig.gameWidth, appConfig.gameHeight));

    // El lienzo, lo mas grande que quepa en la ventana y centrado (con bandas negras).
    int winW = 0, winH = 0;
    SDL_GetRenderOutputSize(sdlRenderer, &winW, &winH);
    float k = std::fmin((float)winW / appConfig.gameWidth, (float)winH / appConfig.gameHeight);
    float w = std::round(appConfig.gameWidth * k), h = std::round(appConfig.gameHeight * k);
    SDL_Texture* image = gameCanvas->scaledFor(w, h); // deja el destino en la ventana
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);
    if (!image || w < 1.0f || h < 1.0f) return;
    SDL_FRect dst{ std::round((winW - w) * 0.5f), std::round((winH - h) * 0.5f), w, h };
    SDL_RenderTexture(sdlRenderer, image, nullptr, &dst);
}

void Application::shutdown() {
    // Orden: el editor (ImGui y sus texturas) y los assets se liberan con el renderer
    // y las librerias aun vivas.
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
