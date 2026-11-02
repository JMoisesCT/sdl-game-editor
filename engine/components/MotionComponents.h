#pragma once
#include <string>
#include <vector>

#include "core/Math.h"

class ComponentRegistry;

// Componentes de MECANISMOS del escenario: cosas que se mueven solas (plataformas,
// sierras), plataformas que caen al pisarlas y trampolines. Como los demas, son SOLO
// datos; la logica esta en systems/Motion.h.
//
// Las plataformas que mueven estos componentes son CINEMATICAS: no llevan RigidBody2D
// (la fisica no las empuja ni les aplica gravedad), las mueve su sistema, y llevan
// consigo a los cuerpos parados encima (moveCarrying, en Physics.h).

// Un punto del recorrido de un PathMover, relativo a donde empieza la entidad.
struct PathPoint {
    Vec2 offset; // px del mundo respecto a la posicion inicial
};

// Recorre una lista de puntos a velocidad constante, esperando un rato en cada uno:
// una plataforma que va y viene, una sierra que patrulla. El primer punto del
// recorrido es la posicion inicial de la entidad (no hace falta ponerlo en la lista).
//
// Con loop, al llegar al ultimo punto vuelve al primero y repite (un circuito); sin
// loop, recorre la lista de ida y de vuelta.
struct PathMover {
    float speed = 80.0f;   // px/seg
    float waitTime = 0.5f; // seg de pausa en cada punto
    bool loop = false;     // true: circuito cerrado; false: ida y vuelta
    std::vector<PathPoint> points;

    // --- Estado de ejecucion (lo escribe PathMoverSystem) ---
    bool started = false;
    Vec2 origin;          // posicion al empezar: los puntos se miden desde aqui
    int target = 1;       // indice del punto hacia el que va (0 = el origen)
    int step = 1;         // +1 o -1 (ida y vuelta)
    float wait = 0.0f;    // seg de pausa que quedan
};

// Plataforma que CAE al pisarla: cuando un cuerpo se para encima, espera 'delay'
// segundos, cae con gravedad (llevandose al que siga encima) y, pasados respawnTime
// segundos desde que empezo a caer, vuelve a su lugar. Necesita su BoxCollider solido
// (normalmente oneWay) y no lleva RigidBody2D.
struct FallingPlatform {
    float delay = 0.5f;          // seg entre que la pisan y que cae
    float gravity = 980.0f;      // px/seg^2 al caer
    float maxFallSpeed = 700.0f; // px/seg
    float respawnTime = 3.0f;    // seg desde que empieza a caer hasta volver (0 = no vuelve)

    // --- Estado de ejecucion (lo escribe FallingPlatformSystem) ---
    bool started = false;
    Vec2 origin;               // donde vuelve
    bool triggered = false;    // la pisaron: esta por caer o cayendo
    bool falling = false;
    float timer = 0.0f;        // seg en el estado actual
    float speed = 0.0f;        // px/seg de caida
};

// Trampolin: lanza hacia arriba a un cuerpo que le CAE encima, a la altura que se pida
// (en px, como el salto del PlatformerMotor). La caja suele ser trigger: asi el
// trampolin no es "suelo" y el motor no confunde el rebote con un salto propio.
struct Bouncer {
    float bounceHeight = 260.0f; // px que sube el cuerpo lanzado
    std::string targetTag;       // a quien lanza (vacio = a cualquier RigidBody2D)

    // --- Estado de ejecucion (lo escribe BouncerSystem) ---
    bool justBounced = false; // lanzo a alguien en este frame
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// despues de registerRuleComponents.
void registerMotionComponents(ComponentRegistry& registry);
