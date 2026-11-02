#pragma once
#include "core/Math.h"

class ComponentRegistry;

// Componentes de fisica y de movimiento de personaje. Igual que los basicos, son SOLO
// datos: los mueve PhysicsSystem (RigidBody2D, BoxCollider) y PlatformerMotorSystem
// (PlatformerMotor). Los campos que no se registran en PhysicsComponents.cpp son estado
// de ejecucion: no se guardan ni se ven en el Inspector, y los escriben los sistemas.

// Cuerpo DINAMICO: la fisica lo mueve con su velocidad y su gravedad. Una entidad sin
// RigidBody2D es estatica (una pared, una plataforma): si tiene BoxCollider, frena a
// los cuerpos, pero nunca se mueve sola.
struct RigidBody2D {
    Vec2 velocity;          // px/seg; el valor guardado es la velocidad al empezar a jugar
    float gravity = 980.0f; // px/seg^2 hacia abajo; 0 = sin gravedad (top-down, naves)

    // --- Estado de ejecucion (lo escribe PhysicsSystem) ---
    bool grounded = false;  // apoyado sobre algo solido en el ultimo paso de fisica
    Vec2 previous;          // posicion antes de moverse en el ultimo paso
};

// Rectangulo de colision (AABB) centrado en el Transform. Como el Sprite, se mide en
// pixeles de la IMAGEN y se multiplica por la escala del Transform: si el personaje se
// agranda, su caja tambien. La rotacion no se aplica (la caja nunca gira).
//
// Sin RigidBody2D la caja es un obstaculo fijo; con RigidBody2D, la fisica la separa
// de los tilemaps solidos y de las otras cajas. Un trigger no frena ni es frenado:
// solo se superpone (para recoger, danar, activar...).
//
// DE UN SENTIDO (oneWay): una plataforma fina que solo frena desde ARRIBA. Se la
// atraviesa saltando desde abajo o de costado, y al caer sobre ella se queda parado.
struct BoxCollider {
    Vec2 size{ 16.0f, 16.0f }; // ancho y alto (px de la imagen)
    Vec2 offset;               // corrimiento del centro de la caja respecto al Transform
    bool isTrigger = false;
    bool oneWay = false;       // solo frena a lo que cae desde arriba
};

// Movimiento de personaje de PLATAFORMAS: aceleracion y frenado, salto con altura en
// pixeles y variable (soltar el boton lo corta), coyote time, jump buffer y saltos en
// el aire. Necesita un RigidBody2D en la misma entidad, con gravedad.
//
// No lee el teclado: cada frame alguien le escribe la INTENCION (moveInput, jumpPressed,
// jumpHeld) y PlatformerMotorSystem la traduce en velocidad. Ese alguien es un sistema
// del juego (ver game/PlatformerControl), una IA o una repeticion: el motor no cambia.
struct PlatformerMotor {
    // --- Ajustes ("game feel"); se guardan y se ven en el Inspector ---
    float maxSpeed = 250.0f;   // px/seg al correr
    float accel = 2200.0f;     // px/seg^2 para llegar a maxSpeed en el suelo
    float decel = 2600.0f;     // px/seg^2 para frenar en el suelo al soltar
    float airAccel = 1400.0f;  // lo mismo en el aire (menos control = mas peso)
    float airDecel = 700.0f;
    float jumpHeight = 150.0f; // altura del salto completo en px (no una velocidad)
    float coyoteTime = 0.10f;  // seg para saltar aun despues de dejar el borde
    float jumpBufferTime = 0.12f; // seg que se recuerda un salto pulsado antes de aterrizar
    float cutJumpMultiplier = 0.45f; // al soltar subiendo, la velocidad se multiplica por esto
    float maxFallSpeed = 900.0f; // velocidad maxima de caida (0 = sin limite)
    int maxAirJumps = 0;       // saltos extra en el aire (1 = doble salto)

    // --- Intencion: la escribe el juego CADA frame ---
    float moveInput = 0.0f;    // -1 izquierda, 0 quieto, +1 derecha (valen intermedios)
    bool jumpPressed = false;  // el boton de salto se ACABA de pulsar en este frame
    bool jumpHeld = false;     // el boton de salto sigue pulsado

    // --- Resultado: lo escribe PlatformerMotorSystem (sirve para la animacion) ---
    bool grounded = false;     // apoyo suavizado por el coyote time (ignora microcortes)
    bool justLanded = false;   // solo en el frame en que toca el suelo
    bool justJumped = false;   // solo en el frame del despegue

    // --- Estado interno de PlatformerMotorSystem ---
    float coyote = 0.0f;       // tiempo que queda de la ventana de coyote
    float buffer = 0.0f;       // tiempo que queda del salto adelantado
    int airJumpsLeft = 0;
    bool jumping = false;      // salto en curso al que aun se le puede cortar la altura
    bool wasGrounded = false;
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// despues de registerCoreComponents.
void registerPhysicsComponents(ComponentRegistry& registry);
