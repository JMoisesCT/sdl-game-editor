#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Reflection.h"
#include "core/World.h"

class PrefabLibrary;

// Guardar y cargar el World en JSON usando la reflexion: ningun componente escribe
// su propio codigo de guardado. Se usa para tres cosas:
//   1. el archivo de escena (.scene.json),
//   2. la copia que se guarda al pulsar Play y se restaura al pulsar Stop,
//   3. los pasos de deshacer/rehacer del editor.
//
// Formato del archivo:
//   {
//     "format": "sdl_engine_scene",
//     "version": 1,
//     "entities": [
//       { "id": 1, "components": {
//           "EntityInfo": {"name":"Camara","tag":"MainCamera"},
//           "Transform": {"position":[640,360],"rotation":0,"scale":[1,1]}
//       }},
//       { "id": 2, "prefab": "assets/prefabs/fruit.prefab.json", "components": {
//           "Transform": {"position":[480,416]}
//       }},
//       ...
//     ]
//   }
//
// INSTANCIAS DE PREFAB (ver Prefabs.h): una entidad con "prefab" se arma primero con
// los componentes del prefab y despues se le aplica lo que dice "components" encima.
// Por eso de una instancia solo se escriben los campos que DIFIEREN del prefab (sus
// "overrides"): si el prefab cambia, todas sus instancias cambian con el, salvo en lo
// que cada una sobrescribio. Si a la instancia se le quito un componente del prefab,
// va en "removed": ["Hazard"].
//
// Cada componente va en una linea. Las excepciones: las grillas (las celdas de un
// Tilemap) se escriben una fila por linea, con los numeros alineados, y las listas de
// structs (los clips de un SpriteAnimator) un elemento por linea:
//       "Tilemap": {"tileset":"...", ... ,"cells":[
//           [ -1, -1, 94, 95],
//           [ -1, -1, -1, -1]
//         ]}
//       "SpriteAnimator": {"startClip":"idle","speed":1.0,"clips":[
//           {"name":"idle","texture":"...","fps":20.0, ...},
//           {"name":"run","texture":"...","fps":20.0, ...}
//         ]}
// Por eso esos campos se registran al final del componente.
//
// Al cargar, un campo que falta conserva su valor por defecto (o el del prefab):
// agregar un campo nuevo a un componente no rompe las escenas ya guardadas. Un
// componente que el registro no conoce se avisa en el log y se ignora.
//
// 'prefabs' puede ser nullptr: entonces las instancias se escriben completas (con su
// "prefab", pero todos sus campos) y al leer no se les agrega nada del prefab.

using Json = nlohmann::ordered_json; // conserva el orden de las claves al escribir

// Una entidad: { "id": N, ["prefab": ruta,] ["removed": [...],] "components": {...} }.
Json serializeEntity(const World& world, const ComponentRegistry& registry,
                     PrefabLibrary* prefabs, Entity e);
// Arma una entidad que YA existe con lo que dice ese JSON: el prefab (si tiene), los
// componentes quitados y encima "components". No mira el "id".
void deserializeEntity(World& world, const ComponentRegistry& registry,
                       PrefabLibrary* prefabs, Entity e, const Json& entity);
// Aplica una seccion "components" a una entidad que YA existe. Los componentes que ya
// tiene se conservan: solo cambian los campos que aparecen en el JSON.
void deserializeComponents(World& world, const ComponentRegistry& registry, Entity e,
                           const Json& components);

// El World completo (formato del archivo, ver arriba).
Json serializeWorld(const World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs);
// Vacia el World y lo llena desde el JSON. false (con el motivo en error) si el JSON
// no tiene la forma esperada; en ese caso el World queda vacio.
bool deserializeWorld(World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs,
                      const Json& scene, std::string& error);

// Archivo <-> JSON. saveSceneFile escribe un componente por linea y las grillas una
// fila por linea (diffs legibles en git), y usa temporal + renombrar: un corte a
// mitad no deja la escena rota.
bool saveSceneFile(const std::string& path, const Json& scene);
bool loadSceneFile(const std::string& path, Json& scene, std::string& error);

// Archivo de prefab <-> su seccion "components" (mismo estilo que la escena):
//   { "format": "sdl_engine_prefab", "version": 1, "components": { ... } }
bool savePrefabFile(const std::string& path, const Json& components);
bool loadPrefabFile(const std::string& path, Json& components, std::string& error);
