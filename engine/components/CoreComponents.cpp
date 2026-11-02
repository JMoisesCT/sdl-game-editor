#include "components/CoreComponents.h"

#include "core/Reflection.h"

void registerCoreComponents(ComponentRegistry& registry) {
    registry.add<EntityInfo>("EntityInfo")
        .notRemovable()
        .field("name", &EntityInfo::name)
        .field("tag", &EntityInfo::tag).tooltip("Clasifica a la entidad para el codigo (Player, Enemy...)");

    registry.add<Transform>("Transform")
        .field("position", &Transform::position).speed(1.0f).tooltip("Centro de la entidad en el mundo (px)")
        .field("rotation", &Transform::rotation).range(-360.0f, 360.0f).speed(1.0f)
        .field("scale", &Transform::scale).speed(0.05f);

    registry.add<Sprite>("Sprite")
        .field("texture", &Sprite::texture).texture()
        .field("source", &Sprite::source).tooltip("Recorte x, y, ancho, alto (0 = imagen completa)")
        .field("tint", &Sprite::tint)
        .field("flipX", &Sprite::flipX)
        .field("flipY", &Sprite::flipY)
        .field("visible", &Sprite::visible)
        .field("order", &Sprite::order).tooltip("Capa de dibujo: menor = mas al fondo");

    // cells va al final: en el archivo ocupa una linea por fila.
    registry.add<Tilemap>("Tilemap")
        .field("tileset", &Tilemap::tileset).texture()
        .field("tileWidth", &Tilemap::tileWidth).range(1.0f, 512.0f).tooltip("Ancho de un tile en la imagen (px)")
        .field("tileHeight", &Tilemap::tileHeight).range(1.0f, 512.0f).tooltip("Alto de un tile en la imagen (px)")
        .field("solidTiles", &Tilemap::solidTiles).tooltip("Tiles que frenan, separados por coma (o marcalos en la paleta)")
        .field("collision", &Tilemap::collision).tooltip("Sin marcar: capa decorativa, nunca frena")
        .field("visible", &Tilemap::visible)
        .field("tint", &Tilemap::tint).tooltip("El alfa es la opacidad de la capa")
        .field("order", &Tilemap::order).tooltip("Capa de dibujo: menor = mas al fondo (comun con los Sprite)")
        .field("cells", &Tilemap::cells).tooltip("Tamano del mapa en celdas (ancho, alto). Achicar borra lo que queda fuera");

    registry.add<Camera>("Camera")
        .field("zoom", &Camera::zoom).range(0.1f, 10.0f).speed(0.01f)
        .field("background", &Camera::background);
}
