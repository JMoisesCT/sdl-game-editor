#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/Entity.h"
#include "core/Math.h"
#include "core/SceneSerializer.h"
#include "systems/RenderSystem.h"

class Application;
class GameCanvas;
struct ComponentInfo;
struct FieldInfo;
struct Tilemap;
struct SDL_Texture;  // forward declarations: SDL and ImGui only in the .cpp files
union SDL_Event;

// General-purpose editor: it edits the application's World DIRECTLY (not a copy).
// It knows nothing about any game: everything it shows comes from the reflection, so a
// new game component shows up in the Inspector without touching the editor.
//
// Panels (Dear ImGui, fixed position):
//   - Top bar: Play/Stop, Pause, Save, Undo/Redo, Open, grid.
//   - Hierarchy (left): the entities as a tree; create, duplicate, delete, and drag
//     one onto another to make it its child or move it (see core/Hierarchy.h).
//     Duplicate and delete take the whole branch, and moving the parent (in the view
//     or in the Inspector) also moves its children. Ctrl+click and Shift+click select
//     several; dragging, Duplicate, Delete and Group (Ctrl+G) act on all of them.
//   - Prefabs (left, below): the .prefab.json files, to place instances.
//   - Inspector (right): the components of the active entity (the last one
//     selected), editable.
//   - View (center): "Scene" tab (the editor's free camera) and "Game" tab (what the
//     player sees, at the game's resolution). Each one is drawn on its own texture.
//
// Undo: each step is a COPY of the serialized scene. Every change goes through
// beginChange() BEFORE modifying the World, and the step is recorded when the gesture
// ends (releasing the mouse or leaving the text field). A change that does not go
// through beginChange cannot be undone and does not mark the scene as modified.
// Nothing is recorded while playing: on Stop, the scene goes back to how it was.
//
// Tilemaps: with an entity with a Tilemap selected, the Inspector shows the tileset
// palette and the tools (Move, Brush, Eraser, Fill); with a painting tool, clicking
// in the scene view paints the cells of THAT layer instead of selecting entities.
// Each stroke (press, drag, release) is one undo step.
//
// Prefabs: the Prefabs panel (below the Hierarchy) lists the .prefab.json files in
// assets/; double-clicking or dragging into the view places an instance. With an
// instance selected, the Inspector shows its prefab, highlights the fields that differ
// from it and offers Apply, Revert and Unlink. Apply saves the prefab right away: undo
// reverts the scene, not the prefab file.
//
// It is split into five .cpp files: Editor.cpp (frame, toolbar, undo, shortcuts),
// EditorPanels.cpp (Hierarchy and Inspector), EditorViewport.cpp (views),
// EditorTilemap.cpp (palette and brushes) and EditorPrefabs.cpp (prefabs).
class Editor {
public:
    explicit Editor(Application& app);
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    bool init();      // ImGui on the application's window and renderer
    void shutdown();  // before destroying the renderer
    void processEvent(const SDL_Event& e);

    // Builds the panels and draws the views onto their textures. Then, with the render
    // target already on the window, present() draws the panels.
    void frame(float dt);
    void present();

    // The application just loaded a scene: selection and history are forgotten.
    void onSceneLoaded();

    // If there are unsaved changes, asks (Save / Discard / Cancel).
    // true = the action can go ahead.
    bool confirmDiscard(const char* action);

    bool isDirty() const { return changePending || version != savedVersion; }

    // Whether text is being typed into a panel field: then the keys belong to the
    // field and not to the game (Application turns off Input's keyboard).
    bool isTypingText() const;

private:
    // --- Editor.cpp ---
    void layoutAndDraw();
    void drawToolbar();
    void handleShortcuts();
    void updateWindowTitle();
    void setStatus(const std::string& text);
    void save();
    void togglePlay();
    void scanSceneFiles();
    // Files under assets/ ending in suffix, with "/" and sorted.
    static std::vector<std::string> listAssetFiles(const std::string& suffix);
    void drawOpenPopup();  // list of scenes; when one is chosen, asks if there are changes

