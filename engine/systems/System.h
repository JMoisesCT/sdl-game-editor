#pragma once

class World;

// Un sistema es el COMPORTAMIENTO: recorre las entidades que tienen ciertos
// componentes y las modifica. Los componentes solo guardan datos.
//
// Ejemplo (del lado del juego):
//     class SpinSystem : public System {
//     public:
//         const char* name() const override { return "Spin"; }
//         void update(World& world, float dt) override {
//             world.each<Spin, Transform>([dt](Entity, Spin& s, Transform& t) {
//                 t.rotation += s.speed * dt;
//             });
//         }
//     };
//     app.addSystem(std::make_unique<SpinSystem>());
//
// Los sistemas corren en el orden en que se agregan, y SOLO mientras se juega (Play):
// editando, la escena esta quieta y lo unico que corre es el dibujo.
class System {
public:
    virtual ~System() = default;
    virtual const char* name() const = 0;

    // Se llama al pulsar Play (y al arrancar el juego sin editor), con la escena ya
    // cargada. Lugar para preparar estado que el sistema no guarda en componentes.
    virtual void onPlay(World&) {}

    virtual void update(World& world, float dt) = 0;
};
