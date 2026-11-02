#include "core/SceneSerializer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

static const char* SCENE_FORMAT = "sdl_engine_scene";
static const int SCENE_VERSION = 1;

// --- Un campo <-> JSON -------------------------------------------------------------

// Un float pasado a double se escribe con todos sus decimales binarios (0.1f sale
// 0.10000000149011612). Redondear a 5 decimales deja el archivo legible.
static double cleanFloat(float f) {
    return std::round(static_cast<double>(f) * 100000.0) / 100000.0;
}

static Json writeField(const FieldInfo& f, const void* p) {
    switch (f.type) {
    case FieldType::Bool:   return *static_cast<const bool*>(p);
    case FieldType::Int:    return *static_cast<const int*>(p);
    case FieldType::Float:  return cleanFloat(*static_cast<const float*>(p));
    case FieldType::String: return *static_cast<const std::string*>(p);
    case FieldType::Vec2: {
        const Vec2& v = *static_cast<const Vec2*>(p);
        return Json::array({ cleanFloat(v.x), cleanFloat(v.y) });
    }
    case FieldType::Color: {
        const Color& c = *static_cast<const Color*>(p);
        return Json::array({ c.r, c.g, c.b, c.a });
    }
    case FieldType::IntRect: {
        const IntRect& r = *static_cast<const IntRect*>(p);
        return Json::array({ r.x, r.y, r.w, r.h });
    }
    }
    return nullptr;
}

static std::uint8_t toByte(const Json& j) {
    return static_cast<std::uint8_t>(std::clamp(j.get<int>(), 0, 255));
}

// Lanza una excepcion de nlohmann si el valor no tiene el tipo esperado; quien llama
// la atrapa y deja el valor por defecto.
static void readField(const FieldInfo& f, void* p, const Json& j) {
    switch (f.type) {
    case FieldType::Bool:   *static_cast<bool*>(p) = j.get<bool>(); break;
    case FieldType::Int:    *static_cast<int*>(p) = j.get<int>(); break;
    case FieldType::Float:  *static_cast<float*>(p) = j.get<float>(); break;
    case FieldType::String: *static_cast<std::string*>(p) = j.get<std::string>(); break;
    case FieldType::Vec2: {
        Vec2 v{ j.at(0).get<float>(), j.at(1).get<float>() };
        *static_cast<Vec2*>(p) = v;
        break;
    }
    case FieldType::Color: {
        Color c{ toByte(j.at(0)), toByte(j.at(1)), toByte(j.at(2)), toByte(j.at(3)) };
        *static_cast<Color*>(p) = c;
        break;
    }
    case FieldType::IntRect: {
        IntRect r{ j.at(0).get<int>(), j.at(1).get<int>(), j.at(2).get<int>(), j.at(3).get<int>() };
        *static_cast<IntRect*>(p) = r;
        break;
    }
    }
}

// --- Entidades ---------------------------------------------------------------------

Json serializeEntity(const World& world, const ComponentRegistry& registry, Entity e) {
    Json components = Json::object();
    for (const auto& info : registry.all()) {
        const void* c = info->get(world, e);
        if (!c) continue;
        Json fields = Json::object();
        for (const FieldInfo& f : info->fields) fields[f.name] = writeField(f, f.in(c));
        components[info->name] = std::move(fields);
    }
    Json out = Json::object();
    out["id"] = e;
    out["components"] = std::move(components);
    return out;
}

void deserializeComponents(World& world, const ComponentRegistry& registry, Entity e,
                           const Json& components) {
    if (!components.is_object()) return;
    // Un aviso por nombre desconocido, no uno por entidad.
    static std::set<std::string> warned;
    for (auto it = components.begin(); it != components.end(); ++it) {
        const ComponentInfo* info = registry.find(it.key());
        if (!info) {
            if (warned.insert(it.key()).second)
                SDL_Log("Escena: componente desconocido '%s' (se ignora)", it.key().c_str());
            continue;
        }
        void* c = info->add(world, e);
        if (!it.value().is_object()) continue;
        for (const FieldInfo& f : info->fields) {
            auto fj = it.value().find(f.name);
            if (fj == it.value().end()) continue; // falta: queda el valor por defecto
            try {
                readField(f, f.in(c), *fj);
            } catch (const Json::exception&) {
                SDL_Log("Escena: %s.%s de la entidad %u tiene un valor invalido (se usa el de "
                        "por defecto)", info->name.c_str(), f.name.c_str(), e);
            }
        }
    }
}

