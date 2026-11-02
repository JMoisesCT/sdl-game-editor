#pragma once
#include "systems/System.h"

// Codigo del JUEGO (no del motor): traduce TECLAS a INTENCION para el PlatformerMotor
// de las entidades con tag "Player". Eso es todo lo que hace: no sabe de gravedad, de
// coyote time ni de saltos dobles; de eso se ocupa el motor, que sirve para cualquier
// plataformas. Cambiar los controles (o enchufar un mando, o una IA) es cambiar solo
// este sistema.
//
// Controles: flechas o A/D para moverse; Espacio, W o flecha arriba para saltar.
//
// Va ANTES de PlatformerMotorSystem (ver main.cpp): si fuera despues, la tecla llegaria
// al motor un frame tarde.
class PlatformerControlSystem : public System {
public:
    const char* name() const override { return "PlatformerControl"; }
    void update(World& world, float dt) override;
};
