#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/Application.h"

// La aplicacion es duena del bucle: aqui solo se configura, se registran las piezas
// del juego (componentes y sistemas) y se abre la escena.
//
// Con el argumento --play arranca sin editor, jugando la escena directamente (como
// lo veria el jugador).
int main(int argc, char* argv[]) {
    AppConfig config;
    config.title = "SDL Game Editor";
    config.gameWidth = 1280;
    config.gameHeight = 720;
    for (int i = 1; i < argc; ++i)
        if (SDL_strcmp(argv[i], "--play") == 0) config.editor = false;

    Application app;
    if (!app.init(config)) return 1;

    // Aqui ira el registro del juego: app.registry().add<MiComponente>(...) y
    // app.addSystem(...). Todavia no hay sistemas de juego (fase 1: nucleo + editor).

    app.openScene("assets/scenes/platformer.scene.json");
    return app.run();
}
