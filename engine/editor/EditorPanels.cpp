// Paneles Jerarquia e Inspector del editor. El Inspector no conoce ningun componente:
// recorre los campos que cada uno declaro a la reflexion y dibuja un widget por tipo.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "core/Reflection.h"
#include "platform/AssetManager.h"

// --- Jerarquia -------------------------------------------------------------------------

std::string Editor::entityLabel(Entity e) {
    const EntityInfo* info = app.world().get<EntityInfo>(e);
    if (info && !info->name.empty()) return info->name;
    return "Entidad " + std::to_string(e);
}

void Editor::drawHierarchy() {
    World& world = app.world();

    if (ImGui::Button("+ Nueva")) createEntity();
    ImGui::SameLine();
    ImGui::BeginDisabled(!world.isAlive(selected));
    if (ImGui::Button("Duplicar")) duplicateSelected();
    ImGui::SetItemTooltip("Ctrl+D");
    ImGui::SameLine();
    if (ImGui::Button("Borrar")) deleteSelected();
    ImGui::SetItemTooltip("Supr");
    ImGui::EndDisabled();
    ImGui::Separator();

    // Las acciones del menu contextual se aplican despues del bucle: borrar o duplicar
    // dentro cambiaria la lista que se esta recorriendo.
    enum class Action { None, Duplicate, Delete } action = Action::None;

    if (ImGui::BeginChild("##entities")) {
        for (Entity e : world.entities()) {
            std::string label = entityLabel(e) + "###e" + std::to_string(e);
            if (ImGui::Selectable(label.c_str(), selected == e, ImGuiSelectableFlags_AllowDoubleClick)) {
                selected = e;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) frameEntity(e);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                const EntityInfo* info = world.get<EntityInfo>(e);
                ImGui::SetTooltip("id %u%s%s", e, info && !info->tag.empty() ? "  tag: " : "",
                                  info ? info->tag.c_str() : "");
            }
            if (ImGui::BeginPopupContextItem()) {
                selected = e;
                if (ImGui::MenuItem("Duplicar", "Ctrl+D")) action = Action::Duplicate;
                if (ImGui::MenuItem("Borrar", "Supr")) action = Action::Delete;
                ImGui::EndPopup();
            }
        }
        // Clic en el hueco libre de la lista: quitar la seleccion.
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered())
            selected = NullEntity;
    }
    ImGui::EndChild();

    if (action == Action::Duplicate) duplicateSelected();
    if (action == Action::Delete) deleteSelected();
}

Entity Editor::createEntity() {
    World& world = app.world();
    beginChange();
    Entity e = world.create();
    world.add<EntityInfo>(e).name = "Entidad " + std::to_string(e);
    Transform t;
    // Aparece en el centro de la vista del editor (en la grilla, si se ajusta).
    t.position = sceneView.center;
    if (snapToGrid) {
        t.position.x = std::round(t.position.x / gridSize) * gridSize;
        t.position.y = std::round(t.position.y / gridSize) * gridSize;
    }
    world.add<Transform>(e, t);
    selected = e;
    return e;
}

void Editor::duplicateSelected() {
    World& world = app.world();
    if (!world.isAlive(selected)) return;
    beginChange();
    // Copiar = serializar y deserializar en una entidad nueva: copia exactamente lo
    // que se guarda en el archivo, ni mas ni menos.
    Json copy = serializeEntity(world, app.registry(), selected);
    Entity e = world.create();
    deserializeComponents(world, app.registry(), e, copy["components"]);
    if (EntityInfo* info = world.get<EntityInfo>(e)) info->name += " (copia)";
    if (Transform* t = world.get<Transform>(e)) {
        t->position.x += gridSize; // corrida un paso, para que se vea que hay dos
        t->position.y += gridSize;
    }
    selected = e;
}

void Editor::deleteSelected() {
    World& world = app.world();
    if (!world.isAlive(selected)) return;
    beginChange();
    world.destroyNow(selected);
    selected = NullEntity;
}

// --- Inspector -------------------------------------------------------------------------

