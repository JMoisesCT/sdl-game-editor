// Prefabs in the editor: the panel that lists them (to place instances) and the
// Inspector header of an instance (Apply, Revert, Unlink) or of a standalone
// entity (Create prefab). See core/Prefabs.h.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "core/Hierarchy.h"
#include "core/Prefabs.h"
#include "core/Reflection.h"
#include "platform/AssetManager.h"

static const char* PREFAB_DIR = "assets/prefabs/";
static const char* PREFAB_EXT = ".prefab.json";
static const char* PREFAB_PAYLOAD = "PREFAB"; // type of the drag from the panel to the view
static const ImVec4 PREFAB_COLOR(0.45f, 0.75f, 1.0f, 1.0f); // the same one as in the Hierarchy

// "assets/prefabs/fruit.prefab.json" -> "fruit"
static std::string prefabName(const std::string& path) {
    std::string name = path.substr(path.find_last_of('/') + 1);
    std::size_t ext = name.rfind(PREFAB_EXT);
    return ext == std::string::npos ? name : name.substr(0, ext);
}

// A safe file name: letters, digits, '_' and '-'; spaces become '_'.
static std::string cleanFileName(const std::string& text) {
    std::string out;
    for (char c : text) {
        unsigned char u = (unsigned char)c;
        if (std::isalnum(u) && u < 128) out += c;
        else if (c == '_' || c == '-') out += c;
        else if (c == ' ') out += '_';
    }
    return out;
}

// --- Prefabs panel ---------------------------------------------------------------------

void Editor::scanPrefabFiles() {
    prefabFiles = listAssetFiles(PREFAB_EXT);
    prefabFilesScanned = true;
}

void Editor::reloadPrefabs() {
    // The scene is saved as overrides of the old prefabs and rebuilt on top of the
    // ones now on disk. It is not an undo step: the scene file does not change.
    //
    commitChange();
    Json scene = app.snapshot();
    app.prefabs().clear();
    restore(scene);
    app.assets().forgetFailures(); // a new image may be the one that was missing
    scanPrefabFiles();
    setStatus("Prefabs reloaded from disk");
}

void Editor::drawPrefabList() {
    if (!prefabFilesScanned) scanPrefabFiles();

    ImGui::BeginDisabled(!app.isEditing());
    if (ImGui::Button("Refresh")) reloadPrefabs();
    ImGui::SetItemTooltip("Rescans the prefabs and reloads them from disk");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Double-click: place it at the center of the view.\n"
                          "Drag into the view: place it there.\n"
                          "To create one: select an entity and use \"Create prefab\" in the Inspector.");
    ImGui::Separator();

    std::string place; // placed after the loop
    if (ImGui::BeginChild("##prefabs")) {
        if (prefabFiles.empty()) ImGui::TextDisabled("No prefabs in assets/");
        for (const std::string& path : prefabFiles) {
            std::string label = prefabName(path) + "###" + path;
            ImGui::PushStyleColor(ImGuiCol_Text, PREFAB_COLOR);
            ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) place = path;
            bool showHelp = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal); // before the drag

            // Dragging into the scene view (received by drawSceneView).
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(PREFAB_PAYLOAD, path.c_str(), path.size() + 1);
                ImGui::Text("%s", prefabName(path).c_str());
                ImGui::EndDragDropSource();
            }

            // Help: the path and, if it has a Sprite, how it looks.
            if (showHelp && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(path.c_str());
                const Json* comps = app.prefabs().find(path);
                if (!comps) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Could not read it (see the log)");
                auto sprite = comps ? comps->find("Sprite") : Json::const_iterator();
                if (comps && sprite != comps->end()) {
                    std::string texture = sprite->value("texture", "");
                    if (SDL_Texture* tex = texture.empty() ? nullptr : app.assets().loadTexture(texture)) {
                        float tw = 0, th = 0;
                        SDL_GetTextureSize(tex, &tw, &th);
                        Json src = sprite->value("source", Json::array({ 0, 0, 0, 0 }));
                        float sx = src[0].get<float>(), sy = src[1].get<float>();
                        float sw = src[2].get<float>(), sh = src[3].get<float>();
                        if (sw <= 0 || sh <= 0) { sx = sy = 0; sw = tw; sh = th; }
                        float k = std::max(1.0f, std::floor(64.0f * uiScale / std::max(sw, sh)));
                        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(sw * k, sh * k),
                                     ImVec2(sx / tw, sy / th), ImVec2((sx + sw) / tw, (sy + sh) / th));
                    }
                }
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();

    if (!place.empty()) {
        Vec2 p = sceneView.center;
        if (snapToGrid) {
            p.x = std::round(p.x / gridSize) * gridSize;
            p.y = std::round(p.y / gridSize) * gridSize;
        }
        placePrefab(place, p);
    }
}

