#include "components/MotionComponents.h"

#include "core/Reflection.h"

void registerMotionComponents(ComponentRegistry& registry) {
    // The list goes last: in the file it takes one line per element.
    registry.add<PathMover>("PathMover")
        .field("speed", &PathMover::speed).range(0.0f, 5000.0f).speed(1.0f).tooltip("px/s")
        .field("waitTime", &PathMover::waitTime).range(0.0f, 60.0f).speed(0.01f).tooltip("Seconds of pause at each point")
        .field("loop", &PathMover::loop).tooltip("Checked: loop (from the last point back to the first). Unchecked: back and forth")
        .list("points", &PathMover::points, [](FieldsBuilder<PathPoint>& point) {
            point.field("offset", &PathPoint::offset).speed(1.0f)
                .tooltip("World px relative to the starting position (which is the first point)");
        });

    registry.add<FallingPlatform>("FallingPlatform")
        .field("delay", &FallingPlatform::delay).range(0.0f, 10.0f).speed(0.01f).tooltip("Seconds between being stepped on and falling")
        .field("gravity", &FallingPlatform::gravity).range(0.0f, 10000.0f).speed(5.0f).tooltip("px/s^2 while falling")
        .field("maxFallSpeed", &FallingPlatform::maxFallSpeed).range(0.0f, 5000.0f).speed(5.0f).tooltip("px/s")
        .field("respawnTime", &FallingPlatform::respawnTime).range(0.0f, 60.0f).speed(0.05f)
            .tooltip("Seconds from starting to fall until it is back in place (0 = never comes back)")
        .field("fadeInTime", &FallingPlatform::fadeInTime).range(0.0f, 10.0f).speed(0.01f)
            .tooltip("Seconds to fade in when it comes back (fades the Sprite tint; 0 = instantly)");

    registry.add<Bouncer>("Bouncer")
        .field("bounceHeight", &Bouncer::bounceHeight).range(0.0f, 5000.0f).speed(1.0f).tooltip("How high the bounced body rises (px)")
        .field("targetTag", &Bouncer::targetTag).tooltip("Tag of whom it launches (empty = any RigidBody2D)");
}
