#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <filesystem>

#include "app/Application.h"

// Pasos de deshacer que se guardan como maximo (cada uno es una copia de la escena).
static const std::size_t MAX_UNDO = 200;

Editor::Editor(Application& app) : app(app) {}

Editor::~Editor() {
    shutdown();
}

// --- Inicio y fin ----------------------------------------------------------------------

bool Editor::init() {
    if (guiReady) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Sin imgui.ini: los paneles siempre arrancan en su sitio y no se ensucia el repo.
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // La ventana no se escala con la pantalla: con Windows al 150%, 13 px de texto se
    // ven diminutos. Los paneles se agrandan segun la escala de la pantalla.
    uiScale = SDL_GetWindowDisplayScale(app.window());
    if (uiScale < 1.0f) uiScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(uiScale);
    style.FontScaleDpi = uiScale;
    style.WindowRounding = 0.0f;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(app.window(), app.renderer())) {
        SDL_Log("Editor: no se pudo iniciar ImGui (SDL3).");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(app.renderer())) {
        SDL_Log("Editor: no se pudo iniciar ImGui (renderer).");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    guiReady = true;
    return true;
}

void Editor::shutdown() {
    if (sceneTarget) { SDL_DestroyTexture(sceneTarget); sceneTarget = nullptr; }
    if (gameTarget) { SDL_DestroyTexture(gameTarget); gameTarget = nullptr; }
    if (!guiReady) return;
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    guiReady = false;
}

void Editor::processEvent(const SDL_Event& e) {
    if (guiReady) ImGui_ImplSDL3_ProcessEvent(&e);
}

void Editor::onSceneLoaded() {
    selected = NullEntity;
    undoStack.clear();
    redoStack.clear();
    pendingBefore = Json();
    changePending = false;
    version = savedVersion = 0;
    nextVersion = 1;
    dragging = pressOnEntity = panning = painting = false;
    tileTool = TileTool::Move;
    sceneView.zoom = 1.0f;
    centerOnCamera();
}

// --- Frame -----------------------------------------------------------------------------

void Editor::frame(float dt) {
    if (!guiReady) return;
    if (statusTime > 0.0f) statusTime -= dt;

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Las herramientas de tiles pintan en la capa elegida: si se eligio otra cosa
    // (o se borro), se vuelve a Mover.
    if (tileTool != TileTool::Move && !selectedTilemap()) {
        tileTool = TileTool::Move;
        painting = false;
    }

    handleShortcuts();
    layoutAndDraw();
    commitIfIdle();
    updateWindowTitle();

    ImGui::Render();
}

void Editor::present() {
    if (!guiReady) return;
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app.renderer());
}

// Disposicion fija, como un editor con paneles acoplados: barra arriba, Jerarquia a
// la izquierda, Inspector a la derecha y la vista en el centro. Se recalcula en cada
// frame, asi sigue a la ventana cuando cambia de tamano.
void Editor::layoutAndDraw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 origin = vp->WorkPos;
    const ImVec2 full = vp->WorkSize;
    const ImGuiStyle& style = ImGui::GetStyle();

    float toolbarH = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
    float leftW = 260.0f * uiScale;
    float rightW = 380.0f * uiScale;
    float bodyH = full.y - toolbarH;
    float centerW = full.x - leftW - rightW;
    if (centerW < 100.0f) centerW = 100.0f;

    const ImGuiWindowFlags fixed = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(ImVec2(full.x, toolbarH));
    if (ImGui::Begin("##toolbar", nullptr, fixed | ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoScrollbar))
        drawToolbar();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(leftW, bodyH));
    if (ImGui::Begin("Jerarquia", nullptr, fixed)) drawHierarchy();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x + leftW + centerW, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(rightW, bodyH));
    if (ImGui::Begin("Inspector", nullptr, fixed)) drawInspector();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x + leftW, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(centerW, bodyH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("Vista", nullptr, fixed | ImGuiWindowFlags_NoTitleBar |
                                                   ImGuiWindowFlags_NoScrollbar |
                                                   ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open) drawViewport();
    ImGui::End();
}

