#pragma once
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/Entity.h"

// The World holds ALL the entities and their components. It is the engine's "ECS":
//   - Entity    = a number (see Entity.h).
//   - Component = a data struct with NO logic (Transform, Sprite...).
//   - System    = a function that walks the entities that have certain components
//                 and does something with them (move, draw, collide...).
//
// Each component type lives in its own ComponentPool: a compact vector with the
// data of every entity that has it, plus an entity -> position index.
//
// Typical use:
//     Entity e = world.create();
//     world.add<Transform>(e).position = { 100, 50 };
//     world.add<Sprite>(e).texture = "assets/player.png";
//
//     world.each<Transform, Sprite>([](Entity e, Transform& t, Sprite& s) { ... });
//
// IMPORTANT RULE: the T& references returned by add/get/each are only valid
// RIGHT NOW. Adding a component of that same type to another entity may move the
// vector and leave them dangling. To remember something between frames, store the
// Entity, not a T*.

namespace detail {
    // A distinct number for each component type, assigned the first time it is
    // requested. It is the index into the World's vector of pools.
    inline std::size_t nextComponentTypeId() {
        static std::size_t counter = 0;
        return counter++;
    }
}

template <class T>
std::size_t componentTypeId() {
    static const std::size_t id = detail::nextComponentTypeId();
    return id;
}

// Untyped interface of a pool: the reflection and the World use it to operate on
// components without knowing their type at compile time (the editor, saving).
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual bool has(Entity e) const = 0;
    virtual void* getRaw(Entity e) = 0;
    virtual const void* getRaw(Entity e) const = 0;
    virtual void remove(Entity e) = 0;
    virtual void clear() = 0;
    virtual std::size_t size() const = 0;
};

template <class T>
class ComponentPool : public IComponentPool {
public:
    // If the entity already had it, the value is replaced.
    T& add(Entity e, T value) {
        auto it = index.find(e);
        if (it != index.end()) {
            data[it->second] = std::move(value);
            return data[it->second];
        }
        index[e] = data.size();
        data.push_back(std::move(value));
        owners.push_back(e);
        return data.back();
    }

    T* get(Entity e) {
        auto it = index.find(e);
        return it == index.end() ? nullptr : &data[it->second];
    }
    const T* get(Entity e) const {
        auto it = index.find(e);
        return it == index.end() ? nullptr : &data[it->second];
    }

    bool has(Entity e) const override { return index.count(e) != 0; }
    void* getRaw(Entity e) override { return get(e); }
    const void* getRaw(Entity e) const override { return get(e); }

    // Removing = moving the last one into the gap (swap and pop): the vector stays compact.
    void remove(Entity e) override {
        auto it = index.find(e);
        if (it == index.end()) return;
        std::size_t hole = it->second;
        std::size_t last = data.size() - 1;
        if (hole != last) {
            data[hole] = std::move(data[last]);
            owners[hole] = owners[last];
            index[owners[hole]] = hole;
        }
        data.pop_back();
        owners.pop_back();
        index.erase(it);
    }

    void clear() override {
        data.clear();
        owners.clear();
        index.clear();
    }

    std::size_t size() const override { return data.size(); }

    // Direct access for iterating: data[i] is the component of owners[i].
    std::vector<T> data;
    std::vector<Entity> owners;

private:
    std::unordered_map<Entity, std::size_t> index; // entity -> position in data
};

class World {
public:
    World() = default;
    World(const World&) = delete;            // a World is copied by serializing it
    World& operator=(const World&) = delete;

    // --- Entities --------------------------------------------------------------------
    Entity create();
    // Creates the entity with a specific id (when loading a scene). false if it already
    // exists or is NullEntity. New ids continue after the highest one ever used.
    bool createWithId(Entity id);

    // Deferred: the entity stays alive until flushDestroyed() (the loop calls it at the
    // end of each frame). That way a system can destroy while another one iterates.
    void destroy(Entity e);
    // Immediate: for the editor, which is never in the middle of an iteration.
    void destroyNow(Entity e);
    void flushDestroyed();

    bool isAlive(Entity e) const { return aliveSet.count(e) != 0; }

    // Every living entity, in creation order (or the one setOrder left). It is the
    // order of the editor's Hierarchy and of the scene file.
    const std::vector<Entity>& entities() const { return alive; }

    // Changes the order of entities(). 'order' must contain exactly the living
    // entities, each one once; otherwise it does nothing and returns false. Ids do not change.
    bool setOrder(const std::vector<Entity>& order);

    // Clears everything. Ids do NOT start over (except with resetIds = true, when
    // loading a scene from scratch).
    void clear(bool resetIds = false);

    // --- Components ------------------------------------------------------------------
    template <class T>
    T& add(Entity e, T value = T{}) {
        return pool<T>().add(e, std::move(value));
    }

    template <class T>
    T* get(Entity e) {
        auto* p = findPool<T>();
        return p ? p->get(e) : nullptr;
    }
    template <class T>
    const T* get(Entity e) const {
        auto* p = findPool<T>();
        return p ? p->get(e) : nullptr;
    }

    template <class T>
    bool has(Entity e) const {
        auto* p = findPool<T>();
        return p && p->has(e);
    }

    template <class T>
    void remove(Entity e) {
        if (auto* p = findPool<T>()) p->remove(e);
    }

    // Walks the entities that have ALL the requested components and calls
    // fn(Entity, First&, Rest&...). It walks the pool of the first type, so it is
    // best to put the rarest component first (the one the fewest entities have).
    // Inside fn, do not add or remove components of the types being iterated.
    template <class First, class... Rest, class Fn>
    void each(Fn&& fn) {
        auto* first = findPool<First>();
        if (!first) return;
        for (std::size_t i = 0; i < first->owners.size(); ++i) {
            Entity e = first->owners[i];
            if ((has<Rest>(e) && ...)) fn(e, first->data[i], *get<Rest>(e)...);
        }
    }

    // The pool of a type (created if it did not exist).
    template <class T>
    ComponentPool<T>& pool() {
        std::size_t id = componentTypeId<T>();
        if (pools.size() <= id) pools.resize(id + 1);
        if (!pools[id]) pools[id] = std::make_unique<ComponentPool<T>>();
        return static_cast<ComponentPool<T>&>(*pools[id]);
    }

    // Pool by type number, without creating it: nullptr if nobody has used that type yet.
    IComponentPool* findPool(std::size_t typeId) {
        return typeId < pools.size() ? pools[typeId].get() : nullptr;
    }
    const IComponentPool* findPool(std::size_t typeId) const {
        return typeId < pools.size() ? pools[typeId].get() : nullptr;
    }

private:
    template <class T>
    ComponentPool<T>* findPool() {
        return static_cast<ComponentPool<T>*>(findPool(componentTypeId<T>()));
    }
    template <class T>
    const ComponentPool<T>* findPool() const {
        return static_cast<const ComponentPool<T>*>(findPool(componentTypeId<T>()));
    }

    void addAlive(Entity e);

    std::vector<std::unique_ptr<IComponentPool>> pools; // index = componentTypeId<T>()
    std::vector<Entity> alive;                          // creation order
    std::unordered_set<Entity> aliveSet;                // for isAlive in O(1)
    std::vector<Entity> pendingDestroy;
    Entity nextId = 1;
};
