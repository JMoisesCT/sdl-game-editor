#include "core/SceneSerializer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

#include "core/Hierarchy.h"
#include "core/Prefabs.h"

static const char* SCENE_FORMAT = "sdl_engine_scene";
static const int SCENE_VERSION = 1;
static const char* PREFAB_FORMAT = "sdl_engine_prefab";
static const int PREFAB_VERSION = 1;

// --- One field <-> JSON ------------------------------------------------------------

// A float converted to double is written with all its binary decimals (0.1f becomes
// 0.10000000149011612). Rounding to 5 decimals keeps the file readable.
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
    case FieldType::Entity:  return *static_cast<const Entity*>(p); // the id; 0 = none
    case FieldType::IntGrid: {
        // List of rows: [[row 0], [row 1], ...]. The width is the length of a row.
        const IntGrid& g = *static_cast<const IntGrid*>(p);
        Json rows = Json::array();
        for (int y = 0; y < g.height; ++y) {
            auto begin = g.cells.begin() + (std::ptrdiff_t)y * g.width;
            rows.push_back(Json(std::vector<int>(begin, begin + g.width)));
        }
        return rows;
    }
    case FieldType::List: {
        // List of objects, each one with ALL the element's fields.
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

// Throws a nlohmann exception if the value does not have the expected type; the
// caller catches it and keeps the default value.
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
    case FieldType::Entity:  *static_cast<Entity*>(p) = j.get<Entity>(); break;
    case FieldType::IntGrid: {
        // The width comes from the longest row; if a row was short (edited by
        // hand), what it lacks is left as -1 (empty).
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
        // The list is built from scratch: each element starts with its default values
        // and gets whatever the file has written onto it (like a component).
        const auto& items = j.get_ref<const Json::array_t&>(); // throws if it is not a list
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
                    SDL_Log("Scene: %s[%u].%s has an invalid value (ignored)",
                            f.name.c_str(), (unsigned)i, ef.name.c_str());
                }
            }
        }
        break;
    }
    }
}

// --- Entities ----------------------------------------------------------------------

// Every field of a component.
static Json componentFields(const ComponentInfo& info, const void* c) {
    Json fields = Json::object();
    for (const FieldInfo& f : info.fields) fields[f.name] = writeField(f, f.in(c));
    return fields;
}

