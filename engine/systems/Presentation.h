#pragma once
#include "systems/System.h"

class World;

// PRESENTATION systems that are not animation (components in
// components/PresentationComponents.h). They run at the end of the frame (see main.cpp):
// the camera follows the position already resolved by physics and the rules (a
// respawn included).

// Moves the cameras that have a FollowCamera. Needs the size of the game view (the
// AppConfig resolution) so that, with clampToTilemaps, the VISIBLE RECTANGLE does not
// leave the map.
class FollowCameraSystem : public System {
public:
    FollowCameraSystem(int viewWidth, int viewHeight) : viewWidth(viewWidth), viewHeight(viewHeight) {}
    const char* name() const override { return "FollowCamera"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;

private:
    int viewWidth;
    int viewHeight;
};

// Advances the own scrolling (scroll) of the ParallaxBackgrounds.
class ParallaxSystem : public System {
public:
    const char* name() const override { return "Parallax"; }
    void onPlay(World& world) override;
    void update(World& world, float dt) override;
};
