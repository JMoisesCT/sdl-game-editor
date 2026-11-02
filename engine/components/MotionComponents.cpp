#include "components/MotionComponents.h"

#include "core/Reflection.h"

void registerMotionComponents(ComponentRegistry& registry) {
    // La lista va al final: en el archivo ocupa una linea por elemento.
    registry.add<PathMover>("PathMover")
        .field("speed", &PathMover::speed).range(0.0f, 5000.0f).speed(1.0f).tooltip("px/seg")
        .field("waitTime", &PathMover::waitTime).range(0.0f, 60.0f).speed(0.01f).tooltip("Seg de pausa en cada punto")
        .field("loop", &PathMover::loop).tooltip("Marcado: circuito (del ultimo punto vuelve al primero). Sin marcar: ida y vuelta")
        .list("points", &PathMover::points, [](FieldsBuilder<PathPoint>& point) {
            point.field("offset", &PathPoint::offset).speed(1.0f)
                .tooltip("px del mundo respecto a la posicion inicial (que es el primer punto)");
        });

    registry.add<FallingPlatform>("FallingPlatform")
        .field("delay", &FallingPlatform::delay).range(0.0f, 10.0f).speed(0.01f).tooltip("Seg entre que la pisan y que cae")
        .field("gravity", &FallingPlatform::gravity).range(0.0f, 10000.0f).speed(5.0f).tooltip("px/seg^2 al caer")
        .field("maxFallSpeed", &FallingPlatform::maxFallSpeed).range(0.0f, 5000.0f).speed(5.0f).tooltip("px/seg")
        .field("respawnTime", &FallingPlatform::respawnTime).range(0.0f, 60.0f).speed(0.05f)
            .tooltip("Seg desde que empieza a caer hasta volver a su lugar (0 = no vuelve)");

    registry.add<Bouncer>("Bouncer")
        .field("bounceHeight", &Bouncer::bounceHeight).range(0.0f, 5000.0f).speed(1.0f).tooltip("px que sube lo que rebota")
        .field("targetTag", &Bouncer::targetTag).tooltip("Etiqueta de a quien lanza (vacio = cualquier RigidBody2D)");
}
