#pragma once
#include <string>

#include "systems/System.h"

class ComponentRegistry;

// Codigo del JUEGO (no del motor): las reglas propias de ESTE plataformas, armadas
// sobre las piezas genericas del motor (Collectible, Health...). Sirve de ejemplo de
// como un juego suma SUS componentes y sistemas sin tocar el motor.

// Meta del nivel: al tocarla quien lleva targetTag, el nivel se completa. Necesita un
// BoxCollider (trigger). Es del juego porque "ganar" no significa lo mismo en todos.
struct LevelGoal {
    std::string targetTag = "Player";

    // --- Estado de ejecucion ---
    bool reached = false;
};

// Declara los componentes del juego a la reflexion: aparecen en "Agregar componente"
// del editor y se guardan en la escena como los del motor. main.cpp la llama antes de
// abrir la escena.
void registerPlatformerComponents(ComponentRegistry& registry);

// La partida: cuenta las frutas (Collectible con tag "Pickup") que hay y las que se
// recogieron, y si se llego a la meta. El motor no lleva ninguna cuenta: este sistema
// lee los avisos de un frame (justCollected) y decide que significan. De momento lo
// cuenta en el log; el HUD llega en la fase 5 y leera fruits(), totalFruits()...
//
// Va DESPUES de los sistemas de reglas del motor (ver main.cpp), para ver sus avisos
// en el mismo frame.
class PlatformerRulesSystem : public System {
public:
    const char* name() const override { return "PlatformerRules"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

    int fruits() const { return collected; }
    int totalFruits() const { return total; }
    bool levelDone() const { return done; }

private:
    int collected = 0;
    int total = 0;
    bool done = false;
};
