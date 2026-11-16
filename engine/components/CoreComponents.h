#pragma once
#include <string>
#include <vector>

#include "core/Math.h"

class ComponentRegistry;

// Basic engine components. They are ONLY data: no lifecycle methods and no SDL.
// What they do is decided by the systems (RenderSystem draws them).
// Their fields are declared to the reflection in CoreComponents.cpp: a new field that
// should be saved and shown in the editor is also added there.

// Name and tag. The name identifies an entity for people (the editor's Hierarchy);
// the tag classifies it for code ("Player", "Enemy").
// The usual rule: code filters by tag, never by name.
struct EntityInfo {
    std::string name = "Entidad";
    std::string tag;
};

// Position, rotation and scale. The position is the CENTER of the entity in the
// world (pixels), not its corner.
struct Transform {
    Vec2 position;
    float rotation = 0.0f;    // degrees, clockwise (like SDL)
    Vec2 scale{ 1.0f, 1.0f }; // 1 = the image's original size
};

// Image drawn centered on the Transform. Size in the world = crop (or the whole
// texture if there is no crop) times the Transform's scale.
struct Sprite {
    std::string texture;    // image path, relative to the executable's folder
    IntRect source;         // crop within the texture; w or h = 0 -> whole texture
    Color tint;             // white = no tint
    bool flipX = false;
    bool flipY = false;
    bool visible = true;
    int order = 0;          // draw layer: lower = further back (on a tie, the id)
};

// One LAYER of tiles: a grid of cells, each with a tile cropped from the tileset.
// A map with several layers (background, ground, decoration...) is several entities
// with a Tilemap: that way each layer has its own order and interleaves with sprites
// (the player can be in front of the ground and behind some plants), and the
// Hierarchy works as the list of layers.
//
// ANCHOR: unlike the Sprite, the Transform marks the TOP LEFT corner of cell (0, 0),
// not the center: that way enlarging the map does not shift it. A cell measures
// tileWidth x tileHeight in the world, times the Transform's scale. Rotation and the
// sign of the scale do not apply to a tilemap.
//
// CELLS: -1 = empty; n >= 0 = tile n of the tileset, counting by rows from the top
// left (with 22 columns, 22 is the first one of the second row). The tileset is a
// grid of tiles with no margins or spacing; its columns come from the width of the
// image.
//
// COLLISION: a cell is solid if its tile is in solidTiles and the layer has
// collision. A layer without collision is decoration: it never blocks.
struct Tilemap {
    std::string tileset;          // image with the tiles
    int tileWidth = 16;           // size of a tile in the image (px)
    int tileHeight = 16;
    std::vector<int> solidTiles;  // tiles that block
    bool collision = true;
    bool visible = true;
    Color tint;                   // white = no tint; the alpha is the layer's opacity
    int order = -10;              // draw layer, on the same scale as Sprite::order
    IntGrid cells{ 32, 18, -1 };  // the cells (width x height); starts empty
};

// What the player sees. The position comes from the Transform of the same entity:
// that point is at the center of the view. If there are several, the game uses the first one found.
//
// pixelSize is how many WORLD px one ART pixel measures: the scale the sprites and
// tilemaps were placed with (2 if everything uses scale 2). It is used by the "Pixel
// perfect" scale (see app/GameScreen.h) when the game is shown smaller or larger than
// its resolution: it adjusts the zoom so each art pixel measures a WHOLE number of
// screen pixels, instead of 2.1 (blurry) or columns of 2 and 3 (distorted).
struct Camera {
    float zoom = 1.0f;      // 2 = everything looks twice as big
    Color background{ 32, 32, 40, 255 }; // color the game view is cleared with
    float pixelSize = 1.0f; // world px per art pixel (the sprite scale)
};

// Declares the components above to the reflection. Application calls it at startup,
// before the game registers its own.
void registerCoreComponents(ComponentRegistry& registry);
