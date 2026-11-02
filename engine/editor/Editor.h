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
struct SDL_Texture;  // declaraciones adelantadas: SDL e ImGui solo en los .cpp
union SDL_Event;

// Editor generalista: edita DIRECTAMENTE el World de la aplicacion (no una copia).
// No sabe nada de ningun juego: todo lo que muestra sale de la reflexion, asi que un
// componente nuevo del juego aparece en el Inspector sin tocar el editor.
//
// Paneles (Dear ImGui, posicion fija):
//   - Barra superior: Jugar/Detener, Pausa, Guardar, Deshacer/Rehacer, grilla.
//   - Jerarquia (izquierda): las entidades; crear, duplicar, borrar.
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
// Se reparte en tres .cpp: Editor.cpp (marco, barra, deshacer, atajos),
// EditorPanels.cpp (Jerarquia e Inspector) y EditorViewport.cpp (vistas).
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

    void beginChange();       // antes de modificar el World
    void commitChange();      // anota el paso pendiente (si lo hay)
    void commitIfIdle();      // ...pero solo si el gesto ya termino
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

    Application& app;
    bool guiReady = false;
    float uiScale = 1.0f;

    Entity selected = NullEntity;

    // Vista del editor (independiente de cualquier Camera de la escena).
    View sceneView;
    bool showGrid = true;
    bool snapToGrid = true;
    int gridSize = 16;
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
};
