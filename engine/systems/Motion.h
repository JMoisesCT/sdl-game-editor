#pragma once
#include "systems/System.h"

class World;

// Sistemas de MECANISMOS (componentes en components/MotionComponents.h). Van asi en
// el frame (ver main.cpp):
//   PathMoverSystem, FallingPlatformSystem -> ANTES de PhysicsSystem: mueven las
//       plataformas y a sus pasajeros (moveCarrying), y la fisica resuelve despues
//       los choques con todo ya en su lugar.
//   BouncerSystem -> DESPUES de PhysicsSystem: mira quien cayo sobre el trampolin con
//       las posiciones ya resueltas, y su velocidad nueva se aplica en el frame
//       siguiente.

// Mueve las entidades con PathMover por su recorrido.
class PathMoverSystem : public System {
public:
    const char* name() const override { return "PathMover"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Hace caer las FallingPlatform pisadas y las devuelve a su lugar.
class FallingPlatformSystem : public System {
public:
    const char* name() const override { return "FallingPlatform"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Lanza hacia arriba a los cuerpos que caen sobre un Bouncer.
class BouncerSystem : public System {
public:
    const char* name() const override { return "Bouncer"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