void Editor::drawToolbar() {
    const bool editing = app.isEditing();
    const bool paused = app.mode() == PlayMode::Paused;

    // El texto del boton cambia: el id va fijo con ### (si no, ImGui lo trata como otro).
    if (!editing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
    if (ImGui::Button(editing ? "Jugar###play" : "Detener###play")) togglePlay();
    if (!editing) ImGui::PopStyleColor();
    ImGui::SetItemTooltip("Ctrl+P");

    ImGui::SameLine();
    ImGui::BeginDisabled(editing);
    if (ImGui::Button(paused ? "Seguir###pause" : "Pausa###pause")) app.setPaused(!paused);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Abrir...")) {
        scanSceneFiles();
        ImGui::OpenPopup("##open");
    }
    ImGui::SetItemTooltip("Abre otra escena de assets/");
    ImGui::SameLine();
    if (ImGui::Button("Guardar")) save();
    ImGui::SetItemTooltip("Ctrl+S");
    ImGui::SameLine();
    ImGui::BeginDisabled(undoStack.empty() && !changePending);
    if (ImGui::Button("Deshacer")) undo();
    ImGui::SetItemTooltip("Ctrl+Z");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack.empty());
    if (ImGui::Button("Rehacer")) redo();
    ImGui::SetItemTooltip("Ctrl+Y / Ctrl+Shift+Z");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    drawOpenPopup(); // fuera del bloque deshabilitado: si no, su lista saldria gris

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Grilla", &showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Ajustar", &snapToGrid);
    ImGui::SetItemTooltip("Al arrastrar, la posicion cae en la grilla");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f * uiScale);
    if (ImGui::InputInt("px##grid", &gridSize)) {
        if (gridSize < 1) gridSize = 1;
        if (gridSize > 512) gridSize = 512;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted(app.scenePath().empty() ? "(sin escena)" : app.scenePath().c_str());
    if (isDirty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "SIN GUARDAR");
    }
    if (!editing) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f),
                           "Jugando: lo que cambie se pierde al detener");
    }
    if (statusTime > 0.0f) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextUnformatted(status.c_str());
    }
}

void Editor::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return; // escribiendo en un campo: las teclas son del campo

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) togglePlay();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) save();

    if (app.isEditing()) {
        if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z)) undo();
        if (ctrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (shift && ImGui::IsKeyPressed(ImGuiKey_Z))))
            redo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelected();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) frameEntity(selected);

    // Herramientas de tiles (pintar necesita una entidad con Tilemap elegida). Solo
    // editando: jugando, esas letras son del juego.
    if (app.isEditing() && !ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) setTileTool(TileTool::Move);
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) setTileTool(TileTool::Brush);
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) setTileTool(TileTool::Erase);
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) setTileTool(TileTool::Fill);
    }
}

void Editor::updateWindowTitle() {
    std::string title = app.config().title + " - " +
                        (app.scenePath().empty() ? std::string("(sin escena)") : app.scenePath());
    if (isDirty()) title += " *";
    if (!app.isEditing()) title += "  [jugando]";
    if (title != windowTitle) {
        windowTitle = title;
        SDL_SetWindowTitle(app.window(), title.c_str());
    }
}

void Editor::setStatus(const std::string& text) {
    status = text;
    statusTime = 4.0f;
    SDL_Log("Editor: %s", text.c_str());
}

void Editor::save() {
    if (!app.isEditing()) {
        setStatus("Deten el juego para guardar");
        return;
    }
    commitChange();
    if (app.saveScene()) {
        savedVersion = version;
        setStatus("Guardado: " + app.scenePath());
    } else {
        setStatus("No se pudo guardar (ver el log)");
    }
}

