#pragma once
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Reflection.h"
#include "core/World.h"

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
//           "EntityInfo": {"name":"Jugador","tag":"Player"},
//           "Transform": {"position":[100,200],"rotation":0,"scale":[2,2]}
//       }},
//       ...
//     ]
//   }
//
// Al cargar, un campo que falta conserva su valor por defecto: agregar un campo nuevo
// a un componente no rompe las escenas ya guardadas. Un componente que el registro no
// conoce se avisa en el log y se ignora.

using Json = nlohmann::ordered_json; // conserva el orden de las claves al escribir

// Una entidad: { "id": N, "components": { ... } }.
Json serializeEntity(const World& world, const ComponentRegistry& registry, Entity e);
// Lee la seccion "components" y se la agrega a una entidad que YA existe.
void deserializeComponents(World& world, const ComponentRegistry& registry, Entity e,
                           const Json& components);

// El World completo (formato del archivo, ver arriba).
Json serializeWorld(const World& world, const ComponentRegistry& registry);
// Vacia el World y lo llena desde el JSON. false (con el motivo en error) si el JSON
// no tiene la forma esperada; en ese caso el World queda vacio.
bool deserializeWorld(World& world, const ComponentRegistry& registry, const Json& scene,
                      std::string& error);

// Archivo <-> JSON. saveSceneFile escribe un componente por linea (diffs legibles en
// git) y usa temporal + renombrar: un corte a mitad no deja la escena rota.
bool saveSceneFile(const std::string& path, const Json& scene);
bool loadSceneFile(const std::string& path, Json& scene, std::string& error);
