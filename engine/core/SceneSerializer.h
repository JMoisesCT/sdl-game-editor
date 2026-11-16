#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Reflection.h"
#include "core/World.h"

class PrefabLibrary;

// Saves and loads the World as JSON using the reflection: no component writes its
// own saving code. It is used for three things:
//   1. the scene file (.scene.json),
//   2. the copy saved when pressing Play and restored when pressing Stop,
//   3. the editor's undo/redo steps.
//
// File format:
//   {
//     "format": "sdl_engine_scene",
//     "version": 1,
//     "entities": [
//       { "id": 1, "components": {
//           "EntityInfo": {"name":"Camera","tag":"MainCamera"},
//           "Transform": {"position":[640,360],"rotation":0,"scale":[1,1]}
//       }},
//       { "id": 2, "prefab": "assets/prefabs/fruit.prefab.json", "components": {
//           "Transform": {"position":[480,416]}
//       }},
//       { "id": 3, "parent": 1, "components": { ... }},
//       ...
//     ]
//   }
//
// HIERARCHY (see Hierarchy.h): a child entity has "parent" with its parent's id.
// The order of the list is the order of the editor's Hierarchy.
//
// PREFAB INSTANCES (see Prefabs.h): an entity with "prefab" is first built with the
// prefab's components, and then what "components" says is applied on top.
// That is why only the fields that DIFFER from the prefab are written for an instance
// (its "overrides"): if the prefab changes, all its instances change with it, except
// for what each one overrode. If a prefab component was removed from the instance,
// it goes in "removed": ["Hazard"].
//
// Each component goes on one line. The exceptions: grids (the cells of a Tilemap)
// are written one row per line, with aligned numbers, and lists of structs (the
// clips of a SpriteAnimator) one element per line:
//       "Tilemap": {"tileset":"...", ... ,"cells":[
//           [ -1, -1, 94, 95],
//           [ -1, -1, -1, -1]
//         ]}
//       "SpriteAnimator": {"startClip":"idle","speed":1.0,"clips":[
//           {"name":"idle","texture":"...","fps":20.0, ...},
//           {"name":"run","texture":"...","fps":20.0, ...}
//         ]}
// That is why those fields are registered last in the component.
//
// On load, a missing field keeps its default value (or the prefab's): adding a new
// field to a component does not break scenes already saved. A component the
// registry does not know is logged and ignored.
//
// 'prefabs' may be nullptr: then instances are written in full (with their
// "prefab", but all their fields) and nothing from the prefab is added on read.

using Json = nlohmann::ordered_json; // keeps the order of the keys when writing

// One entity: { "id": N, ["parent": P,] ["prefab": path,] ["removed": [...],]
// "components": {...} }.
Json serializeEntity(const World& world, const ComponentRegistry& registry,
                     PrefabLibrary* prefabs, Entity e);
// Builds an entity that ALREADY exists from that JSON: the parent, the prefab (if
// any), the removed components and, on top, "components". Ignores the "id". Without
// "parent", any parent it already had is kept.
void deserializeEntity(World& world, const ComponentRegistry& registry,
                       PrefabLibrary* prefabs, Entity e, const Json& entity);
// Applies a "components" section to an entity that ALREADY exists. Components it
// already has are kept: only the fields present in the JSON change.
void deserializeComponents(World& world, const ComponentRegistry& registry, Entity e,
                           const Json& components);

// The whole World (file format, see above).
Json serializeWorld(const World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs);
// Clears the World and fills it from the JSON. false (with the reason in error) if
// the JSON does not have the expected shape; in that case the World is left empty.
bool deserializeWorld(World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs,
                      const Json& scene, std::string& error);

// File <-> JSON. saveSceneFile writes one component per line and grids one row per
// line (readable diffs in git), and uses a temp file + rename: an interruption
// halfway does not leave a broken scene.
bool saveSceneFile(const std::string& path, const Json& scene);
bool loadSceneFile(const std::string& path, Json& scene, std::string& error);

// Prefab file <-> its "components" section (same style as the scene):
//   { "format": "sdl_engine_prefab", "version": 1, "components": { ... } }
bool savePrefabFile(const std::string& path, const Json& components);
bool loadPrefabFile(const std::string& path, Json& components, std::string& error);
