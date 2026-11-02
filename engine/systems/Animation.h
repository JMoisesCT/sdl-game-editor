#pragma once
#include <string>

#include "core/Entity.h"
#include "systems/Conditions.h"
#include "systems/System.h"

class World;
class AssetManager;

// Sistemas de ANIMACION (componentes en components/PresentationComponents.h):
//   AnimatorStateMachineSystem -> elige el clip segun las reglas de cada entidad
//   SpriteAnimatorSystem       -> avanza los cuadros y escribe el Sprite
// Van en ese orden y DESPUES de la fisica y las reglas (ver main.cpp): las
// condiciones (systems/Conditions.h) miran el estado ya resuelto de este frame.

// Hace sonar ese clip del SpriteAnimator de la entidad. Si ya esta sonando no hace nada
// (se puede llamar cada frame sin cortarlo), salvo con restart = true (repetir un
// golpe). false si la entidad no tiene SpriteAnimator o no tiene ese clip.
bool playAnimation(World& world, Entity e, const std::string& clip, bool restart = false);

class AnimatorStateMachineSystem : public System {
public:
    explicit AnimatorStateMachineSystem(const ConditionRegistry& conditions) : conditions(conditions) {}
    const char* name() const override { return "AnimatorStateMachine"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    const ConditionRegistry& conditions;
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
