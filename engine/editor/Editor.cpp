#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "app/Application.h"
#include "platform/GameCanvas.h"

// Maximum number of undo steps kept (each one is a copy of the scene).
static const std::size_t MAX_UNDO = 200;

Editor::Editor(Application& app) : app(app) {}

Editor::~Editor() {
    shutdown();
}

// --- Startup and shutdown --------------------------------------------------------------

bool Editor::init() {
    if (guiReady) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // No imgui.ini: the panels always start in place and the repo stays clean.
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // The window does not scale with the display: with Windows at 150%, 13 px text
    // looks tiny. The panels grow according to the display scale.
    uiScale = SDL_GetWindowDisplayScale(app.window());
    if (uiScale < 1.0f) uiScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(uiScale);
    style.FontScaleDpi = uiScale;
    style.WindowRounding = 0.0f;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(app.window(), app.renderer())) {
        SDL_Log("Editor: could not start ImGui (SDL3).");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(app.renderer())) {
        SDL_Log("Editor: could not start ImGui (renderer).");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    guiReady = true;
    return true;
}

void Editor::shutdown() {
    if (sceneTarget) { SDL_DestroyTexture(sceneTarget); sceneTarget = nullptr; }
    gameCanvas.reset();
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
    selected = selectionAnchor = NullEntity;
    selection.clear();
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
    syncSelection();

    // The tile tools paint on the selected layer: if something else was selected
    // (or it was deleted), go back to Move.
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

// Fixed layout, like an editor with docked panels: bar on top, Hierarchy on the
// left, Inspector on the right and the view in the center. Recomputed every
// frame, so it follows the window when it is resized.
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

    // Left column: Hierarchy on top and Prefabs below.
    float hierarchyH = std::round(bodyH * 0.62f);
    ImGui::SetNextWindowPos(ImVec2(origin.x, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(leftW, hierarchyH));
    if (ImGui::Begin("Hierarchy", nullptr, fixed)) drawHierarchy();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x, origin.y + toolbarH + hierarchyH));
    ImGui::SetNextWindowSize(ImVec2(leftW, bodyH - hierarchyH));
    if (ImGui::Begin("Prefabs", nullptr, fixed)) drawPrefabList();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x + leftW + centerW, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(rightW, bodyH));
    if (ImGui::Begin("Inspector", nullptr, fixed)) drawInspector();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(origin.x + leftW, origin.y + toolbarH));
    ImGui::SetNextWindowSize(ImVec2(centerW, bodyH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("View", nullptr, fixed | ImGuiWindowFlags_NoTitleBar |
                                                   ImGuiWindowFlags_NoScrollbar |
                                                   ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (open) drawViewport();
    ImGui::End();
}

void Editor::drawToolbar() {
    const bool editing = app.isEditing();
    const bool paused = app.mode() == PlayMode::Paused;

    // The button text changes: the id is fixed with ### (otherwise ImGui treats it as another one).
    if (!editing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
    if (ImGui::Button(editing ? "Play###play" : "Stop###play")) togglePlay();
    if (!editing) ImGui::PopStyleColor();
    ImGui::SetItemTooltip("Ctrl+P");

    ImGui::SameLine();
    ImGui::BeginDisabled(editing);
    if (ImGui::Button(paused ? "Resume###pause" : "Pause###pause")) app.setPaused(!paused);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Open...")) {
        scanSceneFiles();
        ImGui::OpenPopup("##open");
    }
    ImGui::SetItemTooltip("Open another scene from assets/");
    ImGui::SameLine();
    if (ImGui::Button("Save")) save();
    ImGui::SetItemTooltip("Ctrl+S");
    ImGui::SameLine();
    ImGui::BeginDisabled(undoStack.empty() && !changePending);
    if (ImGui::Button("Undo")) undo();
    ImGui::SetItemTooltip("Ctrl+Z");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack.empty());
    if (ImGui::Button("Redo")) redo();
    ImGui::SetItemTooltip("Ctrl+Y / Ctrl+Shift+Z");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    drawOpenPopup(); // outside the disabled block: otherwise its list would be grayed out

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snapToGrid);
    ImGui::SetItemTooltip("While dragging, the position snaps to the grid");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f * uiScale);
    if (ImGui::InputInt("px##grid", &gridSize)) {
        if (gridSize < 1) gridSize = 1;
        if (gridSize > 512) gridSize = 512;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Colliders", &showColliders);
    ImGui::SetItemTooltip("Draws the BoxCollider boxes in the Scene view (green: solid, yellow: trigger)");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted(app.scenePath().empty() ? "(no scene)" : app.scenePath().c_str());
    if (isDirty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "UNSAVED");
    }
    if (!editing) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f),
                           "Playing: changes are lost on Stop");
    }
    if (statusTime > 0.0f) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextUnformatted(status.c_str());
    }
}

bool Editor::isTypingText() const {
    return ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
}

