#include "components/CoreComponents.h"

#include "core/Reflection.h"

void registerCoreComponents(ComponentRegistry& registry) {
    registry.add<EntityInfo>("EntityInfo")
        .notRemovable()
        .field("name", &EntityInfo::name)
        .field("tag", &EntityInfo::tag).tooltip("Classifies the entity for code (Player, Enemy...)");

    registry.add<Transform>("Transform")
        .field("position", &Transform::position).speed(1.0f).tooltip("Center of the entity in the world (px)")
        .field("rotation", &Transform::rotation).range(-360.0f, 360.0f).speed(1.0f)
        .field("scale", &Transform::scale).speed(0.05f);

    registry.add<Sprite>("Sprite")
        .field("texture", &Sprite::texture).texture()
        .field("source", &Sprite::source).tooltip("Crop x, y, width, height (0 = whole image)")
        .field("tint", &Sprite::tint)
        .field("flipX", &Sprite::flipX)
        .field("flipY", &Sprite::flipY)
        .field("visible", &Sprite::visible)
        .field("order", &Sprite::order).tooltip("Draw layer: lower = further back");

    // cells goes last: in the file it takes one line per row.
    registry.add<Tilemap>("Tilemap")
        .field("tileset", &Tilemap::tileset).texture()
        .field("tileWidth", &Tilemap::tileWidth).range(1.0f, 512.0f).tooltip("Width of a tile in the image (px)")
        .field("tileHeight", &Tilemap::tileHeight).range(1.0f, 512.0f).tooltip("Height of a tile in the image (px)")
        .field("solidTiles", &Tilemap::solidTiles).tooltip("Blocking tiles, comma separated (or mark them in the palette)")
        .field("collision", &Tilemap::collision).tooltip("Unchecked: decorative layer, never blocks")
        .field("visible", &Tilemap::visible)
        .field("tint", &Tilemap::tint).tooltip("The alpha is the layer's opacity")
        .field("order", &Tilemap::order).tooltip("Draw layer: lower = further back (shared with Sprites)")
        .field("cells", &Tilemap::cells).tooltip("Map size in cells (width, height). Shrinking erases what falls outside");

    registry.add<Camera>("Camera")
        .field("zoom", &Camera::zoom).range(0.1f, 10.0f).speed(0.01f)
        .field("background", &Camera::background)
        .field("pixelSize", &Camera::pixelSize).range(0.1f, 64.0f).speed(0.05f)
            .tooltip("World px covered by one art pixel (the sprite scale, e.g. 2). "
                     "With the 'Pixel perfect' scale, each art pixel is a whole number of screen px");
}