// --- World -------------------------------------------------------------------------

Json serializeWorld(const World& world, const ComponentRegistry& registry) {
    Json entities = Json::array();
    for (Entity e : world.entities()) entities.push_back(serializeEntity(world, registry, e));
    Json scene = Json::object();
    scene["format"] = SCENE_FORMAT;
    scene["version"] = SCENE_VERSION;
    scene["entities"] = std::move(entities);
    return scene;
}

bool deserializeWorld(World& world, const ComponentRegistry& registry, const Json& scene,
                      std::string& error) {
    world.clear();
    if (!scene.is_object() || scene.value("format", "") != SCENE_FORMAT) {
        error = "no es un archivo de escena (falta \"format\": \"sdl_engine_scene\")";
        return false;
    }
    if (scene.value("version", 0) > SCENE_VERSION) {
        error = "la escena es de una version mas nueva del motor";
        return false;
    }
    auto list = scene.find("entities");
    if (list == scene.end() || !list->is_array()) {
        error = "falta la lista \"entities\"";
        return false;
    }
    for (const Json& ej : *list) {
        Entity id = NullEntity;
        try { id = ej.at("id").get<Entity>(); } catch (const Json::exception&) {}
        // Sin id valido (o repetido) se le da uno nuevo: mejor cargarla que perderla.
        if (!world.createWithId(id)) {
            Entity fresh = world.create();
            SDL_Log("Escena: entidad con id %u invalido o repetido; pasa a ser la %u", id, fresh);
            id = fresh;
        }
        auto comps = ej.find("components");
        if (comps != ej.end()) deserializeComponents(world, registry, id, *comps);
    }
    return true;
}

// --- Archivo -----------------------------------------------------------------------

bool saveSceneFile(const std::string& path, const Json& scene) {
    // A mano y no con dump(2): dump(2) pone cada numero de un [x, y] en su propia
    // linea. Asi queda una entidad por bloque y un componente por linea.
    std::ostringstream out;
    out << "{\n";
    out << "  \"format\": " << Json(scene.value("format", SCENE_FORMAT)).dump() << ",\n";
    out << "  \"version\": " << scene.value("version", SCENE_VERSION) << ",\n";
    out << "  \"entities\": [";
    const Json& entities = scene.at("entities");
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Json& ej = entities[i];
        out << (i == 0 ? "\n" : ",\n");
        out << "    { \"id\": " << ej.at("id").dump() << ", \"components\": {";
        const Json& comps = ej.at("components");
        bool first = true;
        for (auto it = comps.begin(); it != comps.end(); ++it) {
            out << (first ? "\n" : ",\n");
            out << "        " << Json(it.key()).dump() << ": " << it.value().dump();
            first = false;
        }
        out << (first ? "}}" : "\n    }}");
    }
    out << (entities.empty() ? "]\n}\n" : "\n  ]\n}\n");

    std::string tmpPath = path + ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary);
        if (!file) {
            SDL_Log("saveSceneFile: no se pudo crear '%s'", tmpPath.c_str());
            return false;
        }
        file << out.str();
        if (!file) {
            SDL_Log("saveSceneFile: fallo la escritura de '%s'", tmpPath.c_str());
            return false;
        }
    }
    if (!SDL_RenamePath(tmpPath.c_str(), path.c_str())) {
        SDL_Log("saveSceneFile: no se pudo reemplazar '%s': %s", path.c_str(), SDL_GetError());
        SDL_RemovePath(tmpPath.c_str());
        return false;
    }
    return true;
}

bool loadSceneFile(const std::string& path, Json& scene, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "no se pudo abrir '" + path + "'";
        return false;
    }
    try {
        scene = Json::parse(file);
    } catch (const Json::exception& ex) {
        error = std::string("JSON invalido: ") + ex.what();
        return false;
    }
    return true;
}
