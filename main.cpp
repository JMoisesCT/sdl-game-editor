#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "app/Application.h"
#include "game/PlatformerControl.h"
#include "game/PlatformerRules.h"
#include "systems/Animation.h"
#include "systems/AudioSystems.h"
#include "systems/Motion.h"
#include "systems/Physics.h"
#include "systems/PlatformerMotorSystem.h"
#include "systems/Presentation.h"
#include "systems/Rules.h"

// The application owns the loop: here we only configure it, register the game's
// pieces (components, conditions and systems) and open the scene.
//
// With the --play argument it starts without the editor, playing the scene directly
// (as the player would see it). --scale=pixel (the default), fit or 100 picks how the
// game is shown if the window is not the size of its resolution.
int main(int argc, char* argv[]) {
    AppConfig config;
    config.title = "SDL Game Editor";
    config.gameWidth = 1280;
    config.gameHeight = 720;
    for (int i = 1; i < argc; ++i) {
        if (SDL_strcmp(argv[i], "--play") == 0) config.editor = false;
        // --scale=pixel | fit | 100: how the game is shown (see app/GameScreen.h).
        else if (SDL_strncmp(argv[i], "--scale=", 8) == 0 && !parseGameScale(argv[i] + 8, config.scale))
            SDL_Log("Unknown scale '%s' (pixel, fit or 100)", argv[i] + 8);
    }

    Application app;
    if (!app.init(config)) return 1;

    // Game components: before opening the scene, so they get loaded. And its
    // conditions (the engine's are already there).
    registerPlatformerComponents(app.registry());
    registerPlatformerConditions(app.conditions());

    // Systems. They run in the order they are added, and here the order is the chain of
    // each frame:
    //   keys -> intent -> velocity -> position and collisions -> rules -> playthrough
    //   -> presentation (animation, sound, camera, background).
    app.addSystem(std::make_unique<PlatformerControlSystem>()); // game: keys -> intent
    app.addSystem(std::make_unique<PlatformerMotorSystem>());   // motor: intent -> velocity
    // Mechanisms (see systems/Motion.h): the platforms move before physics, carrying
    // whoever is on top; the trampoline looks at the already resolved positions.
    app.addSystem(std::make_unique<PathMoverSystem>());
    app.addSystem(std::make_unique<FallingPlatformSystem>());
    app.addSystem(std::make_unique<PhysicsSystem>());           // motor: movement and collisions
    app.addSystem(std::make_unique<BouncerSystem>());
    // Engine rules (see systems/Rules.h): HealthSystem first, because it clears the
    // one-frame flags the others set.
    app.addSystem(std::make_unique<HealthSystem>());
    app.addSystem(std::make_unique<HazardSystem>());
    app.addSystem(std::make_unique<KillZoneSystem>());
    app.addSystem(std::make_unique<CollectibleSystem>());
    app.addSystem(std::make_unique<CheckpointSystem>());
    app.addSystem(std::make_unique<RespawnSystem>());
    app.addSystem(std::make_unique<PlatformerRulesSystem>());   // game: fruits, goal and HUD
    // Presentation: with everything already resolved this frame. The state machine picks
    // the clip and the animator advances it; the sounds see the one-frame flags (jumped,
    // damaged...) before they are cleared; the camera follows the final position (also
    // after respawning).
    app.addSystem(std::make_unique<AnimatorStateMachineSystem>(app.conditions()));
    app.addSystem(std::make_unique<SpriteAnimatorSystem>(app.assets()));
    app.addSystem(std::make_unique<SoundSystem>(app.audio(), app.conditions()));
    app.addSystem(std::make_unique<MusicSystem>(app.audio()));
    app.addSystem(std::make_unique<FollowCameraSystem>(config.gameWidth, config.gameHeight));
    app.addSystem(std::make_unique<ParallaxSystem>());

    app.openScene("assets/scenes/platformer.scene.json");
    return app.run();
}
