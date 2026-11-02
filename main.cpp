#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/Application.h"
#include "game/PlatformerControl.h"
#include "game/PlatformerRules.h"
#include "systems/Physics.h"
#include "systems/PlatformerMotorSystem.h"
#include "systems/Rules.h"

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

    // Componentes del juego: antes de abrir la escena, para que se carguen.
    registerPlatformerComponents(app.registry());

    // Sistemas. Corren en el orden en que se agregan, y aqui el orden es la cadena de
    // cada frame:
    //   teclas -> intencion -> velocidad -> posicion y choques -> reglas -> partida.
    app.addSystem(std::make_unique<PlatformerControlSystem>()); // juego: teclas -> intencion
    app.addSystem(std::make_unique<PlatformerMotorSystem>());   // motor: intencion -> velocidad
    app.addSystem(std::make_unique<PhysicsSystem>());           // motor: movimiento y choques
    // Reglas del motor (ver systems/Rules.h): HealthSystem primero, porque apaga los
    // avisos de un frame que los demas encienden.
    app.addSystem(std::make_unique<HealthSystem>());
    app.addSystem(std::make_unique<HazardSystem>());
    app.addSystem(std::make_unique<KillZoneSystem>());
    app.addSystem(std::make_unique<CollectibleSystem>());
    app.addSystem(std::make_unique<CheckpointSystem>());
    app.addSystem(std::make_unique<RespawnSystem>());
    app.addSystem(std::make_unique<PlatformerRulesSystem>());   // juego: frutas y meta

    app.openScene("assets/scenes/platformer.scene.json");
    return app.run();
}
