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

    registry.add<Camera>("Camera")
        .field("zoom", &Camera::zoom).range(0.1f, 10.0f).speed(0.01f)
        .field("background", &Camera::background);
}
