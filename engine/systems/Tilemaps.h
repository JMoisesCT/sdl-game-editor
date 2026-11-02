#pragma once
#include "core/Math.h"

class World;
class AssetManager;
struct Tilemap;
struct Transform;

// Consultas sobre un Tilemap: pasar del mundo a las celdas y al reves, y saber que
// celdas son solidas. Las usan el dibujo, el editor (pincel, seleccion) y la fisica.
// El origen del mapa es el Transform: la esquina superior izquierda de la celda (0, 0).

// Tamano de UNA celda en el mundo (el tile de la imagen por la escala del Transform).
Vec2 tilemapCellSize(const Tilemap& map, const Transform& t);

// Rectangulo que ocupa el mapa entero en el mundo (min = esquina superior izquierda).
void tilemapBounds(const Tilemap& map, const Transform& t, Vec2& min, Vec2& max);

// Celda bajo un punto del mundo. col y row se calculan siempre, aunque caigan FUERA
// del mapa (negativas o >= tamano); en ese caso devuelve false.
bool tilemapWorldToCell(const Tilemap& map, const Transform& t, Vec2 world, int& col, int& row);

// Esquina superior izquierda de una celda en el mundo (la de la celda col + 1 es su
// borde derecho).
Vec2 tilemapCellOrigin(const Tilemap& map, const Transform& t, int col, int row);

// Si ese tile esta en solidTiles.
bool isSolidTile(const Tilemap& map, int tile);
// Si la celda frena: la capa tiene collision y el tile de la celda es solido. Fuera
// del mapa no es solido (se puede salir por los bordes o caer).
bool isSolidCell(const Tilemap& map, int col, int row);
// Si algun Tilemap del World tiene una celda solida en ese punto del mundo.
bool isSolidAt(World& world, Vec2 point);

// Columnas y filas del tileset (tamano de la imagen / tamano del tile). false si la
// imagen no carga o el tile no cabe en ella.
bool tilesetGrid(AssetManager& assets, const Tilemap& map, int& columns, int& rows);
