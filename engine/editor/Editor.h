#pragma once
#include <string>
#include <vector>

#include "core/Entity.h"
#include "core/Math.h"
#include "core/SceneSerializer.h"
#include "systems/RenderSystem.h"

class Application;
struct ComponentInfo;
struct FieldInfo;
struct Tilemap;
struct SDL_Texture;  // declaraciones adelantadas: SDL e ImGui solo en los .cpp
union SDL_Event;

// Editor generalista: edita DIRECTAMENTE el World de la aplicacion (no una copia).
// No sabe nada de ningun juego: todo lo que muestra sale de la reflexion, asi que un
// componente nuevo del juego aparece en el Inspector sin tocar el editor.
//
// Paneles (Dear ImGui, posicion fija):
//   - Barra superior: Jugar/Detener, Pausa, Guardar, Deshacer/Rehacer, Abrir, grilla.
//   - Jerarquia (izquierda): las entidades; crear, duplicar, borrar.
//   - Prefabs (izquierda, debajo): los .prefab.json, para colocar instancias.
//   - Inspector (derecha): los componentes de la seleccionada, editables.
//   - Vista (centro): pestanas "Escena" (camara libre del editor) y "Juego" (lo que
//     ve el jugador, a la resolucion del juego). Cada una se dibuja en su textura.
//
// Deshacer: cada paso es una COPIA de la escena serializada. Todo cambio pasa por
// beginChange() ANTES de modificar el World, y el paso se anota al terminar el gesto
// (soltar el raton o salir del campo de texto). Un cambio que no pase por
// beginChange no se puede deshacer y no marca la escena como modificada.
// Jugando no se anota nada: al detener, la escena vuelve a como estaba.
//
// Tilemaps: con una entidad con Tilemap elegida, el Inspector muestra la paleta del
// tileset y las herramientas (Mover, Pincel, Goma, Relleno); con una de pintar, el
// clic en la vista de escena pinta las celdas de ESA capa en vez de elegir entidades.
// Cada trazo (pulsar, arrastrar, soltar) es un paso de deshacer.
//
// Prefabs: el panel Prefabs (bajo la Jerarquia) lista los .prefab.json de assets/;
// doble clic o arrastrar a la vista coloca una instancia. Con una instancia elegida,
// el Inspector muestra su prefab, resalta los campos que difieren de el y ofrece
// Aplicar, Revertir y Desvincular. Aplicar guarda el prefab en el acto: deshacer
// vuelve atras la escena, no el archivo del prefab.
//
// Se reparte en cinco .cpp: Editor.cpp (marco, barra, deshacer, atajos),
// EditorPanels.cpp (Jerarquia e Inspector), EditorViewport.cpp (vistas),
// EditorTilemap.cpp (paleta y pinceles) y EditorPrefabs.cpp (prefabs).
class Editor {
public:
    explicit Editor(Application& app);
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    bool init();      // ImGui sobre la ventana y el renderer de la aplicacion
    void shutdown();  // antes de destruir el renderer
    void processEvent(const SDL_Event& e);

    // Arma los paneles y dibuja las vistas en sus texturas. Despues, con el destino
    // de render ya en la ventana, present() dibuja los paneles.
    void frame(float dt);
    void present();

    // La aplicacion acaba de cargar una escena: se olvidan seleccion e historial.
    void onSceneLoaded();

    // Si hay cambios sin guardar, pregunta (Guardar / Descartar / Cancelar).
    // true = se puede seguir con la accion.
    bool confirmDiscard(const char* action);

    bool isDirty() const { return changePending || version != savedVersion; }

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
    // Archivos bajo assets/ que terminan en suffix, con "/" y ordenados.
    static std::vector<std::string> listAssetFiles(const std::string& suffix);
    void drawOpenPopup();  // lista de escenas; al elegir una, pregunta si hay cambios

    void beginChange();       // antes de modificar el World
    void commitChange();      // anota el paso pendiente (si lo hay)
    void commitIfIdle();      // ...pero solo si el gesto ya termino
    void cancelChange();      // olvida el paso pendiente (solo si aun no se modifico nada)
    void undo();
    void redo();
    void restore(const Json& scene);

    // --- EditorPanels.cpp ---
    void drawHierarchy();
    void drawInspector();
    bool drawField(const FieldInfo& field, void* value); // true si cambio
    // Campo de ruta + boton que lista las imagenes de assets/. true si se eligio o
    // escribio otra ruta (queda en out; quien llama decide si la aplica).
    bool drawTexturePicker(const char* label, const std::string& current, std::string& out);
    void scanTextureFiles();

    Entity createEntity();
    void duplicateSelected();
    void deleteSelected();
    std::string entityLabel(Entity e);

    // --- EditorViewport.cpp ---
    void drawViewport();
    void drawSceneView(Vec2 pos, Vec2 size);
    void drawGameView(Vec2 pos, Vec2 size);
    void handleSceneInput(Vec2 imagePos, bool hovered);
    void drawGizmos();
    bool entityBounds(Entity e, Vec2& center, Vec2& size); // en el mundo
    Entity pickAt(Vec2 world);
    void frameEntity(Entity e);
    void centerOnCamera();
    SDL_Texture* ensureTarget(SDL_Texture*& tex, int& curW, int& curH, int w, int h);

