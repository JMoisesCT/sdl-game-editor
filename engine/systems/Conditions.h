#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/Entity.h"

class World;

// CONDICIONES con nombre: dada una entidad, si algo se cumple AHORA. Las usan los
// componentes que reaccionan al estado de una entidad sin que nadie escriba codigo para
// ellos, eligiendo la condicion por su nombre en el editor:
//   - AnimatorStateMachine: "suena el clip jump mientras se cumpla rising".
//   - SoundEffects: "suena jump.wav cuando empiece a cumplirse jumped".
// Asi la regla se arma en el editor y la logica queda en C++.
using Condition = std::function<bool(World&, Entity)>;

// Application es duena de un registro (conditions()) y le carga las del motor; el
// juego suma las suyas antes de jugar:
//     app.conditions().add("attacking", [](World& w, Entity e) {
//         const Weapon* weapon = w.get<Weapon>(e);
//         return weapon && weapon->cooldown > 0.0f;
//     });
class ConditionRegistry {
public:
    // Agrega (o reemplaza) una condicion.
    void add(const std::string& name, Condition condition) { conditions[name] = std::move(condition); }
    // nullptr si no existe.
    const Condition* find(const std::string& name) const;
    // Los nombres, en orden alfabetico (para el Inspector).
    std::vector<std::string> names() const;

private:
    std::map<std::string, Condition> conditions;
};

// Las del motor. Las de movimiento miran RigidBody2D (y el PlatformerMotor, si hay,
// para el apoyo suavizado por el coyote time); las de reglas, sus componentes.
//   Estados (duran mientras pasa):
//     grounded  apoyado                    airborne  en el aire
//     rising    en el aire y subiendo      falling   en el aire y bajando
//     moving    se mueve (velocidad > 1 px/seg)
//     dead      Health en 0                hurt      en la gracia tras un golpe (vivo)
//     collected Collectible ya recogido    activated Checkpoint ya activado
//   Sucesos (un solo frame, de los avisos justX):
//     jumped    despego (PlatformerMotor)  landed    aterrizo (PlatformerMotor)
//     damaged   recibio un golpe (Health)  respawned reaparecio (Respawn)
void registerEngineConditions(ConditionRegistry& conditions);
