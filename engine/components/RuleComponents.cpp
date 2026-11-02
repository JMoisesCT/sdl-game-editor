#include "components/RuleComponents.h"

#include "core/Reflection.h"

void registerRuleComponents(ComponentRegistry& registry) {
    registry.add<Health>("Health")
        .field("maxHP", &Health::maxHP).range(1.0f, 9999.0f).tooltip("Vida al empezar y al reaparecer")
        .field("invulnerabilityTime", &Health::invulnerabilityTime).range(0.0f, 10.0f).speed(0.01f)
            .tooltip("Seg sin recibir dano tras un golpe (el sprite parpadea)")
        .field("blinkInterval", &Health::blinkInterval).range(0.01f, 1.0f).speed(0.01f).tooltip("Seg entre parpadeos")
        .field("destroyOnDeath", &Health::destroyOnDeath).tooltip("Al morir se destruye (enemigos). No usar con Respawn");

    registry.add<Hazard>("Hazard")
        .field("damage", &Hazard::damage).range(0.0f, 9999.0f).tooltip("Vida que quita cada golpe")
        .field("targetTag", &Hazard::targetTag).tooltip("Etiqueta de a quien dana")
        .field("knockback", &Hazard::knockback).speed(1.0f).tooltip("Empujon (px/seg): x alejandose, y hacia arriba")
        .field("repeat", &Hazard::repeat).tooltip("Sin marcar: dana una sola vez");

    registry.add<KillZone>("KillZone")
        .field("targetTag", &KillZone::targetTag).tooltip("Etiqueta de a quien mata (ignora la invulnerabilidad)");

    registry.add<Collectible>("Collectible")
        .field("collectorTag", &Collectible::collectorTag).tooltip("Etiqueta de quien lo puede recoger")
        .field("destroyOnCollect", &Collectible::destroyOnCollect).tooltip("Desaparece al recogerlo");

    registry.add<Checkpoint>("Checkpoint")
        .field("targetTag", &Checkpoint::targetTag).tooltip("Etiqueta de quien lo activa (necesita Respawn)")
        .field("offset", &Checkpoint::offset).speed(0.5f).tooltip("Punto de reaparicion respecto al centro (px de la imagen)")
        .field("activeTexture", &Checkpoint::activeTexture).texture().tooltip("Imagen del Sprite una vez activado (vacio = no cambia)");

    registry.add<Respawn>("Respawn")
        .field("delay", &Respawn::delay).range(0.0f, 10.0f).speed(0.01f).tooltip("Seg entre la muerte y la reaparicion");
}
