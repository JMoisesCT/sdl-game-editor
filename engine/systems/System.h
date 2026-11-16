#pragma once

class World;

// A system is the BEHAVIOR: it walks the entities that have certain components and
// modifies them. Components only store data.
//
// Example (on the game side):
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
// Systems run in the order they are added, and ONLY while playing (Play): while
// editing, the scene stands still and the only thing that runs is the drawing.
class System {
public:
    virtual ~System() = default;
    virtual const char* name() const = 0;

    // Called when pressing Play (and when the game starts without the editor), with the
    // scene already loaded. The place to prepare state the system does not keep in components.
    virtual void onPlay(World&) {}

    virtual void update(World& world, float dt) = 0;
};
