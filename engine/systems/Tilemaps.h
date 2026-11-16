#pragma once
#include "core/Math.h"

class World;
class AssetManager;
struct Tilemap;
struct Transform;

// Queries on a Tilemap: converting from the world to cells and back, and knowing which
// cells are solid. Used by drawing, the editor (brush, selection) and physics.
// The map's origin is the Transform: the top left corner of cell (0, 0).

// Size of ONE cell in the world (the image's tile times the Transform's scale).
Vec2 tilemapCellSize(const Tilemap& map, const Transform& t);

// Rectangle the whole map covers in the world (min = top left corner).
void tilemapBounds(const Tilemap& map, const Transform& t, Vec2& min, Vec2& max);

// Cell under a world point. col and row are always computed, even if they fall
// OUTSIDE the map (negative or >= size); in that case it returns false.
bool tilemapWorldToCell(const Tilemap& map, const Transform& t, Vec2 world, int& col, int& row);

// Top left corner of a cell in the world (the one of cell col + 1 is its right
// edge).
Vec2 tilemapCellOrigin(const Tilemap& map, const Transform& t, int col, int row);

// Whether that tile is in solidTiles.
bool isSolidTile(const Tilemap& map, int tile);
// Whether the cell blocks: the layer has collision and the cell's tile is solid.
// Outside the map nothing is solid (you can leave through the edges or fall).
bool isSolidCell(const Tilemap& map, int col, int row);
// Whether any Tilemap in the World has a solid cell at that world point.
bool isSolidAt(World& world, Vec2 point);

// Columns and rows of the tileset (image size / tile size). false if the image does
// not load or the tile does not fit in it.
bool tilesetGrid(AssetManager& assets, const Tilemap& map, int& columns, int& rows);