Entity Editor::placePrefab(const std::string& path, Vec2 position) {
    World& world = app.world();
    if (!app.prefabs().find(path)) {
        setStatus("Could not read the prefab " + path + " (see the log)");
        return NullEntity;
    }
    beginChange();
    Entity e = app.prefabs().instantiate(world, path);
    Transform* t = world.get<Transform>(e);
    if (!t) t = &world.add<Transform>(e);
    t->position = position;
    selected = e;
    return e;
}

// --- Inspector header ------------------------------------------------------------------

void Editor::drawPrefabHeader() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    const bool editing = app.isEditing();

    if (!link) {
        ImGui::SameLine();
        ImGui::BeginDisabled(!editing);
        if (ImGui::SmallButton("Create prefab...")) {
            newPrefabName = cleanFileName(entityLabel(selected));
            ImGui::OpenPopup("##newprefab");
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Saves this entity as a prefab in %s and links it to it", PREFAB_DIR);
        if (ImGui::BeginPopup("##newprefab")) {
            ImGui::TextUnformatted("Prefab name:");
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(240.0f * uiScale);
            bool enter = ImGui::InputText("##name", &newPrefabName, ImGuiInputTextFlags_EnterReturnsTrue);
            std::string clean = cleanFileName(newPrefabName);
            ImGui::TextDisabled("%s%s%s", PREFAB_DIR, clean.empty() ? "?" : clean.c_str(), PREFAB_EXT);
            ImGui::BeginDisabled(clean.empty());
            if (ImGui::Button("Create") || (enter && !clean.empty())) {
                createPrefabFromSelected(clean);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        return;
    }

    ImGui::TextColored(PREFAB_COLOR, "Prefab: %s", prefabName(link->path).c_str());
    ImGui::SetItemTooltip("%s\nIn orange: the fields where this instance differs from the prefab.",
                          link->path.c_str());
    if (!app.prefabs().find(link->path)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(could not read it)");
    }

    // Not while playing: the state of the playthrough would be saved into the prefab.
    ImGui::BeginDisabled(!editing || !app.prefabs().find(link->path));
    if (ImGui::SmallButton("Apply")) applyToPrefab();
    ImGui::SetItemTooltip("Saves this instance's changes into the prefab: they reach all its\n"
                          "instances (except position and name). The prefab file is saved\n"
                          "right away, and Undo does not revert it.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Revert")) revertToPrefab();
    ImGui::SetItemTooltip("Discards what this instance changed: it matches the prefab again\n"
                          "(keeping its position and name).");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::SmallButton("Unlink")) unlinkPrefab();
    ImGui::SetItemTooltip("It stops being an instance: it keeps its components, but changes to the\n"
                          "prefab no longer reach it.");
    ImGui::EndDisabled();
}

// --- Actions ---------------------------------------------------------------------------

void Editor::rebuildEntity(Entity e, const Json& entity) {
    World& world = app.world();
    for (const auto& info : app.registry().all()) info->remove(world, e);
    world.remove<PrefabInstance>(e);
    deserializeEntity(world, app.registry(), &app.prefabs(), e, entity);
}

