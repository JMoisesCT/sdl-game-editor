// Prefabs en el editor: el panel que los lista (para colocar instancias) y la
// cabecera del Inspector de una instancia (Aplicar, Revertir, Desvincular) o de una
// entidad suelta (Crear prefab). Ver core/Prefabs.h.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "core/Prefabs.h"
#include "core/Reflection.h"
#include "platform/AssetManager.h"

static const char* PREFAB_DIR = "assets/prefabs/";
static const char* PREFAB_EXT = ".prefab.json";
static const char* PREFAB_PAYLOAD = "PREFAB"; // tipo del arrastre del panel a la vista
static const ImVec4 PREFAB_COLOR(0.45f, 0.75f, 1.0f, 1.0f); // el mismo que en la Jerarquia

// "assets/prefabs/fruit.prefab.json" -> "fruit"
static std::string prefabName(const std::string& path) {
    std::string name = path.substr(path.find_last_of('/') + 1);
    std::size_t ext = name.rfind(PREFAB_EXT);
    return ext == std::string::npos ? name : name.substr(0, ext);
}

// Un nombre de archivo seguro: letras, numeros, '_' y '-'; los espacios pasan a '_'.
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

// --- Panel Prefabs ---------------------------------------------------------------------

void Editor::scanPrefabFiles() {
    prefabFiles = listAssetFiles(PREFAB_EXT);
    prefabFilesScanned = true;
}

void Editor::reloadPrefabs() {
    // La escena se guarda como overrides de los prefabs viejos y se vuelve a armar
    // sobre los que hay ahora en disco. No es un paso de deshacer: el archivo de la
    // escena no cambia.
    commitChange();
    Json scene = app.snapshot();
    app.prefabs().clear();
    restore(scene);
    app.assets().forgetFailures(); // una imagen nueva puede ser la que faltaba
    scanPrefabFiles();
    setStatus("Prefabs vueltos a leer del disco");
}

void Editor::drawPrefabList() {
    if (!prefabFilesScanned) scanPrefabFiles();

    ImGui::BeginDisabled(!app.isEditing());
    if (ImGui::Button("Actualizar")) reloadPrefabs();
    ImGui::SetItemTooltip("Vuelve a buscar los prefabs y a leerlos del disco");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Doble clic: colocar en el centro de la vista.\n"
                          "Arrastrar a la vista: colocar ahi.\n"
                          "Para crear uno: elegir una entidad y \"Crear prefab\" en el Inspector.");
    ImGui::Separator();

    std::string place; // se coloca despues del bucle
    if (ImGui::BeginChild("##prefabs")) {
        if (prefabFiles.empty()) ImGui::TextDisabled("No hay prefabs en assets/");
        for (const std::string& path : prefabFiles) {
            std::string label = prefabName(path) + "###" + path;
            ImGui::PushStyleColor(ImGuiCol_Text, PREFAB_COLOR);
            ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) place = path;
            bool showHelp = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal); // antes del arrastre

            // Arrastrar a la vista de escena (la recibe drawSceneView).
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(PREFAB_PAYLOAD, path.c_str(), path.size() + 1);
                ImGui::Text("%s", prefabName(path).c_str());
                ImGui::EndDragDropSource();
            }

            // Ayuda: la ruta y, si tiene Sprite, como se ve.
            if (showHelp && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(path.c_str());
                const Json* comps = app.prefabs().find(path);
                if (!comps) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No se pudo leer (ver el log)");
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
        setStatus("No se pudo leer el prefab " + path + " (ver el log)");
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

// --- Cabecera del Inspector ------------------------------------------------------------

void Editor::drawPrefabHeader() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    const bool editing = app.isEditing();

    if (!link) {
        ImGui::SameLine();
        ImGui::BeginDisabled(!editing);
        if (ImGui::SmallButton("Crear prefab...")) {
            newPrefabName = cleanFileName(entityLabel(selected));
            ImGui::OpenPopup("##newprefab");
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Guarda esta entidad como prefab en %s y la enlaza a el", PREFAB_DIR);
        if (ImGui::BeginPopup("##newprefab")) {
            ImGui::TextUnformatted("Nombre del prefab:");
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(240.0f * uiScale);
            bool enter = ImGui::InputText("##name", &newPrefabName, ImGuiInputTextFlags_EnterReturnsTrue);
            std::string clean = cleanFileName(newPrefabName);
            ImGui::TextDisabled("%s%s%s", PREFAB_DIR, clean.empty() ? "?" : clean.c_str(), PREFAB_EXT);
            ImGui::BeginDisabled(clean.empty());
            if (ImGui::Button("Crear") || (enter && !clean.empty())) {
                createPrefabFromSelected(clean);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        return;
    }

    ImGui::TextColored(PREFAB_COLOR, "Prefab: %s", prefabName(link->path).c_str());
    ImGui::SetItemTooltip("%s\nEn naranja, los campos en que esta instancia difiere del prefab.",
                          link->path.c_str());
    if (!app.prefabs().find(link->path)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(no se pudo leer)");
    }

    // Jugando no: se guardaria en el prefab el estado de la partida.
    ImGui::BeginDisabled(!editing || !app.prefabs().find(link->path));
    if (ImGui::SmallButton("Aplicar")) applyToPrefab();
    ImGui::SetItemTooltip("Guarda los cambios de esta instancia en el prefab: llegan a todas sus\n"
                          "instancias (salvo posicion y nombre). El archivo del prefab se guarda\n"
                          "en el acto y Deshacer no lo vuelve atras.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Revertir")) revertToPrefab();
    ImGui::SetItemTooltip("Descarta lo que esta instancia cambio: vuelve a ser igual al prefab\n"
                          "(conserva su posicion y su nombre).");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!editing);
    if (ImGui::SmallButton("Desvincular")) unlinkPrefab();
    ImGui::SetItemTooltip("Deja de ser instancia: conserva sus componentes, pero los cambios del\n"
                          "prefab ya no le llegan.");
    ImGui::EndDisabled();
}

// --- Acciones --------------------------------------------------------------------------

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
        setStatus("Ya existe " + path + ": elegi otro nombre");
        return;
    }

    // El prefab es la entidad completa, pero en el origen: la posicion es de cada
    // instancia.
    Json comps = serializeEntity(world, app.registry(), nullptr, selected)["components"];
    if (comps.contains("Transform")) comps["Transform"]["position"] = Json::array({ 0.0, 0.0 });
    if (!app.prefabs().save(path, comps)) {
        setStatus("No se pudo guardar " + path + " (ver el log)");
        return;
    }
    beginChange(); // enlazarla cambia como se guarda en la escena
    world.add<PrefabInstance>(selected).path = path;
    scanPrefabFiles();
    setStatus("Prefab creado: " + path);
}

