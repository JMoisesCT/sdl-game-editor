#pragma once
#include <string>

#include "core/Entity.h"
#include "core/Math.h"

class ComponentRegistry;

// Componentes de REGLAS de juego: vida, dano, zonas que matan, objetos que se recogen
// y puntos de reaparicion. Como los demas, son SOLO datos; la logica esta en los
// sistemas de systems/Rules.h.
//
// Todos se activan por CONTACTO entre cajas (BoxCollider): el peligro, la fruta o la
// bandera necesitan su caja (normalmente isTrigger, para que no frene) y el que los
// toca tambien. A quien afectan se decide por la ETIQUETA (EntityInfo::tag) del que
// toca, nunca por su nombre.
//
// No hay callbacks como en el v1 (onDeath, onCollect...): lo que pasa en un frame
// queda escrito en el componente como un aviso de UN frame (justDamaged, justDied,
// justCollected...), igual que justLanded en el PlatformerMotor. Un sistema del juego
// que corra despues lo lee y decide que significa (sumar puntos, un sonido, el HUD).

// Puntos de vida, dano e invulnerabilidad temporal con parpadeo. No sabe quien le
// pega: se le quita vida con damageEntity / killEntity (Rules.h).
//
// INVULNERABILIDAD: tras un golpe, no recibe otro durante invulnerabilityTime. Sin
// eso, quedarse encima de unos pinchos vaciaria la vida en un punado de frames, porque
// el contacto se detecta CADA frame que dure. Mientras dura, el Sprite parpadea.
//
// Al llegar a 0: si la entidad tiene Respawn, reaparece; si no y destroyOnDeath esta
// marcado, se destruye (un enemigo); si no, queda muerta (hp 0) y no recibe mas dano.
struct Health {
    int maxHP = 3;
    float invulnerabilityTime = 1.0f; // seg de gracia tras un golpe (0 = sin gracia)
    float blinkInterval = 0.08f;      // seg entre apagar y encender el sprite
    bool destroyOnDeath = false;      // al morir se destruye (no usar junto con Respawn)

    // --- Estado de ejecucion (lo escriben los sistemas de Rules.h) ---
    int hp = 0;                // vida actual; empieza en maxHP
    float invulnerable = 0.0f; // seg de gracia que quedan
    float blinkTimer = 0.0f;
    bool started = false;      // hp ya se lleno con maxHP
    bool justDamaged = false;  // recibio dano en este frame
    bool justDied = false;     // llego a 0 en este frame
};

// Hace dano a lo que lo toque, si lleva la etiqueta targetTag y tiene Health: pinchos,
// sierras, fuego, un enemigo que dana al contacto.
//
// EMPUJON: al golpear, lanza al objetivo (si tiene RigidBody2D) hacia el lado
// contrario al del peligro y hacia arriba. No es adorno: sin el, el jugador se queda
// encima del peligro y vuelve a cobrar en cuanto se le acaba la invulnerabilidad.
struct Hazard {
    int damage = 1;
    std::string targetTag = "Player";
    Vec2 knockback{ 240.0f, 320.0f }; // px/seg: x alejandose del peligro, y hacia arriba
    bool repeat = true;               // false: dana una sola vez (una trampa de un uso)

    // --- Estado de ejecucion ---
    bool used = false;
};

// Mata en el acto a lo que entre (si lleva targetTag y tiene Health), aunque este
// invulnerable: una franja bajo el nivel para que caerse no deje al jugador cayendo
// para siempre, lava, un pozo. La caja suele ser grande y trigger.
struct KillZone {
    std::string targetTag = "Player";
};

// Se recoge al tocarlo: monedas, frutas, llaves. El motor NO lleva ninguna cuenta: que
// significa recogerlo lo decide un sistema del juego mirando justCollected (en ese
// frame la entidad aun existe, aunque ya este marcada para destruirse).
struct Collectible {
    std::string collectorTag = "Player"; // quien puede recogerlo
    bool destroyOnCollect = true;        // false: se queda (un interruptor que se pisa)

    // --- Estado de ejecucion ---
    bool collected = false;        // ya se recogio (una sola vez)
    bool justCollected = false;    // se recogio en este frame
    Entity collector = NullEntity; // quien lo recogio
};

// Bandera: al tocarla, fija el punto de reaparicion del que la toca (que necesita un
// Respawn; la bandera no guarda nada). Se activa una sola vez.
struct Checkpoint {
    std::string targetTag = "Player";
    Vec2 offset;               // punto de reaparicion respecto al centro (px de la imagen x escala)
    std::string activeTexture; // al activarse, el Sprite pasa a esta imagen (vacio = no cambia)

    // --- Estado de ejecucion ---
    bool activated = false;
    bool justActivated = false;
};

// Va en el JUGADOR: al morir (Health en 0), espera 'delay' segundos congelado y vuelve
// al ultimo punto de reaparicion con la vida llena. El punto empieza siendo donde
// estaba al empezar a jugar; un Checkpoint lo cambia.
struct Respawn {
    float delay = 0.6f; // seg entre la muerte y la reaparicion (deja ver la caida)

    // --- Estado de ejecucion ---
    Vec2 point;                 // donde reaparece
    bool started = false;       // point ya se tomo de la posicion inicial
    bool dying = false;         // muerto, esperando
    float timer = 0.0f;         // seg que faltan para reaparecer
    bool justRespawned = false; // reaparecio en este frame
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// despues de registerPhysicsComponents.
void registerRuleComponents(ComponentRegistry& registry);