    // --- EditorTilemap.cpp ---
    enum class TileTool { Move, Brush, Erase, Fill };
    Tilemap* selectedTilemap();      // el Tilemap de la seleccion (si tambien tiene Transform)
    void setTileTool(TileTool tool);
    void drawTilePalette();          // en el Inspector, bajo los componentes
    // Con una herramienta de tiles, maneja el clic izquierdo en la vista de escena.
    // true = el raton ya se uso (no elegir ni arrastrar entidades).
    bool handleTilemapInput(bool hovered);
    void paintCell(Tilemap& map, int col, int row);        // aplica Pincel o Goma
    void setCell(Tilemap& map, int col, int row, int tile); // beginChange si cambia
    void fillFrom(Tilemap& map, int col, int row);
    void drawTilemapGizmos();        // dentro del destino de la vista de escena

    // --- EditorPrefabs.cpp ---
    void drawPrefabList();           // panel Prefabs
    void scanPrefabFiles();
    void reloadPrefabs();            // vuelve a leer los prefabs y rearma las instancias
    Entity placePrefab(const std::string& path, Vec2 position); // instancia nueva, elegida
    void drawPrefabHeader();         // en el Inspector: su prefab, o "Crear prefab"
    void createPrefabFromSelected(const std::string& name);
    void applyToPrefab();            // la seleccion pasa a ser el prefab
    void revertToPrefab();           // la seleccion vuelve a ser igual al prefab
    void unlinkPrefab();
    // Quita todos los componentes de la entidad y la vuelve a armar desde ese JSON
    // (forma de serializeEntity), conservando su id.
    void rebuildEntity(Entity e, const Json& entity);

    Application& app;
    bool guiReady = false;
    float uiScale = 1.0f;

    Entity selected = NullEntity;

    // Vista del editor (independiente de cualquier Camera de la escena).
    View sceneView;
    bool showGrid = true;
    bool snapToGrid = true;
    int gridSize = 16;
    bool showColliders = true;     // dibujar las cajas de BoxCollider
    bool wantGameTab = false;      // seleccionar la pestana "Juego" (al pulsar Jugar)
    bool wantSceneTab = false;     // ...o "Escena" (al detener)

    // Arrastre de una entidad en la vista de escena.
    bool pressOnEntity = false;    // se pulso sobre una entidad (aun sin moverla)
    bool dragging = false;         // ya se movio mas alla del umbral
    Vec2 pressMouse;               // donde se pulso (pantalla)
    Vec2 dragOffset;               // raton - posicion de la entidad (mundo)
    bool panning = false;
    Vec2 mouseWorld;               // raton en el mundo (para la barra)
    bool mouseInScene = false;

    // Texturas de render de las dos vistas.
    SDL_Texture* sceneTarget = nullptr;
    int sceneTargetW = 0, sceneTargetH = 0;
    SDL_Texture* gameTarget = nullptr;
    int gameTargetW = 0, gameTargetH = 0;

    // Deshacer / rehacer: copias de la escena con su numero de version.
    struct Snapshot { Json scene; int version = 0; };
    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    Json pendingBefore;            // la escena antes del cambio en curso
    bool changePending = false;
    int version = 0;               // version de lo que hay ahora en el World
    int nextVersion = 1;
    int savedVersion = 0;          // version que esta en disco

    std::string status;            // mensaje de la barra
    float statusTime = 0.0f;       // segundos que le quedan
    std::string windowTitle;

    std::vector<std::string> textureFiles; // .png bajo assets/, para elegir texturas
    bool textureFilesScanned = false;
    std::string textureFilter;             // texto del filtro de esa lista

    // Tilemaps (estado del editor: no se guarda en la escena).
    TileTool tileTool = TileTool::Move;
    IntRect brush{ 0, 0, 1, 1 };   // tiles elegidos en la paleta, en columnas/filas del tileset
    bool paletteSelecting = false; // arrastrando en la paleta para elegir varios tiles
    int paletteStartCol = 0, paletteStartRow = 0;
    int paletteZoom = 2;
    bool markSolids = false;       // clic en la paleta: marcar/desmarcar como solido
    bool showSolids = false;       // vista de escena: resaltar las celdas que frenan
    bool painting = false;         // trazo en curso
    int lastCol = 0, lastRow = 0;  // ultima celda pintada del trazo
    bool hoverCell = false;        // el raton esta sobre la vista con una herramienta de tiles
    int hoverCol = 0, hoverRow = 0;

    std::vector<std::string> sceneFiles; // .scene.json bajo assets/, para "Abrir..."

    std::vector<std::string> prefabFiles; // .prefab.json bajo assets/, para el panel Prefabs
    bool prefabFilesScanned = false;
    std::string newPrefabName;            // texto del cuadro "Crear prefab"
};