    void beginChange();       // before modifying the World
    void commitChange();      // records the pending step (if any)
    void commitIfIdle();      // ...but only if the gesture has ended
    void cancelChange();      // forgets the pending step (only if nothing was modified yet)
    void undo();
    void redo();
    void restore(const Json& scene);

    // --- EditorPanels.cpp ---
    void drawHierarchy();
    void drawInspector();
    bool drawField(const FieldInfo& field, void* value); // true if it changed
    bool drawList(const FieldInfo& field, void* value);  // a List field (list of structs)
    // Dropdown with those options. true if another one was chosen (left in out).
    bool drawChoice(const char* label, const std::vector<std::string>& options,
                    const std::string& current, std::string& out);
    // Path field + a button that lists the images (or, with audio, the sounds) in
    // assets/; sounds can be played. true if another path was chosen or typed
    // (left in out; the caller decides whether to apply it).
    bool drawFilePicker(const char* label, const std::string& current, std::string& out, bool audio);
    // Entity field: list of the scene's entities, or drop one dragged from the
    // Hierarchy. true if another one was chosen (left in out).
    bool drawEntityPicker(const char* label, Entity current, Entity& out);
    void scanAssetFiles();

    Entity createEntity(Entity parent = NullEntity); // with a parent: its child, at its position
    void duplicateSelected();        // every selected branch
    void deleteSelected();           // every selected branch
    void groupSelected();            // a new empty entity with the selection inside
    void ungroupSelected();          // the selection out of its groups, next to them
    Entity duplicateBranch(Entity root); // the copy goes right after the original
    // Moves these entities (with their branches, in World order) under 'parent', right
    // before 'before' (NullEntity = last). Not an undo step if nothing moves.
    void moveEntities(const std::vector<Entity>& entities, Entity parent, Entity before);

    // Multi-selection: 'selection' holds every selected entity and 'selected' (the
    // active one, the one the Inspector shows) is always among them. Code that sets
    // 'selected' directly selects only that one: syncSelection() notices it (the
    // active one is not in the list) and resets the list.
    void syncSelection();
    bool isSelected(Entity e) const;
    void selectOnly(Entity e);
    void toggleSelected(Entity e);   // Ctrl+click: add it (as the active one) or remove it
    // The selected entities none of whose ancestors is selected, in World order: moving,
    // duplicating or deleting them takes the whole selection exactly once.
    std::vector<Entity> selectedRoots();
    std::string entityLabel(Entity e);
    // Moves e's whole branch (not e) by 'delta': what is done when moving the parent.
    // HUD texts are in screen px: they only move with a HUD parent.
    void moveBranch(Entity e, Vec2 delta);

    // --- EditorViewport.cpp ---
    void drawViewport();
    void drawSceneView(Vec2 pos, Vec2 size);
    void drawGameView(Vec2 pos, Vec2 size);
    void handleSceneInput(Vec2 imagePos, bool hovered);
    void drawGizmos();
    bool entityBounds(Entity e, Vec2& center, Vec2& size); // in the world
    // HUD text (Text with screenSpace): its position is in game screen px.
    bool isHudEntity(Entity e);
    // Maps game screen px to the Scene view (through the game's camera): the HUD is
    // shown over the camera frame.
    View hudView();
    Entity pickAt(Vec2 world);
    void frameEntity(Entity e);
    void centerOnCamera();
    SDL_Texture* ensureTarget(SDL_Texture*& tex, int& curW, int& curH, int w, int h);

    // --- EditorTilemap.cpp ---
    enum class TileTool { Move, Brush, Erase, Fill };
    Tilemap* selectedTilemap();      // the selection's Tilemap (if it also has a Transform)
    void setTileTool(TileTool tool);
    void drawTilePalette();          // in the Inspector, below the components
    // With a tile tool, handles the left click in the scene view.
    // true = the mouse was already used (do not select or drag entities).
    bool handleTilemapInput(bool hovered);
    void paintCell(Tilemap& map, int col, int row);        // applies Brush or Eraser
    void setCell(Tilemap& map, int col, int row, int tile); // beginChange if it changes
    void fillFrom(Tilemap& map, int col, int row);
    void drawTilemapGizmos();        // inside the scene view's target

