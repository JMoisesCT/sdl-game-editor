#pragma once
#include <map>
#include <string>

#include "core/Entity.h"
#include "core/SceneSerializer.h"

class ComponentRegistry;
class World;

// PREFABS: a "template" entity saved in its own file (.prefab.json), with its
// components. They replace the setupX functions and the object catalog of v1:
// instead of a C++ function that builds the fruit, there is a file with the
// fruit's components, edited in the editor.
//
// An INSTANCE is a scene entity made from a prefab. It stays LINKED to it: the
// scene only stores the fields where it differs (its "overrides", such as the
// position or the texture of a DIFFERENT fruit), and everything else comes from the
// prefab on load. That way, changing the prefab (the spikes' box, the player's
// health...) changes all its instances at once. See SceneSerializer.h.
//
// In the editor: the Prefabs panel places instances (click or drag into the view),
// and the Inspector of an instance offers Apply (its changes go into the prefab),
// Revert (it matches the prefab again, except position and name) and Unlink.
//
// From code (e.g. a system that fires bullets):
//     Entity bullet = prefabs.instantiate(world, "assets/prefabs/bullet.prefab.json");
//     world.get<Transform>(bullet)->position = muzzle;

// Marks an entity as an instance of a prefab. Not registered in the reflection: the
// serializer writes it separately ("prefab": path) and the Inspector shows it on top.
struct PrefabInstance {
    std::string path; // path of the .prefab.json, relative to the executable's folder
};

// The loaded prefabs, by path. Each one is read once (until clear()) and stored in
// COMPLETE form: every field of every component, with the defaults already filled in.
// That way, comparing an instance with its prefab is comparing JSON field by field.
class PrefabLibrary {
public:
    explicit PrefabLibrary(const ComponentRegistry& registry) : registry(registry) {}

    // The prefab's components (a JSON object "Name": {fields}), or nullptr if the file
    // does not exist or is not a prefab (logged once).
    const Json* find(const std::string& path);

    // Saves those components as the prefab at that path (creating the folder if
    // needed) and updates what was loaded. false if it could not be written.
    bool save(const std::string& path, const Json& components);

    // Forgets what was loaded: the next find reads the disk again.
    void clear() { cache.clear(); }

    // Creates a new entity linked to the prefab, with all its components. NullEntity
    // if the prefab cannot be read. Like any creation, do not call it inside an each
    // that iterates components the prefab has.
    Entity instantiate(World& world, const std::string& path);

private:
    Json complete(const Json& components) const; // with every field

    const ComponentRegistry& registry;
    std::map<std::string, Json> cache; // path -> components; null = could not be read
};
