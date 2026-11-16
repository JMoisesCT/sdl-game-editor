#pragma once
#include "core/Entity.h"
#include "core/Math.h"
#include "systems/System.h"

class World;

// Box (AABB) physics for 2D pixel art games. Each step:
//   1. Moves the bodies (RigidBody2D + Transform): adds gravity to the velocity and
//      the velocity to the position.
//   2. Separates each body with a BoxCollider from the solid cells of the Tilemaps.
//   3. Separates the pairs of boxes that overlap (except triggers).
//   4. Sets 'grounded' on the bodies that ended up resting on something.
//
// AGAINST TILEMAPS it resolves ONE AXIS AT A TIME: first X (with the Y the body had
// before moving) and then Y. Resolving both axes at once, along the side of least
// penetration, makes the character snag on the SEAMS between two neighboring ground
// tiles: a tiny vertical penetration is mistaken for a side collision with the next
// tile and stops it dead. Separating by axis, the X pass only sees walls and the
// Y pass only sees floors and ceilings.
//
// STABLE GROUNDING: besides the separation, a strip of GROUND_PROBE px under the
// feet is checked. Without that probe, grounding would depend on the body penetrating
// the ground that frame, which at a high framerate is a fraction of a pixel (or
// nothing): 'grounded' would flicker and the jump would sometimes fail.
//
// It runs AFTER the systems that decide the velocity (player control, platformer
// motor) and before the ones that react to the final position (camera, rules).
//
// ONE-WAY PLATFORMS (BoxCollider::oneWay): they only stop a body that is falling and
// whose feet were above the platform in the previous step (with ONE_WAY_TOLERANCE
// of margin, for platforms that rise and catch up with it). They never push sideways
// or downward. A body with RigidBody2D::ignoreOneWay > 0 (the PlatformerMotor sets it
// with down + jump) passes through them downward while it lasts.
class PhysicsSystem : public System {
public:
    static constexpr float GROUND_PROBE = 2.0f;      // world px under the feet
    static constexpr float ONE_WAY_TOLERANCE = 4.0f; // px the feet may be below the edge

    const char* name() const override { return "Physics"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};

// Collision box of an entity in the world (min = top left corner).
// false if it has no BoxCollider + Transform.
bool colliderBounds(World& world, Entity e, Vec2& min, Vec2& max);

// Whether the boxes of two entities overlap (touching edge to edge does not count).
// Includes triggers: useful to collect, damage or activate without going through physics.
// With margin > 0 the boxes grow that many px on each side: that way the CONTACT
// between two solid boxes, which physics leaves exactly edge to edge, also counts.
bool collidersOverlap(World& world, Entity a, Entity b, float margin = 0.0f);

// Whether any Tilemap has a solid cell inside that world rectangle.
bool isSolidArea(World& world, Vec2 min, Vec2 max);

// Whether 'body' (with a RigidBody2D) rests ONLY on one-way platforms (not on a
// solid tile or a regular box): it can drop down from there. See dropThroughTime.
bool isOnOneWayOnly(World& world, Entity body);

// Whether 'body' (with a RigidBody2D) stands on the solid box of 'platform': grounded
// in the last physics step, not rising, with its feet on the platform's top edge and
// overlapping in X.
bool isStandingOn(World& world, Entity body, Entity platform);

// Moves a KINEMATIC entity (no RigidBody2D, moved by a system: a moving platform, a
// falling one) and with it the bodies standing on top. Without this, the platform
// would slide out from under their feet (sideways) or shove them in jerks (going up).
// Call it BEFORE PhysicsSystem, which then sees the passengers in their new place.
void moveCarrying(World& world, Entity platform, Vec2 delta);
