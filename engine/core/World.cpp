#include "core/World.h"

#include <algorithm>

Entity World::create() {
    Entity e = nextId++;
    addAlive(e);
    return e;
}

bool World::createWithId(Entity id) {
    if (id == NullEntity || isAlive(id)) return false;
    addAlive(id);
    if (id >= nextId) nextId = id + 1;
    return true;
}

void World::addAlive(Entity e) {
    alive.push_back(e);
    aliveSet.insert(e);
}

void World::destroy(Entity e) {
    if (isAlive(e)) pendingDestroy.push_back(e);
}

void World::destroyNow(Entity e) {
    if (!isAlive(e)) return;
    for (auto& p : pools)
        if (p) p->remove(e);
    aliveSet.erase(e);
    alive.erase(std::remove(alive.begin(), alive.end(), e), alive.end());
}

void World::flushDestroyed() {
    if (pendingDestroy.empty()) return;
    // Una sola pasada sobre la lista de vivas (y no un erase por entidad): asi
    // destruir muchas balas en el mismo frame no cuesta n^2.
    for (Entity e : pendingDestroy) {
        if (!aliveSet.erase(e)) continue; // ya destruida (pedida dos veces)
        for (auto& p : pools)
            if (p) p->remove(e);
    }
    alive.erase(std::remove_if(alive.begin(), alive.end(),
                               [this](Entity e) { return aliveSet.count(e) == 0; }),
                alive.end());
    pendingDestroy.clear();
}

void World::clear(bool resetIds) {
    for (auto& p : pools)
        if (p) p->clear();
    alive.clear();
    aliveSet.clear();
    pendingDestroy.clear();
    if (resetIds) nextId = 1;
}
