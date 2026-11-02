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
    case FieldType::IntList: return *static_cast<const std::vector<int>*>(p);
    case FieldType::IntGrid: {
        // Lista de filas: [[fila 0], [fila 1], ...]. El ancho es el largo de una fila.
        const IntGrid& g = *static_cast<const IntGrid*>(p);
        Json rows = Json::array();
        for (int y = 0; y < g.height; ++y) {
            auto begin = g.cells.begin() + (std::ptrdiff_t)y * g.width;
            rows.push_back(Json(std::vector<int>(begin, begin + g.width)));
        }
        return rows;
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
    case FieldType::IntList: *static_cast<std::vector<int>*>(p) = j.get<std::vector<int>>(); break;
    case FieldType::IntGrid: {
        // El ancho lo da la fila mas larga; si alguna fila quedo corta (editada a
        // mano), lo que le falta queda en -1 (vacio).
        auto rows = j.get<std::vector<std::vector<int>>>();
        int w = 0;
        for (const auto& row : rows) w = std::max(w, (int)row.size());
        IntGrid g(w, (int)rows.size(), -1);
        for (int y = 0; y < g.height; ++y)
            for (int x = 0; x < (int)rows[y].size(); ++x) g.set(x, y, rows[y][x]);
        *static_cast<IntGrid*>(p) = std::move(g);
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

// Una matriz es una lista no vacia de listas de numeros (las celdas de un Tilemap).
static bool isMatrix(const Json& j) {
    if (!j.is_array() || j.empty()) return false;
    for (const Json& row : j) {
        if (!row.is_array()) return false;
        for (const Json& v : row)
            if (!v.is_number()) return false;
    }
    return true;
}

// Una fila por linea y los numeros alineados a la derecha: asi el mapa se "ve" en el
// archivo y pintar una celda cambia una sola linea del diff.
static void writeMatrix(std::ostream& out, const Json& m) {
    std::size_t width = 1;
    for (const Json& row : m)
        for (const Json& v : row) width = std::max(width, v.dump().size());
    out << "[";
    for (std::size_t i = 0; i < m.size(); ++i) {
        out << (i == 0 ? "\n" : ",\n") << "          [";
        const Json& row = m[i];
        for (std::size_t k = 0; k < row.size(); ++k) {
            std::string s = row[k].dump();
            if (k > 0) out << ",";
            out << std::string(width - s.size(), ' ') << s;
        }
        out << "]";
    }
    out << "\n        ]";
}

// Un componente en su linea, igual que dump(): {"campo":valor,...}. La excepcion son
// los campos matriz, que van una fila por linea (ver writeMatrix).
static void writeComponent(std::ostream& out, const Json& c) {
    if (!c.is_object()) {
        out << c.dump();
        return;
    }
    out << "{";
    bool first = true;
    for (auto it = c.begin(); it != c.end(); ++it) {
        if (!first) out << ",";
        first = false;
        out << Json(it.key()).dump() << ":";
        if (isMatrix(it.value())) writeMatrix(out, it.value());
        else out << it.value().dump();
    }
    out << "}";
}

bool saveSceneFile(const std::string& path, const Json& scene) {
    // A mano y no con dump(2): dump(2) pone cada numero de un [x, y] en su propia
    // linea. Asi queda una entidad por bloque y un componente por linea (salvo las
    // matrices, una fila por linea).
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
            out << "        " << Json(it.key()).dump() << ": ";
            writeComponent(out, it.value());
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