void Editor::drawInspector() {
    World& world = app.world();
    const ComponentRegistry& registry = app.registry();

    if (!world.isAlive(selected)) {
        ImGui::TextDisabled("Nada seleccionado.");
        ImGui::TextDisabled("Clic en una entidad de la vista");
        ImGui::TextDisabled("o de la Jerarquia.");
        return;
    }

    ImGui::Text("Entidad %u", selected);
    ImGui::Separator();

    const ComponentInfo* toRemove = nullptr;
    for (const auto& infoPtr : registry.all()) {
        const ComponentInfo& info = *infoPtr;
        void* component = info.get(world, selected);
        if (!component) continue;

        ImGui::PushID(info.name.c_str());
        bool keep = true;
        bool open = info.removable
            ? ImGui::CollapsingHeader(info.name.c_str(), &keep, ImGuiTreeNodeFlags_DefaultOpen)
            : ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (open) {
            for (const FieldInfo& f : info.fields) drawField(f, f.in(component));
            if (info.fields.empty()) ImGui::TextDisabled("(sin campos)");
        }
        if (!keep) toRemove = &info;
        ImGui::PopID();
    }
    if (toRemove) {
        beginChange();
        toRemove->remove(world, selected);
    }

    // Unica excepcion al Inspector generico: la paleta para pintar un Tilemap.
    drawTilePalette();

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Agregar componente", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##add");
    if (ImGui::BeginPopup("##add")) {
        bool any = false;
        for (const auto& info : registry.all()) {
            if (info->has(world, selected)) continue;
            any = true;
            if (ImGui::MenuItem(info->name.c_str())) {
                beginChange();
                info->add(world, selected);
            }
        }
        if (!any) ImGui::TextDisabled("Ya tiene todos los componentes");
        ImGui::EndPopup();
    }
}

// "6, 7 ,8" -> {6, 7, 8}. Lo que no es un numero hace de separador.
static std::vector<int> parseIntList(const std::string& text) {
    std::vector<int> out;
    const char* p = text.c_str();
    while (*p) {
        char* end = nullptr;
        long n = std::strtol(p, &end, 10);
        if (end == p) { ++p; continue; }
        out.push_back((int)n);
        p = end;
    }
    return out;
}

// Cada widget edita una COPIA del valor; solo si cambio se llama a beginChange (que
// guarda la escena tal como estaba) y despues se escribe en el componente.
bool Editor::drawField(const FieldInfo& f, void* value) {
    const char* label = f.name.c_str();
    const bool ranged = f.max > f.min;
    const ImGuiSliderFlags clamp = ranged ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
    bool changed = false;

    switch (f.type) {
    case FieldType::Bool: {
        bool v = *static_cast<bool*>(value);
        if (ImGui::Checkbox(label, &v)) { beginChange(); *static_cast<bool*>(value) = v; changed = true; }
        break;
    }
    case FieldType::Int: {
        int v = *static_cast<int*>(value);
        if (ImGui::DragInt(label, &v, f.speed, ranged ? (int)f.min : 0, ranged ? (int)f.max : 0,
                           "%d", clamp)) {
            beginChange(); *static_cast<int*>(value) = v; changed = true;
        }
        break;
    }
    case FieldType::Float: {
        float v = *static_cast<float*>(value);
        if (ImGui::DragFloat(label, &v, f.speed, ranged ? f.min : 0.0f, ranged ? f.max : 0.0f,
                             "%.2f", clamp)) {
            beginChange(); *static_cast<float*>(value) = v; changed = true;
        }
        break;
    }
    case FieldType::String: {
        std::string& s = *static_cast<std::string*>(value);
        std::string v = s;
        bool edited = f.hint == FieldHint::TexturePath ? drawTexturePicker(label, s, v)
                                                       : ImGui::InputText(label, &v);
        if (edited && v != s) { beginChange(); s = v; changed = true; }
        break;
    }
    case FieldType::Vec2: {
        Vec2& p = *static_cast<Vec2*>(value);
        float v[2] = { p.x, p.y };
        if (ImGui::DragFloat2(label, v, f.speed, ranged ? f.min : 0.0f, ranged ? f.max : 0.0f,
                              "%.2f", clamp)) {
            beginChange(); p = { v[0], v[1] }; changed = true;
        }
        break;
    }
    case FieldType::Color: {
        Color& c = *static_cast<Color*>(value);
        float v[4] = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
        if (ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_Uint8)) {
            auto b = [](float x) { return (std::uint8_t)std::lround(std::clamp(x, 0.0f, 1.0f) * 255.0f); };
            Color nc{ b(v[0]), b(v[1]), b(v[2]), b(v[3]) };
            if (nc.r != c.r || nc.g != c.g || nc.b != c.b || nc.a != c.a) {
                beginChange(); c = nc; changed = true;
            }
        }
        break;
    }
    case FieldType::IntRect: {
        IntRect& r = *static_cast<IntRect*>(value);
        int v[4] = { r.x, r.y, r.w, r.h };
        if (ImGui::DragInt4(label, v, f.speed)) {
            beginChange(); r = { v[0], v[1], v[2], v[3] }; changed = true;
        }
        break;
    }
    case FieldType::IntList: {
        // Texto con los numeros separados por coma: "6, 7, 8".
        std::vector<int>& list = *static_cast<std::vector<int>*>(value);
        std::string text;
        for (std::size_t i = 0; i < list.size(); ++i) text += (i ? ", " : "") + std::to_string(list[i]);
        if (ImGui::InputText(label, &text)) {
            std::vector<int> v = parseIntList(text);
            if (v != list) { beginChange(); list = std::move(v); changed = true; }
        }
        break;
    }
    case FieldType::IntGrid: {
        // Solo el tamano (las celdas se editan con la paleta). InputInt2 aplica al
        // validar (Enter o salir del campo), no con cada tecla: escribir "30" pasaria
        // por "3", y achicar borra lo que queda fuera.
        IntGrid& g = *static_cast<IntGrid*>(value);
        int v[2] = { g.width, g.height };
        if (ImGui::InputInt2(label, v)) {
            v[0] = std::clamp(v[0], 0, 4096);
            v[1] = std::clamp(v[1], 0, 4096);
            if (v[0] != g.width || v[1] != g.height) {
                beginChange(); g.resize(v[0], v[1], -1); changed = true;
            }
        }
        break;
    }
    }

    if (!f.tooltip.empty()) ImGui::SetItemTooltip("%s", f.tooltip.c_str());
    return changed;
}