void Editor::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return; // typing in a field: the keys belong to the field

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
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) groupSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelected();
    if (!ctrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) frameEntity(selected);

    // Tile tools (painting needs a selected entity with a Tilemap). Only while
    // editing: while playing, those letters belong to the game.
    if (app.isEditing() && !ctrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) setTileTool(TileTool::Move);
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) setTileTool(TileTool::Brush);
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) setTileTool(TileTool::Erase);
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) setTileTool(TileTool::Fill);
    }
}

void Editor::updateWindowTitle() {
    std::string title = app.config().title + " - " +
                        (app.scenePath().empty() ? std::string("(no scene)") : app.scenePath());
    if (isDirty()) title += " *";
    if (!app.isEditing()) title += "  [playing]";
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
        setStatus("Stop the game to save");
        return;
    }
    commitChange();
    if (app.saveScene()) {
        savedVersion = version;
        setStatus("Saved: " + app.scenePath());
    } else {
        setStatus("Could not save (see the log)");
    }
}

std::vector<std::string> Editor::listAssetFiles(const std::string& suffix) {
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator it("assets", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string file = it->path().generic_string(); // with "/" on Windows too
        if (file.size() > suffix.size() &&
            file.compare(file.size() - suffix.size(), suffix.size(), suffix) == 0)
            files.push_back(file);
    }
    std::sort(files.begin(), files.end());
    return files;
}

void Editor::scanSceneFiles() {
    sceneFiles = listAssetFiles(".scene.json");
}

void Editor::drawOpenPopup() {
    if (!ImGui::BeginPopup("##open")) return;
    std::string chosen;
    if (sceneFiles.empty()) ImGui::TextDisabled("No scenes (.scene.json) in assets/");
    for (const std::string& file : sceneFiles)
        if (ImGui::Selectable(file.c_str(), file == app.scenePath())) chosen = file;
    if (!chosen.empty()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();

    // Already outside the popup: confirmDiscard may open a blocking native dialog.
    if (!chosen.empty() && confirmDiscard("opening another scene")) {
        if (app.openScene(chosen)) setStatus("Opened: " + chosen);
        else setStatus("Could not open " + chosen + " (see the log)");
    }
}

void Editor::togglePlay() {
    dragging = pressOnEntity = panning = painting = false;
    if (app.isEditing()) {
        commitChange(); // the Play copy must include the latest edit
        app.play();
        wantGameTab = true;
    } else {
        app.stop();
        wantSceneTab = true;
        if (!app.world().isAlive(selected)) selected = NullEntity;
    }
}

// --- Undo / redo -----------------------------------------------------------------------

void Editor::beginChange() {
    if (!app.isEditing() || changePending) return;
    pendingBefore = app.snapshot();
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

void Editor::cancelChange() {
    pendingBefore = Json();
    changePending = false;
}

void Editor::commitIfIdle() {
    // The gesture ends when the mouse is released or the edited field is left: that way
    // dragging a value, typing a name or a brush stroke is ONE step, not one
    // per frame.
    if (changePending && !dragging && !painting && !ImGui::IsAnyItemActive()) commitChange();
}

void Editor::undo() {
    if (!app.isEditing()) return;
    commitChange();
    if (undoStack.empty()) return;
    redoStack.push_back({ app.snapshot(), version });
    Snapshot s = std::move(undoStack.back());
    undoStack.pop_back();
    restore(s.scene);
    version = s.version;
    setStatus("Undo");
}

void Editor::redo() {
    if (!app.isEditing() || redoStack.empty()) return;
    commitChange();
    undoStack.push_back({ app.snapshot(), version });
    Snapshot s = std::move(redoStack.back());
    redoStack.pop_back();
    restore(s.scene);
    version = s.version;
    setStatus("Redo");
}

void Editor::restore(const Json& scene) {
    std::string error;
    if (!app.restore(scene, error))
        SDL_Log("Editor: could not restore the scene: %s", error.c_str());
    if (!app.world().isAlive(selected)) selected = NullEntity;
    dragging = pressOnEntity = painting = false;
}

// --- Unsaved changes -------------------------------------------------------------------

bool Editor::confirmDiscard(const char* action) {
    if (!isDirty()) return true;

    // Native dialog: it blocks until answered and does not depend on ImGui.
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Save" },
        { 0,                                       2, "Discard" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" },
    };
    std::string question = "The scene has unsaved changes:\n" + app.scenePath() +
                           "\n\nSave them before " + action + "?";
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    box.window = app.window();
    box.title = "Unsaved changes";
    box.message = question.c_str();
    box.numbuttons = 3;
    box.buttons = buttons;

    int choice = 0;
    if (!SDL_ShowMessageBox(&box, &choice)) {
        SDL_Log("Editor: could not ask (%s); the changes are discarded.", SDL_GetError());
        return true;
    }
    if (choice == 1) {
        if (!app.isEditing()) togglePlay(); // the scene is saved, not the game in progress
        save();
        return !isDirty(); // if saving fails, it is canceled
    }
    return choice == 2;
}
