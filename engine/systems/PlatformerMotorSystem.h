#pragma once
#include "systems/System.h"

struct PlatformerMotor;
struct RigidBody2D;

// Traduce la INTENCION de cada PlatformerMotor (moveInput, jumpPressed, jumpHeld) en
// la velocidad de su RigidBody2D: acelera y frena hacia la velocidad deseada, salta
// (con coyote time y jump buffer), corta el salto al soltar y limita la caida.
//
// ORDEN de los sistemas en cada frame:
//     control del juego (teclas -> intencion)
//       -> PlatformerMotorSystem (intencion -> velocidad)
//         -> PhysicsSystem (velocidad -> posicion, choques y 'grounded')
// El motor lee el 'grounded' que dejo la fisica en el frame anterior. Si el control
// fuera despues del motor, la tecla llegaria un frame tarde.
class PlatformerMotorSystem : public System {
public:
    const char* name() const override { return "PlatformerMotor"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Velocidad inicial del salto completo, deducida de jumpHeight y de la gravedad del
// cuerpo: para subir h px con gravedad g hay que salir a raiz(2 * g * h). Por eso el
// ajuste es una ALTURA: si cambia la gravedad, el salto sigue llegando igual de alto.
float platformerJumpSpeed(const PlatformerMotor& motor, const RigidBody2D& rb);
