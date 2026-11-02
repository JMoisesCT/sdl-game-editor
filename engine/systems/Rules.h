#pragma once
#include "core/Entity.h"
#include "systems/System.h"

class World;

// Sistemas de REGLAS (componentes en components/RuleComponents.h). Todos funcionan
// igual: buscan los contactos entre la caja de la regla (el peligro, la fruta, la
// bandera) y las cajas de las entidades con la etiqueta que les interesa, y actuan.
//
// Van DESPUES de PhysicsSystem (miran las posiciones ya resueltas) y en este orden
// (ver main.cpp):
//   HealthSystem      -> cuenta la invulnerabilidad, parpadea y apaga los avisos
//                        de un frame (justDamaged, justDied) del frame anterior
//   HazardSystem      -> dano + empujon
//   KillZoneSystem    -> muerte en el acto
//   CollectibleSystem -> recoger
//   CheckpointSystem  -> fijar el punto de reaparicion
//   RespawnSystem     -> esperar y reaparecer a los muertos
// HealthSystem va PRIMERO porque es el que borra los avisos: asi un golpe de este
// frame sigue marcado hasta el final del frame, para los sistemas del juego.

// Margen de contacto (px): dos cajas solidas que la fisica deja borde con borde (un
// enemigo solido contra el jugador) tambien cuentan como tocandose.
constexpr float RULE_CONTACT_MARGIN = 1.0f;

// Quita vida respetando la invulnerabilidad. true si el golpe conto. Si la vida llega
// a 0 la entidad muere (ver Health). Sin Health no hace nada.
bool damageEntity(World& world, Entity target, int amount);
// Mata en el acto, aunque este invulnerable (caer al vacio). Sin Health no hace nada.
void killEntity(World& world, Entity target);
// Devuelve vida, sin pasar de maxHP. A un muerto no lo cura.
void healEntity(World& world, Entity target, int amount);
// true si tiene Health y su vida llego a 0 (esperando reaparecer, o sin Respawn).
bool isDead(World& world, Entity e);

class HealthSystem : public System {
public:
    const char* name() const override { return "Health"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class HazardSystem : public System {
public:
    const char* name() const override { return "Hazard"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class KillZoneSystem : public System {
public:
    const char* name() const override { return "KillZone"; }
    void update(World& world, float dt) override;
};

class CollectibleSystem : public System {
public:
    const char* name() const override { return "Collectible"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class CheckpointSystem : public System {
public:
    const char* name() const override { return "Checkpoint"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

class RespawnSystem : public System {
public:
    const char* name() const override { return "Respawn"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
