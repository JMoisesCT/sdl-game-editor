#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/Entity.h"
#include "systems/System.h"

class World;
class AssetManager;

// Sistemas de ANIMACION (componentes en components/PresentationComponents.h):
//   AnimatorStateMachineSystem -> elige el clip segun las reglas de cada entidad
//   SpriteAnimatorSystem       -> avanza los cuadros y escribe el Sprite
// Van en ese orden y DESPUES de la fisica y las reglas (ver main.cpp): las
// condiciones miran el estado ya resuelto de este frame.

// Una condicion: dada la entidad, si se cumple ahora. La usan las reglas del
// AnimatorStateMachine por su NOMBRE, asi la regla se arma en el editor y la logica
// queda en C++.
using AnimationCondition = std::function<bool(World&, Entity)>;

// Las condiciones con nombre. Application es duena de una (animationConditions()) y
// le registra las del motor; el juego suma las suyas antes de jugar:
//     app.animationConditions().add("attacking", [](World& w, Entity e) {
//         const Weapon* weapon = w.get<Weapon>(e);
//         return weapon && weapon->cooldown > 0.0f;
//     });
class AnimationConditions {
public:
    // Agrega (o reemplaza) una condicion.
    void add(const std::string& name, AnimationCondition condition) { conditions[name] = std::move(condition); }
    // nullptr si no existe.
    const AnimationCondition* find(const std::string& name) const;
    // Los nombres, en orden alfabetico (para el Inspector).
    std::vector<std::string> names() const;

private:
    std::map<std::string, AnimationCondition> conditions;
};

// Las del motor. Las que hablan de movimiento miran RigidBody2D (y el PlatformerMotor,
// si hay, para el apoyo suavizado por el coyote time); las de reglas, sus componentes:
//   grounded  apoyado                    airborne  en el aire
//   rising    en el aire y subiendo      falling   en el aire y bajando
//   moving    se mueve (velocidad > 1 px/seg)
//   dead      Health en 0                hurt      en la gracia tras un golpe (vivo)
//   collected Collectible ya recogido    activated Checkpoint ya activado
void registerEngineAnimationConditions(AnimationConditions& conditions);

// Hace sonar ese clip del SpriteAnimator de la entidad. Si ya esta sonando no hace nada
// (se puede llamar cada frame sin cortarlo), salvo con restart = true (repetir un
// golpe). false si la entidad no tiene SpriteAnimator o no tiene ese clip.
bool playAnimation(World& world, Entity e, const std::string& clip, bool restart = false);

class AnimatorStateMachineSystem : public System {
public:
    explicit AnimatorStateMachineSystem(const AnimationConditions& conditions) : conditions(conditions) {}
    const char* name() const override { return "AnimatorStateMachine"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    const AnimationConditions& conditions;
};

class SpriteAnimatorSystem : public System {
public:
    // Necesita las imagenes para saber cuantos cuadros tiene un clip sin 'frames'.
    explicit SpriteAnimatorSystem(AssetManager& assets) : assets(assets) {}
    const char* name() const override { return "SpriteAnimator"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    AssetManager& assets;
};
