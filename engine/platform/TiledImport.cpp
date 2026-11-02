#include "platform/TiledImport.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "core/World.h"

using json = nlohmann::json;

namespace {
    // Una capa de tiles del JSON con lo que hereda de sus grupos.
    struct TileLayerRef {
        const json* layer;
        bool visible;
        float opacity;
    };

    // Junta las tilelayer en orden de dibujo. El array "layers" de Tiled ya viene de
    // abajo hacia arriba (la primera es la del fondo). Los grupos solo organizan: se
    // aplanan, y su visibilidad y opacidad pasan a sus capas.
    void collectTileLayers(const json& list, bool visible, float opacity, std::vector<TileLayerRef>& out) {
        for (const json& l : list) {
            std::string type = l.value("type", std::string());
            bool v = visible && l.value("visible", true);
            float o = opacity * l.value("opacity", 1.0f);
            if (type == "tilelayer")
                out.push_back({ &l, v, o });
            else if (type == "group" && l.contains("layers") && l["layers"].is_array())
                collectTileLayers(l["layers"], v, o, out);
            else if (type == "objectgroup")
                SDL_Log("Tiled: la capa de objetos '%s' se ignora", l.value("name", std::string()).c_str());
        }
    }

    // Propiedad booleana personalizada ("properties": [{name, type, value}]).
    bool boolProperty(const json& owner, const char* name, bool def) {
        auto props = owner.find("properties");
        if (props == owner.end() || !props->is_array()) return def;
        for (const json& p : *props) {
            if (p.value("name", std::string()) == name && p.contains("value") && p["value"].is_boolean())
                return p["value"].get<bool>();
        }
        return def;
    }
}

bool loadTiledMap(const std::string& path, std::vector<ImportedLayer>& layers, std::string& error) {
    layers.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "no se pudo abrir '" + path + "'";
        return false;
    }
    json j;
    try {
        j = json::parse(file);
    } catch (const json::exception& ex) {
        error = std::string("JSON invalido: ") + ex.what();
        return false;
    }

    try {
        if (j.value("type", std::string()) != "map") {
            error = "no es un mapa de Tiled";
            return false;
        }
        if (j.value("orientation", std::string("orthogonal")) != "orthogonal") {
            error = "solo se importan mapas ortogonales";
            return false;
        }
        if (j.value("infinite", false)) {
            error = "el mapa es infinito (en Tiled: Mapa > Propiedades > Infinito, desmarcar)";
            return false;
        }
        const int width = j.at("width").get<int>();
        const int height = j.at("height").get<int>();

        // El tileset: uno solo y embebido.
        const json& tilesets = j.at("tilesets");
        if (!tilesets.is_array() || tilesets.empty()) {
            error = "el mapa no tiene tileset";
            return false;
        }
        if (tilesets.size() > 1)
            SDL_Log("Tiled: '%s' tiene %d tilesets; solo se usa el primero", path.c_str(), (int)tilesets.size());
        const json& ts = tilesets[0];
        if (ts.contains("source")) {
            error = "el tileset es externo (.tsx); en Tiled, incrustarlo en el mapa";
            return false;
        }
        if (ts.value("margin", 0) != 0 || ts.value("spacing", 0) != 0) {
            error = "el tileset tiene margen o separacion entre tiles (no soportado)";
            return false;
        }
        const int firstgid = ts.value("firstgid", 1);
        const int tileCount = ts.value("tilecount", 0);

        Tilemap base;
        base.tileWidth = ts.at("tilewidth").get<int>();
        base.tileHeight = ts.at("tileheight").get<int>();
        // La imagen viene relativa al mapa; el AssetManager la quiere relativa a la
        // carpeta del ejecutable: "assets/maps/../x.png" -> "assets/x.png".
        namespace fs = std::filesystem;
        fs::path image = fs::u8path(ts.at("image").get<std::string>());
        base.tileset = (fs::u8path(path).parent_path() / image).lexically_normal().generic_u8string();

        // Tiles solidos: propiedad "solid" en los tiles del tileset.
        if (ts.contains("tiles") && ts["tiles"].is_array())
            for (const json& t : ts["tiles"])
                if (t.contains("id") && boolProperty(t, "solid", false))
                    base.solidTiles.push_back(t["id"].get<int>());

        std::vector<TileLayerRef> refs;
        collectTileLayers(j.at("layers"), true, 1.0f, refs);
        if (refs.empty()) {
            error = "el mapa no tiene capas de tiles";
            return false;
        }

        const unsigned FLIP_BITS = 0xE0000000u; // los 3 bits altos: volteo y rotacion
        bool flipped = false, outOfRange = false;
        std::vector<ImportedLayer> result;
        for (const TileLayerRef& ref : refs) {
            const json& l = *ref.layer;
            ImportedLayer out;
            out.name = l.value("name", std::string("Capa"));
            const json& data = l.at("data");
            if (!data.is_array()) {
                error = "la capa '" + out.name + "' no esta en CSV (en Tiled: Mapa > Propiedades > " +
                        "Formato de capa de patrones: CSV)";
                return false;
            }
            if ((int)data.size() != width * height) {
                error = "la capa '" + out.name + "' no tiene " + std::to_string(width * height) + " celdas";
                return false;
            }
            out.tilemap = base;
            out.tilemap.visible = ref.visible;
            out.tilemap.collision = boolProperty(l, "collision", true);
            float alpha = std::max(0.0f, std::min(1.0f, ref.opacity));
            out.tilemap.tint.a = (std::uint8_t)std::lround(alpha * 255.0f);
            out.tilemap.cells = IntGrid(width, height, -1);
            for (int i = 0; i < width * height; ++i) {
                unsigned gid = data[i].get<unsigned>();
                if (gid & FLIP_BITS) flipped = true;
                gid &= ~FLIP_BITS;
                if (gid == 0) continue; // vacia
                int tile = (int)gid - firstgid;
                if (tile < 0 || (tileCount > 0 && tile >= tileCount)) {
                    outOfRange = true; // de otro tileset: queda vacia
                    continue;
                }
                out.tilemap.cells.set(i % width, i / width, tile);
            }
            result.push_back(std::move(out));
        }
        if (flipped) SDL_Log("Tiled: '%s' usa tiles volteados o rotados; se cargan sin voltear", path.c_str());
        if (outOfRange) SDL_Log("Tiled: '%s' usa tiles de otro tileset; esas celdas quedan vacias", path.c_str());
        layers = std::move(result);
    } catch (const json::exception& ex) {
        error = std::string("falta un dato o tiene otro tipo: ") + ex.what();
        return false;
    }
    return true;
}

std::vector<Entity> addImportedLayers(World& world, const std::vector<ImportedLayer>& layers,
                                      Vec2 origin, float scale) {
    std::vector<Entity> created;
    const int n = (int)layers.size();
    for (int i = 0; i < n; ++i) {
        Entity e = world.create();
        world.add<EntityInfo>(e).name = layers[i].name;
        Transform t;
        t.position = origin;
        t.scale = { scale, scale };
        world.add<Transform>(e, t);
        Tilemap& map = world.add<Tilemap>(e, layers[i].tilemap);
        map.order = i - n - 9; // la de delante en -10, como el order por defecto
        created.push_back(e);
    }
    return created;
}