Json serializeEntity(const World& world, const ComponentRegistry& registry,
                     PrefabLibrary* prefabs, Entity e) {
    // If it is an instance, it is compared against its prefab's (complete) components.
    const PrefabInstance* link = world.get<PrefabInstance>(e);
    const Json* base = link && prefabs ? prefabs->find(link->path) : nullptr;

    Json components = Json::object();
    for (const auto& info : registry.all()) {
        const void* c = info->get(world, e);
        if (!c) continue;
        Json fields = componentFields(*info, c);
        auto inBase = base ? base->find(info->name) : Json::const_iterator();
        if (base && inBase != base->end()) {
            // Only what differs from the prefab. If nothing differs, it is not even named.
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
    // The hierarchy (see Hierarchy.h): a parent that no longer exists is not written.
    if (Entity parent = parentOf(world, e)) out["parent"] = parent;
    if (link) {
        out["prefab"] = link->path;
        // Prefab components this instance no longer has.
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
    // The parent may not exist yet (it comes further down in the file): it is stored
    // as is, and fixHierarchy checks it once the scene has finished loading.
    auto parent = entity.find("parent");
    if (parent != entity.end() && parent->is_number_unsigned() && parent->get<Entity>() != e)
        world.add<Parent>(e).entity = parent->get<Entity>();
    auto prefab = entity.find("prefab");
    if (prefab != entity.end() && prefab->is_string()) {
        std::string path = prefab->get<std::string>();
        world.add<PrefabInstance>(e).path = path;
        // First the complete prefab; if it cannot be read, the entity keeps only its
        // overrides (and keeps the link, for when the prefab comes back).
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
    // One warning per unknown name, not one per entity.
    static std::set<std::string> warned;
    for (auto it = components.begin(); it != components.end(); ++it) {
        const ComponentInfo* info = registry.find(it.key());
        if (!info) {
            if (warned.insert(it.key()).second)
                SDL_Log("Scene: unknown component '%s' (ignored)", it.key().c_str());
            continue;
        }
        // If it already has it (it comes from the prefab), it is written on top; if not,
        // it starts with its default values.
        void* c = info->get(world, e);
        if (!c) c = info->add(world, e);
        if (!it.value().is_object()) continue;
        for (const FieldInfo& f : info->fields) {
            auto fj = it.value().find(f.name);
            if (fj == it.value().end()) continue; // missing: the value it had is kept
            try {
                readField(f, f.in(c), *fj);
            } catch (const Json::exception&) {
                SDL_Log("Scene: %s.%s of entity %u has an invalid value (ignored)",
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
        error = "not a scene file (missing \"format\": \"sdl_engine_scene\")";
        return false;
    }
    if (scene.value("version", 0) > SCENE_VERSION) {
        error = "the scene comes from a newer version of the engine";
        return false;
    }
    auto list = scene.find("entities");
    if (list == scene.end() || !list->is_array()) {
        error = "missing the \"entities\" list";
        return false;
    }
    for (const Json& ej : *list) {
        Entity id = NullEntity;
        try { id = ej.at("id").get<Entity>(); } catch (const Json::exception&) {}
        // Without a valid (or with a repeated) id, it gets a new one: better to load it than lose it.
        if (!world.createWithId(id)) {
            Entity fresh = world.create();
            SDL_Log("Scene: entity with an invalid or repeated id %u; it becomes %u", id, fresh);
            id = fresh;
        }
        deserializeEntity(world, registry, prefabs, id, ej);
    }
    fixHierarchy(world); // parents that do not exist, or cycles (file edited by hand)
    return true;
}

// --- File --------------------------------------------------------------------------

// A matrix is a non-empty list of lists of numbers (the cells of a Tilemap).
static bool isMatrix(const Json& j) {
    if (!j.is_array() || j.empty()) return false;
    for (const Json& row : j) {
        if (!row.is_array()) return false;
        for (const Json& v : row)
            if (!v.is_number()) return false;
    }
    return true;
}

// One row per line and the numbers right-aligned: that way the map is "visible" in
// the file and painting a cell changes a single line of the diff. 'indent' is the
// indentation of the component's line: the rows go 2 spaces further in.
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

// A non-empty list of objects (the clips of a SpriteAnimator).
static bool isObjectList(const Json& j) {
    if (!j.is_array() || j.empty()) return false;
    for (const Json& item : j)
        if (!item.is_object()) return false;
    return true;
}

// One element per line, like the rows of a matrix: adding or changing a clip
// changes a single line of the diff.
static void writeObjectList(std::ostream& out, const Json& list, int indent) {
    const std::string itemIndent(indent + 2, ' ');
    out << "[";
    for (std::size_t i = 0; i < list.size(); ++i)
        out << (i == 0 ? "\n" : ",\n") << itemIndent << list[i].dump();
    out << "\n" << std::string(indent, ' ') << "]";
}

// A component on its own line, just like dump(): {"field":value,...}. The exceptions
// are matrix fields, one row per line (see writeMatrix), and lists of structs, one
// element per line.
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

// The components of an entity or a prefab, one per line, with that indentation. Leaves
// the cursor after the last line (no newline); false if there were none.
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

// Writes with a temp file + rename: an interruption halfway does not leave a broken file.
static bool writeTextFile(const std::string& path, const std::string& text) {
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream file(tmpPath, std::ios::binary);
        if (!file) {
            SDL_Log("Could not create '%s'", tmpPath.c_str());
            return false;
        }
        file << text;
        if (!file) {
            SDL_Log("Failed to write '%s'", tmpPath.c_str());
            return false;
        }
    }
    if (!SDL_RenamePath(tmpPath.c_str(), path.c_str())) {
        SDL_Log("Could not replace '%s': %s", path.c_str(), SDL_GetError());
        SDL_RemovePath(tmpPath.c_str());
        return false;
    }
    return true;
}

static bool readJsonFile(const std::string& path, Json& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "could not open '" + path + "'";
        return false;
    }
    try {
        out = Json::parse(file);
    } catch (const Json::exception& ex) {
        error = std::string("invalid JSON: ") + ex.what();
        return false;
    }
    return true;
}

bool saveSceneFile(const std::string& path, const Json& scene) {
    // By hand rather than with dump(2): dump(2) puts each number of an [x, y] on its own
    // line. This way there is one entity per block and one component per line (except
    // matrices, one row per line).
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
        if (ej.contains("parent")) out << ", \"parent\": " << ej["parent"].dump();
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
        error = "not a prefab (missing \"format\": \"sdl_engine_prefab\")";
        return false;
    }
    if (file.value("version", 0) > PREFAB_VERSION) {
        error = "the prefab comes from a newer version of the engine";
        return false;
    }
    auto comps = file.find("components");
    if (comps == file.end() || !comps->is_object()) {
        error = "missing the \"components\" object";
        return false;
    }
    components = *comps;
    return true;
}
