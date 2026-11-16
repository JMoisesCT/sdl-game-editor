**English** | [Español](README.es.md)

# SDL Game Editor — an educational 2D engine and editor built on SDL3

A 2D game engine with a **built-in editor**, written in C++17 on top of SDL3. Its goal is
**educational**: students who already know basic SDL3 can build games by editing scenes in
an editor, without fighting the raw SDL API.

> **Status:** in development, in phases. Done so far: the core (ECS + reflection + JSON
> scenes), the editor, tilemaps (painted with a palette inside the editor), physics with a
> platformer controller, prefabs, gameplay rules (health, damage, collectibles,
> checkpoints), presentation (rule-driven animation, follow camera, parallax background,
> text and HUD), audio and stage mechanisms (one-way, moving, falling platforms and
> trampolines). The sample scene is a complete platformer level. Top-down and shoot 'em up
> samples come next.

## Design

- **The scene is a document.** Everything in it is saved to a `.scene.json`, and the editor
  works directly on that data: what you see is what gets saved.
- **A custom ECS** (entities, components and systems), written to be read in class:
  - **Entity**: just a number.
  - **Component**: a plain data `struct` with no logic (`Transform`, `Sprite`, `Camera`...).
  - **System**: the behavior; it walks the entities that have certain components and
    changes them. Systems only run while the game is playing.
- **Minimal reflection.** Each component declares its fields once:

  ```cpp
  registry.add<Transform>("Transform")
      .field("position", &Transform::position)
      .field("rotation", &Transform::rotation).range(-360, 360)
      .field("scale",    &Transform::scale);
  ```

  From that list the engine saves and loads it as JSON, and the editor's Inspector shows
  and edits it. A new component shows up in the editor **without touching the editor**. A
  field can also be a **list of structs** (the clips of an animation), declared with
  `.list(...)`; a string can offer **choices** (`.choices(...)`) that the Inspector shows
  as a dropdown, and an `Entity` field is a **reference to another entity** in the scene
  (picked from a list or by dragging it from the Hierarchy).
- **The application owns the loop.** A game does not write its own SDL `main` loop: it
  registers its components and systems, and opens a scene.

  ```cpp
  int main(int argc, char* argv[]) {
      Application app;
      if (!app.init({ "My Game", 1280, 720 })) return 1;
      app.registry().add<MyComponent>("MyComponent").field(/* ... */);
      app.addSystem(std::make_unique<MySystem>());
      app.openScene("assets/scenes/level1.scene.json");
      return app.run();
  }
  ```

### Your own code

To add your own behavior with values you can edit in the Inspector (a component, its
registration and a system, all in `game/`), see the step-by-step guide
**[docs/GUIA_CODIGO_PROPIO.md](docs/GUIA_CODIGO_PROPIO.md)** (in Spanish): a complete
example, the field types, references to other entities, conditions for animation and
sound, and the most common mistakes.

## The editor

