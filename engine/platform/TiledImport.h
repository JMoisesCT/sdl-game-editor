#pragma once
#include <string>
#include <vector>

#include "components/CoreComponents.h"
#include "core/Entity.h"
#include "core/Math.h"

class World;

// Importar un mapa de Tiled (.json) UNA vez: cada capa de tiles pasa a ser una entidad
// con Tilemap, y desde ahi el mapa se edita en el editor y se guarda en la escena (el
// .json de Tiled ya no se vuelve a leer).
//
// Supuestos del export (los mismos del v1):
//   - Mapa ortogonal y finito, capas en CSV (no base64 ni comprimidas).
//   - Un solo tileset, EMBEBIDO en el mapa (no un .tsx externo), sin margen ni
//     separacion entre tiles.
//   - Se leen todas las capas de tiles en el orden de Tiled (la primera, al fondo); los
//     grupos se aplanan y heredan visible y opacity. Las capas de objetos se ignoran.
//   - Propiedades: "solid" (bool) en los tiles del tileset -> solidTiles; "collision"
//     (bool) en una capa -> Tilemap::collision (si falta, true).
//   - Los tiles volteados o rotados se cargan sin voltear (se avisa en el log).
// Indices: Tiled usa 0 = vacio y los tiles empiezan en firstgid; el motor usa -1 =
// vacio y 0 = primer tile.

struct ImportedLayer {
    std::string name;
    Tilemap tilemap;
};

// Lee el mapa sin tocar ningun World: si falla (error dice por que), no queda nada a
// medias. Las rutas de imagen se pasan a relativas a la carpeta del ejecutable.
bool loadTiledMap(const std::string& path, std::vector<ImportedLayer>& layers, std::string& error);

// Crea una entidad por capa (EntityInfo + Transform + Tilemap), todas con la esquina
// del mapa en 'origin' y esa escala. Las capas quedan debajo de los sprites: la de
// delante con order -10, la anterior -11, etc. Devuelve las entidades creadas.
std::vector<Entity> addImportedLayers(World& world, const std::vector<ImportedLayer>& layers,
                                      Vec2 origin, float scale);
