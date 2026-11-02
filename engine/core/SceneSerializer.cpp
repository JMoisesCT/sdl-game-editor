#include "core/SceneSerializer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

#include "core/Prefabs.h"

static const char* SCENE_FORMAT = "sdl_engine_scene";
static const int SCENE_VERSION = 1;
static const char* PREFAB_FORMAT = "sdl_engine_prefab";
static const int PREFAB_VERSION = 1;

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
    case FieldType::List: {
        // Lista de objetos, cada uno con TODOS los campos del elemento.
        Json items = Json::array();
        const std::size_t n = f.listSize(p);
        for (std::size_t i = 0; i < n; ++i) {
            const void* item = f.listElement(const_cast<void*>(p), i);
            Json fields = Json::object();
            for (const FieldInfo& ef : f.elementFields) fields[ef.name] = writeField(ef, ef.in(item));
            items.push_back(std::move(fields));
        }
        return items;
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
    case FieldType::List: {
        // La lista se arma de cero: cada elemento nace con sus valores por defecto y
        // se le escribe lo que traiga el archivo (como a un componente).
        const auto& items = j.get_ref<const Json::array_t&>(); // si no es lista, lanza
        f.listResize(p, 0);
        f.listResize(p, items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (!items[i].is_object()) continue;
            void* item = f.listElement(p, i);
            for (const FieldInfo& ef : f.elementFields) {
                auto v = items[i].find(ef.name);
                if (v == items[i].end()) continue;
                try {
                    readField(ef, ef.in(item), *v);
                } catch (const Json::exception&) {
                    SDL_Log("Escena: %s[%u].%s tiene un valor invalido (se ignora)",
                            f.name.c_str(), (unsigned)i, ef.name.c_str());
                }
            }
        }
        break;
    }
    }
}

// --- Entidades ---------------------------------------------------------------------

// Todos los campos de un componente.
static Json componentFields(const ComponentInfo& info, const void* c) {
    Json fields = Json::object();
    for (const FieldInfo& f : info.fields) fields[f.name] = writeField(f, f.in(c));
    return fields;
}

Json serializeEntity(const World& world, const ComponentRegistry& registry,
                     PrefabLibrary* prefabs, Entity e) {
    // Si es instancia, se compara contra los componentes (completos) de su prefab.
    const PrefabInstance* link = world.get<PrefabInstance>(e);
    const Json* base = link && prefabs ? prefabs->find(link->path) : nullptr;

    Json components = Json::object();
    for (const auto& info : registry.all()) {
        const void* c = info->get(world, e);
        if (!c) continue;
        Json fields = componentFields(*info, c);
        auto inBase = base ? base->find(info->name) : Json::const_iterator();
        if (base && inBase != base->end()) {
            // Solo lo que difiere del prefab. Si no difiere nada, ni se nombra.
            Json diff = Json::object();
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                auto bf = inBase->find(it.key());
                if (bf == inBase->end() || *bf != it.value()) diff[it.key()] = it.value();
            }
            if (!diff.empty()) components[info->name] = std::move(diff);
        } else {
            components[info->name] = std::move(fields);
        }
    }

    Json out = Json::object();
    out["id"] = e;
    if (link) {
        out["prefab"] = link->path;
        // Componentes del prefab que esta instancia ya no tiene.
        Json removed = Json::array();
        if (base)
            for (auto it = base->begin(); it != base->end(); ++it) {
                const ComponentInfo* info = registry.find(it.key());
                if (info && !info->has(world, e)) removed.push_back(it.key());
            }
        if (!removed.empty()) out["removed"] = std::move(removed);
    }
    out["components"] = std::move(components);
    return out;
}

void deserializeEntity(World& world, const ComponentRegistry& registry,
                       PrefabLibrary* prefabs, Entity e, const Json& entity) {
    if (!entity.is_object()) return;
    auto prefab = entity.find("prefab");
    if (prefab != entity.end() && prefab->is_string()) {
        std::string path = prefab->get<std::string>();
        world.add<PrefabInstance>(e).path = path;
        // Primero el prefab completo; si no se puede leer, la entidad queda solo con
        // sus overrides (y conserva el enlace, para cuando el prefab vuelva).
        if (const Json* base = prefabs ? prefabs->find(path) : nullptr)
            deserializeComponents(world, registry, e, *base);
        auto removed = entity.find("removed");
        if (removed != entity.end() && removed->is_array())
            for (const Json& name : *removed)
                if (const ComponentInfo* info = name.is_string() ? registry.find(name.get<std::string>()) : nullptr)
                    info->remove(world, e);
    }
    auto comps = entity.find("components");
    if (comps != entity.end()) deserializeComponents(world, registry, e, *comps);
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
        // Si ya lo tiene (viene del prefab), se escribe encima; si no, nace con sus
        // valores por defecto.
        void* c = info->get(world, e);
        if (!c) c = info->add(world, e);
        if (!it.value().is_object()) continue;
        for (const FieldInfo& f : info->fields) {
            auto fj = it.value().find(f.name);
            if (fj == it.value().end()) continue; // falta: queda el valor que tenia
            try {
                readField(f, f.in(c), *fj);
            } catch (const Json::exception&) {
                SDL_Log("Escena: %s.%s de la entidad %u tiene un valor invalido (se ignora)",
                        info->name.c_str(), f.name.c_str(), e);
            }
        }
    }
}

// --- World -------------------------------------------------------------------------

Json serializeWorld(const World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs) {
    Json entities = Json::array();
    for (Entity e : world.entities()) entities.push_back(serializeEntity(world, registry, prefabs, e));
    Json scene = Json::object();
    scene["format"] = SCENE_FORMAT;
    scene["version"] = SCENE_VERSION;
    scene["entities"] = std::move(entities);
    return scene;
}

