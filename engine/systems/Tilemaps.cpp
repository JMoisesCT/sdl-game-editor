#include "systems/Tilemaps.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

#include "components/CoreComponents.h"
#include "core/World.h"
#include "platform/AssetManager.h"

Vec2 tilemapCellSize(const Tilemap& map, const Transform& t) {
    return { map.tileWidth * std::fabs(t.scale.x), map.tileHeight * std::fabs(t.scale.y) };
}

void tilemapBounds(const Tilemap& map, const Transform& t, Vec2& min, Vec2& max) {
    Vec2 cell = tilemapCellSize(map, t);
    min = t.position;
    max = { t.position.x + map.cells.width * cell.x, t.position.y + map.cells.height * cell.y };
}

bool tilemapWorldToCell(const Tilemap& map, const Transform& t, Vec2 world, int& col, int& row) {
    Vec2 cell = tilemapCellSize(map, t);
    if (cell.x <= 0.0f || cell.y <= 0.0f) {
        col = row = -1;
        return false;
    }
    // floor y no truncar: a la izquierda (o arriba) del origen las celdas son negativas.
    col = (int)std::floor((world.x - t.position.x) / cell.x);
    row = (int)std::floor((world.y - t.position.y) / cell.y);
    return map.cells.inside(col, row);
}

Vec2 tilemapCellOrigin(const Tilemap& map, const Transform& t, int col, int row) {
    Vec2 cell = tilemapCellSize(map, t);
    return { t.position.x + col * cell.x, t.position.y + row * cell.y };
}

bool isSolidTile(const Tilemap& map, int tile) {
    return tile >= 0 && std::find(map.solidTiles.begin(), map.solidTiles.end(), tile) != map.solidTiles.end();
}

bool isSolidCell(const Tilemap& map, int col, int row) {
    return map.collision && isSolidTile(map, map.cells.get(col, row));
}

bool isSolidAt(World& world, Vec2 point) {
    bool solid = false;
    world.each<Tilemap, Transform>([&](Entity, Tilemap& map, Transform& t) {
        int col, row;
        if (!solid && tilemapWorldToCell(map, t, point, col, row)) solid = isSolidCell(map, col, row);
    });
    return solid;
}

bool tilesetGrid(AssetManager& assets, const Tilemap& map, int& columns, int& rows) {
    columns = rows = 0;
    if (map.tileset.empty() || map.tileWidth <= 0 || map.tileHeight <= 0) return false;
    SDL_Texture* tex = assets.loadTexture(map.tileset);
    if (!tex) return false;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    columns = (int)tw / map.tileWidth;
    rows = (int)th / map.tileHeight;
    return columns > 0 && rows > 0;
}
