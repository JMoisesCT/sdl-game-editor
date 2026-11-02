#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/Application.h"
#include "game/PlatformerControl.h"
#include "systems/Physics.h"
#include "systems/PlatformerMotorSystem.h"

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

    // Registro del juego: sus componentes (app.registry().add<MiComponente>(...)) y sus
    // sistemas. Los sistemas corren en el orden en que se agregan, y aqui el orden es
    // la cadena de cada frame:
    //   teclas -> intencion -> velocidad -> posicion y choques.
    app.addSystem(std::make_unique<PlatformerControlSystem>()); // juego: teclas -> intencion
    app.addSystem(std::make_unique<PlatformerMotorSystem>());   // motor: intencion -> velocidad
    app.addSystem(std::make_unique<PhysicsSystem>());           // motor: movimiento y choques

    app.openScene("assets/scenes/platformer.scene.json");
    return app.run();
}