- **Hierarchy** (left): the scene's entities as a **tree**; create, duplicate and delete.
  Prefab instances are shown in blue. See [Hierarchy](#hierarchy).
- **Prefabs** (left, bottom): the prefabs in `assets/`; double-click one or drag it into
  the view to place it.
- **Inspector** (right): the components of the selected entity, editable; add and remove
  components.
- **View** (center), with two tabs:
  - **Scene**: a free editor camera, grid, selection and dragging.
  - **Game**: what the player sees. A dropdown in its top-right corner picks how it is
    scaled: **Pixel perfect** (default), **Fit** or **100%**.
- **Play / Pause / Stop**: on Stop, the scene goes back to exactly how it was before
  playing.

### Hierarchy

Entities can be **nested** to organize the scene into groups ("Fruits", "Traps",
"HUD"...). It is an **organizational** hierarchy: each entity's `Transform` is still its
position in the world, so physics, drawing and rules do not change.

- **Drag** an entity onto another to make it its **child**; onto the top or bottom edge of
  another, to place it **before** or **after** it (as a sibling). Dropping it in the empty
  space below takes it out of its group. The right-click menu offers **Create child**,
  **Group** and **Remove from group**.
- **Select several** with `Ctrl+click` (add or remove one) or `Shift+click` (a range).
  Dragging one of them takes them all, in order; **Duplicate**, **Delete** and **Remove from
  group** act on all of them. The Inspector shows the last one picked.
- **Group** (`Ctrl+G`) creates an empty entity in place of the first selected one and puts
  the whole selection inside it: a folder in one step. It is selected, ready to be renamed.
- A group can also be an empty entity made by hand (**+ New**, no sprite).
- **Moving a parent** (in the view or in the Inspector) also moves its whole branch, and
  **Duplicate** or **Delete** take the whole branch along.
- **While playing**, children do **not** follow their parent (if a system destroys the
  parent, the children are left on their own; to take them along, use
  `destroyWithChildren`).
- A prefab is **one** entity: when you create a prefab from a parent, its children are not
  included.

In the file, a child stores its parent's id: `{ "id": 7, "parent": 3, "components": {...} }`,
and entities are saved in tree order.

### Tilemaps

A tile level is built from entities with the **`Tilemap`** component: each one is a
**layer** (background, ground, decoration...). Since each layer has its own `order`, it
interleaves with sprites: the player can be in front of the ground and behind some plants.
Its `Transform` marks the map's **top-left corner** (not the center, as with sprites).

- Select a layer: the Inspector shows the tileset **palette**. Click a tile to pick it, or
  drag to pick a block of several.
- Tools: **Brush** (`B`), **Eraser** (`E`), **Fill** (`G`) and **Move** (`V`). `Alt+click`
  in the view picks a cell's tile. Each stroke is undone in one step.
- **Solids**: with "Mark solids" on, clicking a tile in the palette marks or unmarks it as
  solid. "Show solids" highlights the blocking cells in the view. A layer with `collision`
  unchecked is pure decoration.
- The map size (`cells`) is changed in the Inspector, when you press Enter.
- For a new layer: **+ New**, then **Add component > Tilemap**, and choose the tileset image
  in the `tileset` field.

Maps are built **only in this editor**: there is no separate map file or external editor.
Cells are stored inside the `.scene.json`, one row per line (`-1` = empty), so the map is
readable in the file and a diff shows only the rows that changed.

### Physics and the platformer controller

- **`RigidBody2D`**: the entity moves on its own with its `velocity` and `gravity`
  (px/s²; 0 for top-down or shmup games). Without it, an entity is static.
- **`BoxCollider`**: a collision box centered on the `Transform`. `size` and `offset` are in
  **image** pixels, multiplied by the `Transform` scale (like the sprite); the offset is
  mirrored when the sprite is flipped. Without a `RigidBody2D` it is a fixed obstacle; with
  `isTrigger` it neither blocks nor gets blocked; with `oneWay` it is a **one-way
  platform** (it only stops what falls onto it from above).
- Bodies collide with the solid cells of tilemaps and with each other. Against tiles,
  collisions are resolved **one axis at a time** (so bodies do not snag on the seams of the
  ground) and a 2 px probe under the feet decides whether the body is grounded.
- **`PlatformerMotor`**: running and jumping that feel good: acceleration and braking, jump
  height **in pixels**, short jump when the button is released, *coyote time*, *jump
  buffer*, air jumps (`maxAirJumps = 1` = double jump) and dropping through one-way
  platforms (down + jump). All its knobs are in the Inspector: tweaking them while playing
  is the best exercise for understanding *game feel*.
- The controller does not read the keyboard: a game system does that
  (`game/PlatformerControl`) for entities tagged `Player`. Arrows or `A`/`D` to move;
  `Space`, `W` or the up arrow to jump; down (`↓` or `S`) + jump to drop through a one-way
  platform.
- The **Colliders** checkbox in the toolbar draws the boxes in the Scene view (green:
  solid; yellow: trigger).

Systems run in the order `main.cpp` adds them, and that order is the chain of each frame:
control (keys → intent) → `PlatformerMotorSystem` (intent → velocity) → mechanisms
(moving and falling platforms) → `PhysicsSystem` (velocity → position, collisions and
grounding) → trampolines → rules (health, damage, collectibles, checkpoints, respawn) →
game rules and HUD → presentation (animation, sound, camera, background).

### Mechanisms

Stage pieces that move. Moving platforms are **kinematic** (no `RigidBody2D`): they move
whoever is standing on them.

- **`PathMover`**: follows a list of points (relative to its starting position) at a given
  speed, with a pause at each point; as a loop or back and forth. The path is drawn in the
  Scene view.
- **`FallingPlatform`**: falls a moment after being stepped on and comes back after a while,
  fading in.
- **`Bouncer`**: a trampoline: launches whoever lands on it from above to a height in
  pixels.

### Prefabs

A **prefab** is a template entity saved in its own file (`assets/prefabs/*.prefab.json`):
the player, a fruit, some spikes. They take the place that, in a "code-first" engine, a
function that builds each object would have.

- **Place**: double-click it in the Prefabs panel (it appears at the center of the view) or
  drag it into the view.
- An **instance** stays **linked** to its prefab: the scene only stores how it differs
  from it (the position, the name, the texture of a *different* fruit...). Changing the
  prefab changes all its instances, except for what each one overrode. In the Inspector,
  overridden fields are shown in **orange**.
- **Create prefab...** (Inspector of a standalone entity): saves it as a prefab and links
  it.
- **Apply**: the instance's changes go into the prefab (and so to all its instances),
  except position and name. The prefab file is saved right away: Undo reverts the scene,
  not the prefab.
- **Revert**: the instance matches the prefab again (keeping its position and name).
- **Unlink**: it stops being an instance; it keeps its components.
- **Refresh** (Prefabs panel) reloads the prefabs from disk, in case they were edited by
  hand.

In a scene file, an instance looks like this:

```json
{ "id": 4, "prefab": "assets/prefabs/fruit.prefab.json", "components": {
    "EntityInfo": {"name":"Banana"},
    "Transform": {"position":[576.0,384.0]},
    "Sprite": {"texture":"assets/pixel_adventure/Items/Fruits/Bananas.png"}
}}
```

### Gameplay rules

Data components and engine systems that work through **box contact** (`BoxCollider`,
usually a trigger) and decide whom they affect by **tag**:

- **`Health`**: health, invulnerability after a hit (the sprite blinks) and death.
- **`Hazard`**: takes health from whoever touches it and knocks them back (spikes, saws).
- **`KillZone`**: kills instantly, even when invulnerable (the strip below the level).
- **`Collectible`**: collected on touch (fruits, coins); it disappears at once or, like the
  sample's fruits, when its collect animation ends.
- **`Checkpoint`**: sets the respawn point of whoever touches it (its flag animates with an
  `activated` rule).
- **`Respawn`**: on the player; on death it waits a moment and respawns at the last
  checkpoint with full health.

There are no *callbacks*: whatever happens in a frame is left as a one-frame flag on the
component (`justDamaged`, `justDied`, `justCollected`...). Rules specific to **one** game go
in `game/`: in the sample, `PlatformerRules` counts the collected fruits, registers the
`LevelGoal` component (the goal) and writes the HUD.

### Presentation

- **`SpriteAnimator`**: a list of **clips**, each a sequence of frames from a grid image (one
  strip per file, a row or column of a sheet, or individual cells). Per clip: `fps`, `loop`
  and, if it does not loop, what happens when it ends: continue with another clip (`next`)
  or destroy the entity (`destroyOnEnd`, for effects). It does not draw: it writes the image
  and the crop of the `Sprite`. A clip without a `texture` uses the image the `Sprite` had:
  that way each fruit instance is still a `Sprite` *override*.
- **`AnimatorStateMachine`**: picks the clip with **rules** "clip if condition", in order;
  the first one that holds wins and, if none does, `defaultClip` plays. Conditions are named
  C++ functions (see below).
- **`FollowCamera`** (on the `Camera` entity): follows the entity with `targetTag`, with a
  dead zone, smoothing, look-ahead toward where it runs (`lookAhead`) and without leaving
  the tilemaps. When selected, the Scene view shows its dead zone.
- **`ParallaxBackground`**: a tiled image that fills the view and only partly follows the
  camera (`factor`), with optional scrolling of its own (`scroll`).
- **`Text`**: a line of text with a TTF font, in the world or, with `screenSpace`, on the
  game screen (the **HUD**). In the Scene view the HUD is shown over the camera frame and
  can be dragged like any entity. The game only changes the content; position, font and
  color are chosen in the editor.
- **Crisp pixel art at any size**: with the **Pixel perfect** scale (the default in the
  Game tab and in `--play`), one art pixel (`Camera.pixelSize`) always measures a whole
  number of screen pixels; a bit more or less of the world is visible. With **Fit**, the
  game is drawn at its resolution and enlarged with nearest-neighbor to the next integer,
  then smoothly fitted (`GameCanvas`). Scaling directly by a non-integer factor distorts
  pixels (some 3 px wide, others 4) and makes animations look jumpy.

### Audio

- **`SoundEffects`**: a list of rules "play *this file* when *this condition* starts to
  hold", with a volume. It works the same for an event (`jumped`, `damaged`) as for a state
  (`dead`, `collected`): it plays once, when it starts.
- **`Music`**: the scene's background music (file, volume, loop).
- Stopping the game cuts the sound; pausing freezes it. In the Inspector, the **`>`** button
  next to an audio file plays it.
- Formats: WAV, OGG Vorbis and Opus (SDL3_mixer). The sample's sounds and music
  (`assets/audio/`) were generated by synthesis for this project.

### Conditions

Animation and sound rules pick their condition by **name** in the Inspector; each condition
is a C++ function. The engine provides states (`grounded`, `airborne`, `rising`, `falling`,
`moving`, `dead`, `hurt`, `collected`, `activated`, `crumbling`) and one-frame events
(`jumped`, `landed`, `dropped`, `damaged`, `respawned`, `bounced`), and a game adds its own
(the sample adds `goalReached`):

```cpp
app.conditions().add("attacking", [](World& w, Entity e) { /* ... */ });
```

### Sample scenes

**Open...** (toolbar) lists the scenes in `assets/`:

- `platformer.scene.json`: a complete platformer level with Pixel Adventure, built from
  prefabs: player, fruits, checkpoints, spikes, saws (fixed and moving), one-way, moving
  and falling platforms, a trampoline, a goal and a kill zone below the pits; parallax
  background, follow camera (zoom 1.5), health and fruit HUD, sound effects and music.
- `topdown.scene.json`: a Ninja Adventure floor, for a top-down game.
- `shmup.scene.json`: a long Pixel Shmup background, scrolled upward.

| Action | Control |
|---|---|
| Select / drag an entity | Left click (with `Alt`, no grid snapping) |
| Pan the view | Right or middle click and drag |
| Zoom toward the cursor | Mouse wheel |
| Focus on the selection | `F` (or double-click in the Hierarchy) |
| Select several | `Ctrl+click` (add / remove) or `Shift+click` (range, in the Hierarchy) |
| Nest / reorder entities | Drag in the Hierarchy (middle: child; edge: before or after) |
| Group the selection | `Ctrl+G` |
| Play / stop | `Ctrl+P` |
| Save | `Ctrl+S` |
| Undo / redo | `Ctrl+Z` / `Ctrl+Y` or `Ctrl+Shift+Z` |
| Duplicate / delete | `Ctrl+D` / `Del` |
| Tilemap: move / brush / eraser / fill | `V` / `B` / `E` / `G` |
| Tilemap: pick a cell's tile | `Alt+click` |
| Screenshot (to `screenshots/`) | `F9` |

With the `--play` argument, the executable opens the scene and plays it directly, without
the editor (`--scale=pixel|fit|100` picks the scaling).

## Layout

```
engine/
  core/        Entity, World (ECS), Reflection, SceneSerializer, Prefabs, Hierarchy
  components/  engine components (EntityInfo, Transform, Sprite, Tilemap, Camera,
               RigidBody2D, BoxCollider, PlatformerMotor, Health, Hazard, KillZone,
               Collectible, Checkpoint, Respawn, PathMover, FallingPlatform, Bouncer,
               SpriteAnimator, AnimatorStateMachine, FollowCamera, ParallaxBackground,
               Text, SoundEffects, Music)
  systems/     System interface, RenderSystem, PhysicsSystem, PlatformerMotorSystem,
               mechanisms (Motion), rule systems (Rules), animation (Animation),
               FollowCamera and Parallax (Presentation), sound and music (AudioSystems),
               named conditions (Conditions) and Tilemaps queries
  platform/    Input, AssetManager, Screenshot, Audio (SDL3_mixer), GameCanvas
  app/         Application: window, loop, Edit / Play / Paused modes; GameScreen (scaling)
  editor/      the editor (Dear ImGui)
  third_party/ nlohmann/json and Dear ImGui (vendored)
game/          the sample game's code (PlatformerControl: keys -> intent;
               PlatformerRules: fruits, goal, HUD and the goalReached condition)
assets/        sprites, tilesets, audio, prefabs (assets/prefabs/) and scenes (assets/scenes/)
docs/          GUIA_CODIGO_PROPIO.md: how to add your own components and systems
main.cpp       registers the systems in order and opens the scene
CMakeLists.txt the build (with CMakePresets.json); sdl-game-editor.vcxproj, for VS 2026
tools/         get_sdl.ps1: downloads the prebuilt SDL3 packages into deps/ (Windows)
```

## Building and running

**The quickest way (Windows):** clone or download the repo, open
`sdl-game-editor.vcxproj` in Visual Studio 2026 (or the folder, in Visual Studio 2022 /
2026) and press **F5**. There is nothing to install or configure: the first build
downloads SDL into the repo's `deps/` folder.

The project also builds with **CMake** (`CMakeLists.txt` and `CMakePresets.json`), so you
can use the IDE or editor you like: Visual Studio, VS Code, CLion, Qt Creator or just the
command line. It is written for Windows, Linux and macOS (so far it has been tested on
Windows, with MSVC and with MinGW-w64 GCC).

### Requirements

- A **C++17** compiler: MSVC (Visual Studio 2022 or later), GCC / MinGW-w64 or Clang.
- For CMake: **CMake 3.24** or later and **Ninja** (Visual Studio and CLion already include
  both).
- An **Internet connection** the first time, to download SDL (and **Git** outside
  Windows).
- The **SDL3 family**. You do not have to install it, it is downloaded when missing:
  - **On Windows**, the official prebuilt packages (the `-VC` zips from GitHub, a few
    MB) are unzipped into `deps/SDL3`, `deps/SDL3_image`, `deps/SDL3_ttf` and
    `deps/SDL3_mixer` (`deps/` is ignored by git). The `.vcxproj` does it on its first
    build and CMake on its first configure; both use `tools/get_sdl.ps1`, which can also
    be run by hand.
  - **On Linux and macOS** (or if that download fails), CMake downloads the sources from
    GitHub and builds them together with the project (a few minutes, only the first time
    for each build folder).

  | Library | Version | Used for |
  |---|---|---|
  | **SDL3** | **3.4.14** | window, renderer, input, time |
  | **SDL3_image** | **3.4.4** | loading PNG |
  | **SDL3_ttf** | **3.2.2** | text |
  | **SDL3_mixer** | **3.2.4** | audio |

  To use copies you already have instead:
  - The **`SGE_DEPS_DIR` environment variable** (or `-DSGE_DEPS_DIR=<folder>` in CMake)
    points to another folder with the packages unzipped, for example `D:\` with
    `D:\SDL3`, `D:\SDL3_image`... For the `.vcxproj` the folders must have exactly those
    names; CMake also accepts them with the version (`SDL3-3.4.14`...) and the `-mingw`
    packages.
  - With CMake, the ones installed with a package manager (vcpkg, MSYS2, Homebrew, your
    Linux distribution...) are found through `CMAKE_PREFIX_PATH` or the system paths.
  - `-DSGE_FETCH_SDL=OFF` turns CMake's download off (configuring fails if something is
    missing).

  When CMake builds SDL3_image and SDL3_mixer from source, it leaves out the formats that
  need extra libraries: images and WAV, OGG Vorbis, FLAC and MP3 work; Opus, MOD, WavPack
  and the like do not (the sample only uses PNG and WAV). On Linux, building SDL from
  source needs the development packages of your desktop (X11 / Wayland, ALSA / PulseAudio
  / PipeWire...): see SDL's `docs/README-linux.md`.
- **nlohmann/json** and **Dear ImGui** are already in `engine/third_party/`: nothing to
  install.

### With an IDE

- **Visual Studio 2026 with `sdl-game-editor.vcxproj`**: open it, choose **x64** (Debug or
  Release) and press F5. Until the first build has downloaded SDL, IntelliSense marks the
  SDL headers as missing. It does not pick up new files by itself: add them to the
  project.
- **Visual Studio 2022 / 2026 with CMake**: **File > Open > Folder...** and choose the repo.
  Pick the `debug` or `release` configuration in the toolbar, `sdl-game-editor.exe` as the
  startup item, and run with F5. If you prefer a classic solution: `cmake --preset vs2026`
  (or `vs2022`) generates it in `out/build/vs2026/` (`.slnx` in VS 2026, `.sln` in VS
  2022).
- **VS Code**: install the **C/C++** and **CMake Tools** extensions, open the folder, pick a
  configure preset when asked (`debug`, `release`, or `mingw-debug` / `mingw-release` to
  force MinGW's GCC on Windows) and use **Build** and **Run** / **Debug** in the status
  bar.
- **CLion**: open the folder; in **Settings > Build, Execution, Deployment > CMake** enable
  the `debug` / `release` presets.

### From the command line

```
cmake --preset release
cmake --build --preset release
out/build/release/bin/sdl-game-editor
```

Without presets (or without Ninja), the classic form works too:

```
cmake -S . -B out/build/release -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release --config Release
```

The executable and the libraries it needs end up together in `out/build/<preset>/bin/`.
Every `.cpp` in `engine/` and `game/` is compiled: a file you add to `game/` is picked up
on the next build without touching `CMakeLists.txt`.

### Where `assets/` is read and saved

The program looks for the `assets/` folder in the working folder, then next to the
executable and, with the CMake build, in the repo it was built from. So it does not matter
how or from where you start it: **Save writes to the repo's `assets/`**. (The `.vcxproj`
runs it from the project folder, which gives the same result.)

### A folder to share

```
cmake --install out/build/release
```

leaves in `out/install/release/` the executable, its libraries and a copy of `assets/`:
ready to zip or start with a double click (it uses the `assets/` next to it). With a
Visual Studio solution, add `--config Release`; `--prefix <folder>` picks another
destination.

## Credits and third-party licenses

### Libraries

- **[nlohmann/json](https://github.com/nlohmann/json)** by **Niels Lohmann** — JSON for scenes
  and prefabs. **MIT** license, vendored in `engine/third_party/nlohmann/json.hpp`.
- **[Dear ImGui](https://github.com/ocornut/imgui)** by **Omar Cornut** and contributors — the
  editor UI. **MIT** license, vendored (v1.92.9b) in `engine/third_party/imgui/`, with its
  `LICENSE.txt` and a `VENDORED.txt` describing its origin.

### Assets

- **[Pixel Adventure](https://pixelfrog-assets.itch.io/pixel-adventure-1)** by **Pixel Frog**
  (itch.io). Respect its license if you reuse or redistribute the assets.
- **[Ninja Adventure Asset Pack](https://pixel-boy.itch.io/ninja-adventure-asset-pack)** by
  **Pixel-boy and AAA**. **CC0** (public domain; attribution not required but appreciated).
- **[Pixel Shmup](https://kenney.nl/assets/pixel-shmup)** by **Kenney**. **CC0**. See
  `assets/kenney_pixelshmup/License.txt`.
- **Audio** (`assets/audio/`): sound effects and music generated by synthesis for this
  project, with no third-party license.

Used for educational purposes.