void Editor::scanSceneFiles() {
    namespace fs = std::filesystem;
    static const std::string EXT = ".scene.json";
    sceneFiles.clear();
    std::error_code ec;
    for (fs::recursive_directory_iterator it("assets", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string file = it->path().generic_string(); // con "/" tambien en Windows
        if (file.size() > EXT.size() && file.compare(file.size() - EXT.size(), EXT.size(), EXT) == 0)
            sceneFiles.push_back(file);
    }
    std::sort(sceneFiles.begin(), sceneFiles.end());
}

void Editor::drawOpenPopup() {
    if (!ImGui::BeginPopup("##open")) return;
    std::string chosen;
    if (sceneFiles.empty()) ImGui::TextDisabled("No hay escenas (.scene.json) en assets/");
    for (const std::string& file : sceneFiles)
        if (ImGui::Selectable(file.c_str(), file == app.scenePath())) chosen = file;
    if (!chosen.empty()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();

    // Ya fuera del popup: confirmDiscard puede abrir un cuadro nativo que bloquea.
    if (!chosen.empty() && confirmDiscard("abrir otra escena")) {
        if (app.openScene(chosen)) setStatus("Abierta: " + chosen);
        else setStatus("No se pudo abrir " + chosen + " (ver el log)");
    }
}

void Editor::togglePlay() {
    dragging = pressOnEntity = panning = painting = false;
    if (app.isEditing()) {
        commitChange(); // la copia de Play debe incluir lo ultimo editado
        app.play();
        wantGameTab = true;
    } else {
        app.stop();
        wantSceneTab = true;
        if (!app.world().isAlive(selected)) selected = NullEntity;
    }
}

// --- Deshacer / rehacer ----------------------------------------------------------------

void Editor::beginChange() {
    if (!app.isEditing() || changePending) return;
    pendingBefore = serializeWorld(app.world(), app.registry());
    changePending = true;
}

void Editor::commitChange() {
    if (!changePending) return;
    undoStack.push_back({ std::move(pendingBefore), version });
    if (undoStack.size() > MAX_UNDO) undoStack.erase(undoStack.begin());
    pendingBefore = Json();
    changePending = false;
    version = nextVersion++;
    redoStack.clear();
}

void Editor::commitIfIdle() {
    // El gesto termina al soltar el raton o al salir del campo que se editaba: asi
    // arrastrar un valor, escribir un nombre o un trazo del pincel es UN paso, no uno
    // por frame.
    if (changePending && !dragging && !painting && !ImGui::IsAnyItemActive()) commitChange();
}

void Editor::undo() {
    if (!app.isEditing()) return;
    commitChange();
    if (undoStack.empty()) return;
    redoStack.push_back({ serializeWorld(app.world(), app.registry()), version });
    Snapshot s = std::move(undoStack.back());
    undoStack.pop_back();
    restore(s.scene);
    version = s.version;
    setStatus("Deshacer");
}

void Editor::redo() {
    if (!app.isEditing() || redoStack.empty()) return;
    commitChange();
    undoStack.push_back({ serializeWorld(app.world(), app.registry()), version });
    Snapshot s = std::move(redoStack.back());
    redoStack.pop_back();
    restore(s.scene);
    version = s.version;
    setStatus("Rehacer");
}

void Editor::restore(const Json& scene) {
    std::string error;
    if (!deserializeWorld(app.world(), app.registry(), scene, error))
        SDL_Log("Editor: no se pudo restaurar la escena: %s", error.c_str());
    if (!app.world().isAlive(selected)) selected = NullEntity;
    dragging = pressOnEntity = painting = false;
}

// --- Cambios sin guardar ---------------------------------------------------------------

bool Editor::confirmDiscard(const char* action) {
    if (!isDirty()) return true;

    // Cuadro de dialogo nativo: bloquea hasta que se responde y no depende de ImGui.
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Guardar" },
        { 0,                                       2, "Descartar" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancelar" },
    };
    std::string question = "La escena tiene cambios sin guardar:\n" + app.scenePath() +
                           "\n\nGuardarlos antes de " + action + "?";
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    box.window = app.window();
    box.title = "Cambios sin guardar";
    box.message = question.c_str();
    box.numbuttons = 3;
    box.buttons = buttons;

    int choice = 0;
    if (!SDL_ShowMessageBox(&box, &choice)) {
        SDL_Log("Editor: no se pudo preguntar (%s); se descartan los cambios.", SDL_GetError());
        return true;
    }
    if (choice == 1) {
        if (!app.isEditing()) togglePlay(); // se guarda la escena, no el juego en curso
        save();
        return !isDirty(); // si guardar falla, se cancela
    }
    return choice == 2;
}