void Editor::createPrefabFromSelected(const std::string& name) {
    World& world = app.world();
    if (!app.isEditing() || !world.isAlive(selected) || name.empty()) return;
    std::string path = std::string(PREFAB_DIR) + name + PREFAB_EXT;
    if (SDL_GetPathInfo(path.c_str(), nullptr)) {
        setStatus(path + " already exists: choose another name");
        return;
    }

    // The prefab is the complete entity, but at the origin: the position belongs to each
    // instance.
    Json comps = serializeEntity(world, app.registry(), nullptr, selected)["components"];
    if (comps.contains("Transform")) comps["Transform"]["position"] = Json::array({ 0.0, 0.0 });
    if (!app.prefabs().save(path, comps)) {
        setStatus("Could not save " + path + " (see the log)");
        return;
    }
    beginChange(); // linking it changes how it is saved in the scene
    world.add<PrefabInstance>(selected).path = path;
    scanPrefabFiles();
    // No nested prefabs: the prefab is only this entity, without its branch.
    if (descendantsOf(world, selected).empty()) setStatus("Prefab created: " + path);
    else setStatus("Prefab created: " + path + " (its children are not part of the prefab)");
}

void Editor::applyToPrefab() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    const Json* current = link ? app.prefabs().find(link->path) : nullptr;
    if (!app.isEditing() || !current) return;
    const std::string path = link->path; // copy: the pool may move when rebuilding
    const Json old = *current;           // copy: save() replaces it

    // Each instance of this prefab, as overrides of the OLD prefab: afterwards they are
    // rebuilt on top of the new one and keep only what each one changed.
    commitChange();
    beginChange();
    std::vector<std::pair<Entity, Json>> instances;
    for (Entity e : world.entities()) {
        const PrefabInstance* l = world.get<PrefabInstance>(e);
        if (l && l->path == path) instances.push_back({ e, serializeEntity(world, app.registry(), &app.prefabs(), e) });
    }

    // The new prefab is the complete selection, except position and name, which belong
    // to each instance: those stay as they were in the prefab.
    Json comps = serializeEntity(world, app.registry(), nullptr, selected)["components"];
    if (comps.contains("Transform"))
        comps["Transform"]["position"] = old.contains("Transform") ? old["Transform"]["position"]
                                                                   : Json::array({ 0.0, 0.0 });
    if (comps.contains("EntityInfo") && old.contains("EntityInfo"))
        comps["EntityInfo"]["name"] = old["EntityInfo"]["name"];
    if (!app.prefabs().save(path, comps)) {
        cancelChange(); // nothing was touched
        setStatus("Could not save " + path + " (see the log)");
        return;
    }

    for (const auto& inst : instances) rebuildEntity(inst.first, inst.second);
    setStatus("Prefab saved: " + path + " (" + std::to_string(instances.size()) + " instances)");
}

void Editor::revertToPrefab() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    if (!app.isEditing() || !link || !app.prefabs().find(link->path)) return;

    // Only the position and the name survive.
    Json entity = serializeEntity(world, app.registry(), &app.prefabs(), selected);
    const Json& comps = entity["components"];
    Json keep = Json::object();
    if (comps.contains("Transform") && comps["Transform"].contains("position"))
        keep["Transform"]["position"] = comps["Transform"]["position"];
    if (comps.contains("EntityInfo") && comps["EntityInfo"].contains("name"))
        keep["EntityInfo"]["name"] = comps["EntityInfo"]["name"];
    Json reverted = Json::object();
    reverted["prefab"] = link->path;
    reverted["components"] = std::move(keep);

    beginChange();
    rebuildEntity(selected, reverted);
    setStatus("Instance reverted to the prefab");
}

void Editor::unlinkPrefab() {
    World& world = app.world();
    if (!app.isEditing() || !world.has<PrefabInstance>(selected)) return;
    beginChange();
    world.remove<PrefabInstance>(selected);
    setStatus("Unlinked from the prefab: it is now a standalone entity");
}