bool deserializeWorld(World& world, const ComponentRegistry& registry, PrefabLibrary* prefabs,
                      const Json& scene, std::string& error) {
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
        deserializeEntity(world, registry, prefabs, id, ej);
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
// archivo y pintar una celda cambia una sola linea del diff. 'indent' es la sangria
// de la linea del componente: las filas van 2 espacios mas adentro.
static void writeMatrix(std::ostream& out, const Json& m, int indent) {
    std::size_t width = 1;
    for (const Json& row : m)
        for (const Json& v : row) width = std::max(width, v.dump().size());
    const std::string rowIndent(indent + 2, ' ');
    out << "[";
    for (std::size_t i = 0; i < m.size(); ++i) {
        out << (i == 0 ? "\n" : ",\n") << rowIndent << "[";
        const Json& row = m[i];
        for (std::size_t k = 0; k < row.size(); ++k) {
            std::string s = row[k].dump();
            if (k > 0) out << ",";
            out << std::string(width - s.size(), ' ') << s;
        }
        out << "]";
    }
    out << "\n" << std::string(indent, ' ') << "]";
}

// Una lista no vacia de objetos (los clips de un SpriteAnimator).
static bool isObjectList(const Json& j) {
    if (!j.is_array() || j.empty()) return false;
    for (const Json& item : j)
        if (!item.is_object()) return false;
    return true;
}

// Un elemento por linea, como las filas de una matriz: agregar o cambiar un clip
// cambia una sola linea del diff.
static void writeObjectList(std::ostream& out, const Json& list, int indent) {
    const std::string itemIndent(indent + 2, ' ');
    out << "[";
    for (std::size_t i = 0; i < list.size(); ++i)
        out << (i == 0 ? "\n" : ",\n") << itemIndent << list[i].dump();
    out << "\n" << std::string(indent, ' ') << "]";
}

// Un componente en su linea, igual que dump(): {"campo":valor,...}. Las excepciones
// son los campos matriz, que van una fila por linea (ver writeMatrix), y las listas
// de structs, un elemento por linea.
static void writeComponent(std::ostream& out, const Json& c, int indent) {
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
        if (isMatrix(it.value())) writeMatrix(out, it.value(), indent);
        else if (isObjectList(it.value())) writeObjectList(out, it.value(), indent);
        else out << it.value().dump();
    }
    out << "}";
}

// Los componentes de una entidad o de un prefab, uno por linea, con esa sangria. Deja
// el cursor despues de la ultima linea (sin salto); false si no habia ninguno.
static bool writeComponentLines(std::ostream& out, const Json& comps, int indent) {
    bool first = true;
    for (auto it = comps.begin(); it != comps.end(); ++it) {
        out << (first ? "\n" : ",\n");
        out << std::string(indent, ' ') << Json(it.key()).dump() << ": ";
        writeComponent(out, it.value(), indent);
        first = false;
    }
    return !first;
}

// Escribe con temporal + renombrar: un corte a mitad no deja el archivo roto.
static bool writeTextFile(const std::string& path, const std::string& text) {
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary);
        if (!file) {
            SDL_Log("No se pudo crear '%s'", tmpPath.c_str());
            return false;
        }
        file << text;
        if (!file) {
            SDL_Log("Fallo la escritura de '%s'", tmpPath.c_str());
            return false;
        }
    }
    if (!SDL_RenamePath(tmpPath.c_str(), path.c_str())) {
        SDL_Log("No se pudo reemplazar '%s': %s", path.c_str(), SDL_GetError());
        SDL_RemovePath(tmpPath.c_str());
        return false;
    }
    return true;
}

static bool readJsonFile(const std::string& path, Json& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "no se pudo abrir '" + path + "'";
        return false;
    }
    try {
        out = Json::parse(file);
    } catch (const Json::exception& ex) {
        error = std::string("JSON invalido: ") + ex.what();
        return false;
    }
    return true;
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
        out << "    { \"id\": " << ej.at("id").dump();
        if (ej.contains("prefab")) out << ", \"prefab\": " << ej["prefab"].dump();
        if (ej.contains("removed")) out << ", \"removed\": " << ej["removed"].dump();
        out << ", \"components\": {";
        bool any = writeComponentLines(out, ej.at("components"), 8);
        out << (any ? "\n    }}" : "}}");
    }
    out << (entities.empty() ? "]\n}\n" : "\n  ]\n}\n");
    return writeTextFile(path, out.str());
}

bool loadSceneFile(const std::string& path, Json& scene, std::string& error) {
    return readJsonFile(path, scene, error);
}

bool savePrefabFile(const std::string& path, const Json& components) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"format\": " << Json(PREFAB_FORMAT).dump() << ",\n";
    out << "  \"version\": " << PREFAB_VERSION << ",\n";
    out << "  \"components\": {";
    bool any = writeComponentLines(out, components, 4);
    out << (any ? "\n  }\n}\n" : "}\n}\n");
    return writeTextFile(path, out.str());
}

bool loadPrefabFile(const std::string& path, Json& components, std::string& error) {
    Json file;
    if (!readJsonFile(path, file, error)) return false;
    if (!file.is_object() || file.value("format", "") != PREFAB_FORMAT) {
        error = "no es un prefab (falta \"format\": \"sdl_engine_prefab\")";
        return false;
    }
    if (file.value("version", 0) > PREFAB_VERSION) {
        error = "el prefab es de una version mas nueva del motor";
        return false;
    }
    auto comps = file.find("components");
    if (comps == file.end() || !comps->is_object()) {
        error = "falta el objeto \"components\"";
        return false;
    }
    components = *comps;
    return true;
}
