#pragma once
#include "systems/System.h"

class World;

// Sistemas de PRESENTACION que no son animacion (componentes en
// components/PresentationComponents.h). Van al final del frame (ver main.cpp): la
// camara sigue la posicion ya resuelta por la fisica y las reglas (una reaparicion
// incluida).

// Mueve las camaras con FollowCamera. Necesita el tamano de la vista del juego (la
// resolucion de AppConfig) para que, con clampToTilemaps, el RECTANGULO VISIBLE no se
// salga del mapa.
class FollowCameraSystem : public System {
public:
    FollowCameraSystem(int viewWidth, int viewHeight) : viewWidth(viewWidth), viewHeight(viewHeight) {}
    const char* name() const override { return "FollowCamera"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    int viewWidth;
    int viewHeight;
};

// Avanza el desplazamiento propio (scroll) de los ParallaxBackground.
class ParallaxSystem : public System {
public:
    const char* name() const override { return "Parallax"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