// --- Selector de texturas --------------------------------------------------------------

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

void Editor::scanTextureFiles() {
    namespace fs = std::filesystem;
    textureFiles.clear();
    std::error_code ec;
    for (fs::recursive_directory_iterator it("assets", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (toLower(it->path().extension().string()) != ".png") continue;
        textureFiles.push_back(it->path().generic_string()); // con "/" tambien en Windows
    }
    std::sort(textureFiles.begin(), textureFiles.end());
    textureFilesScanned = true;
    app.assets().forgetFailures(); // una imagen nueva puede ser la que faltaba
}

bool Editor::drawTexturePicker(const char* label, const std::string& current, std::string& out) {
    bool changed = false;
    ImGui::PushID(label);

    const ImGuiStyle& style = ImGui::GetStyle();
    float buttonW = ImGui::CalcTextSize("...").x + style.FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - buttonW - style.ItemInnerSpacing.x);
    out = current;
    if (ImGui::InputText("##path", &out)) changed = true;
    if (!current.empty()) ImGui::SetItemTooltip("%s", current.c_str());
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    if (ImGui::Button("...")) {
        if (!textureFilesScanned) scanTextureFiles();
        ImGui::OpenPopup("##pick");
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);

    if (ImGui::BeginPopup("##pick")) {
        ImGui::SetNextItemWidth(320.0f * uiScale);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##filter", "filtrar...", &textureFilter);
        ImGui::SameLine();
        if (ImGui::Button("Volver a buscar")) scanTextureFiles();

        std::string filter = toLower(textureFilter);
        if (ImGui::BeginChild("##files", ImVec2(560.0f * uiScale, 320.0f * uiScale))) {
            for (const std::string& file : textureFiles) {
                if (!filter.empty() && toLower(file).find(filter) == std::string::npos) continue;
                if (ImGui::Selectable(file.c_str(), file == current)) {
                    out = file;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                // Vista previa al pasar el raton.
                if (ImGui::IsItemHovered()) {
                    if (SDL_Texture* tex = app.assets().loadTexture(file)) {
                        float tw = 0, th = 0;
                        SDL_GetTextureSize(tex, &tw, &th);
                        float k = std::min(1.0f, 256.0f * uiScale / std::max(tw, th));
                        k = std::max(k, std::floor(64.0f * uiScale / std::max(tw, th))); // las chicas, grandes
                        ImGui::BeginTooltip();
                        ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(tw * k, th * k));
                        ImGui::Text("%.0f x %.0f px", tw, th);
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}
