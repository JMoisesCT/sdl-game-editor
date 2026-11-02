#pragma once
#include "core/Entity.h"
#include "core/Math.h"
#include "systems/System.h"

class World;

// Fisica de cajas (AABB) para juegos 2D de pixel art. Cada paso:
//   1. Mueve los cuerpos (RigidBody2D + Transform): suma la gravedad a la velocidad y
//      la velocidad a la posicion.
//   2. Separa cada cuerpo con BoxCollider de las celdas solidas de los Tilemap.
//   3. Separa los pares de cajas que se superponen (salvo triggers).
//   4. Marca 'grounded' en los cuerpos que quedaron apoyados.
//
// CONTRA LOS TILEMAPS se resuelve EJE POR EJE: primero X (con la Y que el cuerpo
// tenia antes de moverse) y despues Y. Resolver los dos ejes a la vez, por el lado de
// menor penetracion, hace que el personaje se enganche en las COSTURAS entre dos tiles
// vecinos del suelo: una penetracion vertical minima se confunde con un choque lateral
// contra el tile siguiente y lo frena en seco. Separando por eje, la pasada X solo ve
// paredes y la pasada Y solo ve suelos y techos.
//
// APOYO ESTABLE: ademas de la separacion, se mira una franja de GROUND_PROBE px bajo
// los pies. Sin esa sonda el apoyo dependeria de que el cuerpo penetre el suelo en ese
// frame, que a framerate alto es una fraccion de pixel (o nada): 'grounded' parpadearia
// y el salto fallaria a veces.
//
// Va DESPUES de los sistemas que deciden la velocidad (control del jugador, motor de
// plataformas) y antes de los que reaccionan a la posicion final (camara, reglas).
//
// PLATAFORMAS DE UN SENTIDO (BoxCollider::oneWay): solo frenan a un cuerpo que cae y
// que en el paso anterior tenia los pies por encima de la plataforma (con
// ONE_WAY_TOLERANCE de margen, para las que suben y lo alcanzan). Nunca empujan de
// costado ni hacia abajo.
class PhysicsSystem : public System {
public:
    static constexpr float GROUND_PROBE = 2.0f;      // px de mundo bajo los pies
    static constexpr float ONE_WAY_TOLERANCE = 4.0f; // px que los pies pueden estar bajo el borde

    const char* name() const override { return "Physics"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Caja de colision de una entidad en el mundo (min = esquina superior izquierda).
// false si no tiene BoxCollider + Transform.
bool colliderBounds(World& world, Entity e, Vec2& min, Vec2& max);

// Si las cajas de dos entidades se superponen (tocarse borde con borde no cuenta).
// Incluye triggers: sirve para recoger, danar o activar sin pasar por la fisica.
// Con margin > 0 las cajas se agrandan esos px por cada lado: asi cuenta tambien el
// CONTACTO entre dos cajas solidas, que la fisica deja justo borde con borde.
bool collidersOverlap(World& world, Entity a, Entity b, float margin = 0.0f);

// Si algun Tilemap tiene una celda solida dentro de ese rectangulo del mundo.
bool isSolidArea(World& world, Vec2 min, Vec2 max);

// Si 'body' (con RigidBody2D) esta parado sobre la caja solida de 'platform': apoyado
// en el ultimo paso de fisica, sin subir, con los pies en el borde de arriba de la
// plataforma y superpuesto en X.
bool isStandingOn(World& world, Entity body, Entity platform);

// Mueve una entidad CINEMATICA (sin RigidBody2D, la mueve un sistema: una plataforma
// movil, una que cae) y con ella a los cuerpos parados encima. Sin esto la plataforma
// se iria de debajo de los pies (de costado) o los empujaria a saltos (al subir). Se
// llama ANTES de PhysicsSystem, que ya ve a los pasajeros en su nuevo lugar.
void moveCarrying(World& world, Entity platform, Vec2 delta);
