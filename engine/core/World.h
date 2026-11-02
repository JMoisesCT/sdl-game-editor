#pragma once
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/Entity.h"

// El World guarda TODAS las entidades y sus componentes. Es el "ECS" del motor:
//   - Entity    = un numero (ver Entity.h).
//   - Componente = un struct de datos SIN logica (Transform, Sprite...).
//   - Sistema   = una funcion que recorre las entidades que tienen ciertos componentes
//                 y hace algo con ellos (mover, dibujar, colisionar...).
//
// Cada tipo de componente vive en su propio ComponentPool: un vector compacto con los
// datos de todas las entidades que lo tienen, mas un indice entidad -> posicion.
//
// Uso tipico:
//     Entity e = world.create();
//     world.add<Transform>(e).position = { 100, 50 };
//     world.add<Sprite>(e).texture = "assets/player.png";
//
//     world.each<Transform, Sprite>([](Entity e, Transform& t, Sprite& s) { ... });
//
// REGLA IMPORTANTE: las referencias T& que devuelven add/get/each son validas solo
// AHORA. Agregar un componente de ese mismo tipo a otra entidad puede mover el vector
// y dejarlas colgando. Para recordar algo entre frames se guarda el Entity, no un T*.

namespace detail {
    // Un numero distinto por cada tipo de componente, asignado la primera vez que se
    // pide. Sirve de indice en el vector de pools del World.
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

// Interfaz sin tipo de un pool: la usan la reflexion y el World para operar sobre
// componentes sin conocer su tipo en tiempo de compilacion (el editor, el guardado).
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
    // Si la entidad ya lo tenia, se reemplaza el valor.
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

    // Quitar = mover el ultimo al hueco (swap and pop): el vector sigue compacto.
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

    // Acceso directo para recorrer: data[i] es el componente de owners[i].
    std::vector<T> data;
    std::vector<Entity> owners;

private:
    std::unordered_map<Entity, std::size_t> index; // entidad -> posicion en data
};

class World {
public:
    World() = default;
    World(const World&) = delete;            // copiar un World se hace serializando
    World& operator=(const World&) = delete;

    // --- Entidades -------------------------------------------------------------------
    Entity create();
    // Crea la entidad con un id concreto (al cargar una escena). false si ya existe o
    // si es NullEntity. Los ids nuevos siguen despues del mayor que se haya usado.
    bool createWithId(Entity id);

    // Diferido: la entidad sigue viva hasta flushDestroyed() (lo llama el bucle al
    // final de cada frame). Asi un sistema puede destruir mientras otro recorre.
    void destroy(Entity e);
    // Inmediato: para el editor, que nunca esta en medio de un recorrido.
    void destroyNow(Entity e);
    void flushDestroyed();

    bool isAlive(Entity e) const { return aliveSet.count(e) != 0; }

    // Todas las entidades vivas, en orden de creacion.
    const std::vector<Entity>& entities() const { return alive; }

    // Borra todo. Los ids NO vuelven a empezar (salvo con resetIds = true, al cargar
    // una escena desde cero).
    void clear(bool resetIds = false);

    // --- Componentes -----------------------------------------------------------------
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

    // Recorre las entidades que tienen TODOS los componentes pedidos y llama a
    // fn(Entity, First&, Rest&...). Se recorre el pool del primer tipo, asi que
    // conviene poner primero el componente mas raro (el que menos entidades tienen).
    // Dentro de fn no se deben agregar ni quitar componentes de los tipos recorridos.
    template <class First, class... Rest, class Fn>
    void each(Fn&& fn) {
        auto* first = findPool<First>();
        if (!first) return;
        for (std::size_t i = 0; i < first->owners.size(); ++i) {
            Entity e = first->owners[i];
            if ((has<Rest>(e) && ...)) fn(e, first->data[i], *get<Rest>(e)...);
        }
    }

    // El pool de un tipo (se crea si no existia).
    template <class T>
    ComponentPool<T>& pool() {
        std::size_t id = componentTypeId<T>();
        if (pools.size() <= id) pools.resize(id + 1);
        if (!pools[id]) pools[id] = std::make_unique<ComponentPool<T>>();
        return static_cast<ComponentPool<T>&>(*pools[id]);
    }

    // Pool por numero de tipo, sin crearlo: nullptr si nadie uso ese tipo todavia.
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

    std::vector<std::unique_ptr<IComponentPool>> pools; // indice = componentTypeId<T>()
    std::vector<Entity> alive;                          // orden de creacion
    std::unordered_set<Entity> aliveSet;                // para isAlive en O(1)
    std::vector<Entity> pendingDestroy;
    Entity nextId = 1;
};