void Editor::applyToPrefab() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    const Json* current = link ? app.prefabs().find(link->path) : nullptr;
    if (!app.isEditing() || !current) return;
    const std::string path = link->path; // copia: el pool puede moverse al rearmar
    const Json old = *current;           // copia: save() lo reemplaza

    // Cada instancia de este prefab, como overrides del prefab VIEJO: despues se
    // rearman sobre el nuevo y conservan solo lo que cada una cambio.
    commitChange();
    beginChange();
    std::vector<std::pair<Entity, Json>> instances;
    for (Entity e : world.entities()) {
        const PrefabInstance* l = world.get<PrefabInstance>(e);
        if (l && l->path == path) instances.push_back({ e, serializeEntity(world, app.registry(), &app.prefabs(), e) });
    }

    // El prefab nuevo es la seleccion completa, salvo la posicion y el nombre, que son
    // de cada instancia: esos quedan como estaban en el prefab.
    Json comps = serializeEntity(world, app.registry(), nullptr, selected)["components"];
    if (comps.contains("Transform"))
        comps["Transform"]["position"] = old.contains("Transform") ? old["Transform"]["position"]
                                                                   : Json::array({ 0.0, 0.0 });
    if (comps.contains("EntityInfo") && old.contains("EntityInfo"))
        comps["EntityInfo"]["name"] = old["EntityInfo"]["name"];
    if (!app.prefabs().save(path, comps)) {
        cancelChange(); // no se toco nada
        setStatus("No se pudo guardar " + path + " (ver el log)");
        return;
    }

    for (const auto& inst : instances) rebuildEntity(inst.first, inst.second);
    setStatus("Prefab guardado: " + path + " (" + std::to_string(instances.size()) + " instancias)");
}

void Editor::revertToPrefab() {
    World& world = app.world();
    const PrefabInstance* link = world.get<PrefabInstance>(selected);
    if (!app.isEditing() || !link || !app.prefabs().find(link->path)) return;

    // Solo sobreviven la posicion y el nombre.
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
    setStatus("Instancia revertida al prefab");
}

void Editor::unlinkPrefab() {
    World& world = app.world();
    if (!app.isEditing() || !world.has<PrefabInstance>(selected)) return;
    beginChange();
    world.remove<PrefabInstance>(selected);
    setStatus("Desvinculada del prefab: ahora es una entidad suelta");
}