    // --- EditorPrefabs.cpp ---
    void drawPrefabList();           // Prefabs panel
    void scanPrefabFiles();
    void reloadPrefabs();            // reloads the prefabs and rebuilds the instances
    Entity placePrefab(const std::string& path, Vec2 position); // new instance, selected
    void drawPrefabHeader();         // in the Inspector: its prefab, or "Create prefab"
    void createPrefabFromSelected(const std::string& name);
    void applyToPrefab();            // the selection becomes the prefab
    void revertToPrefab();           // the selection matches the prefab again
    void unlinkPrefab();
    // Removes every component from the entity and rebuilds it from that JSON
    // (serializeEntity's shape), keeping its id.
    void rebuildEntity(Entity e, const Json& entity);

    Application& app;
    bool guiReady = false;
    float uiScale = 1.0f;

    Entity selected = NullEntity;          // the active entity (see syncSelection)
    std::vector<Entity> selection;         // every selected entity, 'selected' included
    Entity selectionAnchor = NullEntity;   // where a Shift+click range in the Hierarchy starts
    Entity revealedSelection = NullEntity; // the Hierarchy already opened this one's branch and showed it
    Entity hierarchyPress = NullEntity;    // this one was pressed in the Hierarchy (selected on release)
    std::string entityFilter;              // filter text of the entity picker

    // Editor view (independent of any Camera in the scene).
    View sceneView;
    bool showGrid = true;
    bool snapToGrid = true;
    int gridSize = 16;
    bool showColliders = true;     // draw the BoxCollider boxes
    bool wantGameTab = false;      // select the "Game" tab (when pressing Play)
    bool wantSceneTab = false;     // ...or "Scene" (when stopping)

    // Dragging an entity in the scene view.
    bool pressOnEntity = false;    // an entity was pressed (not moved yet)
    bool pressKeepsSelection = false; // pressed on one of several selected: on release
                                      // without dragging, it becomes the only one
    bool dragging = false;         // already moved past the threshold
    Vec2 pressMouse;               // where it was pressed (screen)
    Vec2 dragOffset;               // mouse - entity position (world)
    bool panning = false;
    Vec2 mouseWorld;               // mouse in the world (for the bar)
    bool mouseInScene = false;

    // Render textures of the two views.
    SDL_Texture* sceneTarget = nullptr;
    int sceneTargetW = 0, sceneTargetH = 0;
    std::unique_ptr<GameCanvas> gameCanvas; // the game view: canvas + sharp scaling

    // Undo / redo: copies of the scene with their version number.
    struct Snapshot { Json scene; int version = 0; };
    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    Json pendingBefore;            // the scene before the change in progress
    bool changePending = false;
    int version = 0;               // version of what is in the World now
    int nextVersion = 1;
    int savedVersion = 0;          // version that is on disk

    std::string status;            // status bar message
    float statusTime = 0.0f;       // seconds it has left
    std::string windowTitle;

    std::vector<std::string> textureFiles; // .png under assets/, to pick textures
    std::vector<std::string> audioFiles;   // .wav, .ogg... under assets/, to pick sounds
    bool assetFilesScanned = false;
    std::string fileFilter;                // filter text of those lists

    // Tilemaps (editor state: not saved in the scene).
    TileTool tileTool = TileTool::Move;
    IntRect brush{ 0, 0, 1, 1 };   // tiles picked in the palette, in tileset columns/rows
    bool paletteSelecting = false; // dragging in the palette to pick several tiles
    int paletteStartCol = 0, paletteStartRow = 0;
    int paletteZoom = 2;
    bool markSolids = false;       // click in the palette: mark/unmark as solid
    bool showSolids = false;       // scene view: highlight the blocking cells
    bool painting = false;         // stroke in progress
    int lastCol = 0, lastRow = 0;  // last cell painted in the stroke
    bool hoverCell = false;        // the mouse is over the view with a tile tool
    int hoverCol = 0, hoverRow = 0;

    std::vector<std::string> sceneFiles; // .scene.json under assets/, for "Open..."

    std::vector<std::string> prefabFiles; // .prefab.json under assets/, for the Prefabs panel
    bool prefabFilesScanned = false;
    std::string newPrefabName;            // text of the "Create prefab" box
};
