#include "core/Prefabs.h"

#include <SDL3/SDL.h>

#include "core/Reflection.h"
#include "core/World.h"

Json PrefabLibrary::complete(const Json& components) const {
    // Se carga en un World de paso y se vuelve a escribir: lo que falta toma su valor
    // por defecto y lo que sobra (componentes o campos que ya no existen) se cae.
    World scratch;
    Entity e = scratch.create();
    deserializeComponents(scratch, registry, e, components);
    return serializeEntity(scratch, registry, nullptr, e)["components"];
}

const Json* PrefabLibrary::find(const std::string& path) {
    auto it = cache.find(path);
    if (it == cache.end()) {
        Json components;
        std::string error;
        if (loadPrefabFile(path, components, error)) {
            components = complete(components);
        } else {
            SDL_Log("Prefab '%s': %s", path.c_str(), error.c_str());
            components = Json(); // se recuerda el fallo: no se reintenta en cada frame
        }
        it = cache.emplace(path, std::move(components)).first;
    }
    return it->second.is_null() ? nullptr : &it->second;
}

bool PrefabLibrary::save(const std::string& path, const Json& components) {
    std::string::size_type slash = path.find_last_of("/\\");
    if (slash != std::string::npos) SDL_CreateDirectory(path.substr(0, slash).c_str());
    Json full = complete(components);
    if (!savePrefabFile(path, full)) return false;
    cache[path] = std::move(full);
    return true;
}

Entity PrefabLibrary::instantiate(World& world, const std::string& path) {
    const Json* components = find(path);
    if (!components) return NullEntity;
    Entity e = world.create();
    world.add<PrefabInstance>(e).path = path;
    deserializeComponents(world, registry, e, *components);
    return e;
}
