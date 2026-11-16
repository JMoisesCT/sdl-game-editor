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
```

## Building and running

A single **Visual Studio 2026** project (`sdl-game-editor.vcxproj`, no `.sln`).

### Requirements

- **Visual Studio 2026** with the C++ toolset (`v145`), C++17, **x64** platform.
- The **SDL3 family installed by hand** (not from vcpkg), in these versions and paths:

  | Library | Version | Expected path | Used for |
  |---|---|---|---|
  | **SDL3** | **3.4.14** | `D:\SDL3` | window, renderer, input, time |
  | **SDL3_image** | **3.4.4** | `D:\SDL3_image` | loading PNG |
  | **SDL3_ttf** | **3.2.2** | `D:\SDL3_ttf` | text |
  | **SDL3_mixer** | **3.2.4** | `D:\SDL3_mixer` | audio |

  Unzip each one (the VC development package) into its folder in `D:\`, so that
  `<folder>\include` and `<folder>\lib\x64` exist. If you use other paths, adjust
  `AdditionalIncludeDirectories`, `AdditionalLibraryDirectories` and the `PostBuildEvent`
  in the `.vcxproj`.
- **nlohmann/json** and **Dear ImGui** are already in `engine/third_party/`: nothing to
  install.

### Steps

1. Open `sdl-game-editor.vcxproj` in Visual Studio 2026.
2. Choose **x64** (**Debug** and **Release** both work; Win32 is not supported).
3. Build and run (F5). The post-build step copies the SDL3 DLLs and the `assets/` folder
   next to the executable.

From Visual Studio the working directory is the project folder, so **Save** writes to the
repo's `assets/`. If you run the `.exe` from another folder, it saves to the copy of
`assets/` next to it.

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
