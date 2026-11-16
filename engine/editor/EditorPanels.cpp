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
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "core/Hierarchy.h"
#include "core/Prefabs.h"
#include "core/Reflection.h"
#include "platform/AssetManager.h"
#include "platform/Audio.h"

// Instancias de prefab en la Jerarquia (el mismo color que en EditorPrefabs.cpp) y
// campos en que una instancia difiere de su prefab.
static const ImVec4 PREFAB_COLOR(0.45f, 0.75f, 1.0f, 1.0f);
static const ImVec4 OVERRIDE_COLOR(1.0f, 0.72f, 0.3f, 1.0f);
// Tipo del arrastre de una entidad desde la Jerarquia (a otra entidad o a un campo
// Entity del Inspector). El dato es el Entity.
static const char* ENTITY_PAYLOAD = "ENTITY";

// --- Jerarquia -------------------------------------------------------------------------

std::string Editor::entityLabel(Entity e) {
    const EntityInfo* info = app.world().get<EntityInfo>(e);
    if (info && !info->name.empty()) return info->name;
    return "Entity " + std::to_string(e);
}

// El hermano que sigue a e (saltando 'skip'), o NullEntity si es el ultimo.
static Entity nextSibling(const World& world, Entity e, Entity skip) {
    std::vector<Entity> siblings = childrenOf(world, parentOf(world, e));
    auto it = std::find(siblings.begin(), siblings.end(), e);
    if (it == siblings.end()) return NullEntity;
    for (++it; it != siblings.end(); ++it)
        if (*it != skip) return *it;
    return NullEntity;
}

void Editor::drawHierarchy() {
    World& world = app.world();

    if (ImGui::Button("+ New")) createEntity();
    ImGui::SameLine();
    ImGui::BeginDisabled(!world.isAlive(selected));
    if (ImGui::Button("Duplicate")) duplicateSelected();
    ImGui::SetItemTooltip("Ctrl+D (with its whole branch)");
    ImGui::SameLine();
    if (ImGui::Button("Delete")) deleteSelected();
    ImGui::SetItemTooltip("Del (with its whole branch)");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ImGui::SetItemTooltip("Drag an entity onto another: it becomes its child.\n"
                          "Onto the top or bottom edge: places it before or after.\n"
                          "Into the empty space below: takes it out of its group.\n"
                          "Moving a parent also moves its children (only while editing).");
    ImGui::Separator();

    // Las acciones se aplican despues de dibujar el arbol: borrar, duplicar o mover
    // dentro cambiaria la lista que se esta recorriendo.
    enum class Action { None, Duplicate, Delete, CreateChild, Move };
    struct HierarchyAction {
        Action kind = Action::None;
        Entity e = NullEntity, parent = NullEntity, before = NullEntity;
    } act;

    // Padre -> hijos, en el orden del World (NullEntity = las raices).
    std::unordered_map<Entity, std::vector<Entity>> children;
    for (Entity e : world.entities()) children[parentOf(world, e)].push_back(e);

    // Si la seleccion cambio (p. ej. se eligio en la vista), se abre su rama y se la
    // trae a la vista una vez.
    const bool reveal = selected != revealedSelection && world.isAlive(selected);
    std::unordered_set<Entity> toOpen;
    if (reveal)
        for (Entity p = parentOf(world, selected); p != NullEntity; p = parentOf(world, p)) toOpen.insert(p);

    const ImU32 dropColor = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
    std::function<void(Entity)> drawNode = [&](Entity e) {
        auto kids = children.find(e);
        const bool leaf = kids == children.end();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_DefaultOpen;
        if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selected == e) flags |= ImGuiTreeNodeFlags_Selected;
        if (toOpen.count(e)) ImGui::SetNextItemOpen(true);

        std::string label = entityLabel(e) + "###e" + std::to_string(e);
        const PrefabInstance* link = world.get<PrefabInstance>(e);
        if (link) ImGui::PushStyleColor(ImGuiCol_Text, PREFAB_COLOR);
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (link) ImGui::PopStyleColor();

        // Clic elige (salvo en la flecha, que abre o cierra), pero al SOLTAR y si no se
        // arrastro: asi se puede arrastrar una entidad a un campo Entity del Inspector
        // sin que el Inspector pase a mostrar a la arrastrada. Doble clic centra.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) hierarchyPress = e;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            frameEntity(e);
        if (reveal && e == selected && !ImGui::IsItemVisible()) ImGui::SetScrollHereY();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const EntityInfo* info = world.get<EntityInfo>(e);
            ImGui::SetTooltip("id %u%s%s%s%s", e, info && !info->tag.empty() ? "  tag: " : "",
                              info ? info->tag.c_str() : "", link ? "\nprefab: " : "",
                              link ? link->path.c_str() : "");
        }

        // Arrastrar: a otra entidad de la Jerarquia o a un campo Entity del Inspector.
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(ENTITY_PAYLOAD, &e, sizeof(Entity));
            ImGui::TextUnformatted(entityLabel(e).c_str());
            ImGui::EndDragDropSource();
        }
        // Soltar: el cuarto de arriba = antes, el de abajo = despues, el medio = hija.
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                ENTITY_PAYLOAD, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
            Entity dragged = payload ? *static_cast<const Entity*>(payload->Data) : NullEntity;
            // No se puede soltar una entidad sobre si misma ni sobre su propia rama.
            if (payload && dragged != e && !isAncestorOf(world, dragged, e)) {
                ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
                float y = ImGui::GetIO().MousePos.y, quarter = (r1.y - r0.y) * 0.25f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                Entity parent = parentOf(world, e), before = NullEntity;
                if (y < r0.y + quarter) {
                    before = e;
                    dl->AddLine(ImVec2(r0.x, r0.y), ImVec2(r1.x, r0.y), dropColor, 2.0f);
                } else if (y > r1.y - quarter) {
                    before = nextSibling(world, e, dragged);
                    dl->AddLine(ImVec2(r0.x, r1.y), ImVec2(r1.x, r1.y), dropColor, 2.0f);
                } else {
                    parent = e;
                    dl->AddRect(r0, r1, dropColor, 0.0f, 0, 2.0f);
                }
                if (payload->IsDelivery()) act = { Action::Move, dragged, parent, before };
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem()) {
            selected = e;
            if (ImGui::MenuItem("Create child")) act = { Action::CreateChild, e };
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) act = { Action::Duplicate, e };
            if (ImGui::MenuItem("Delete", "Del")) act = { Action::Delete, e };
            if (parentOf(world, e) != NullEntity && ImGui::MenuItem("Remove from group"))
                act = { Action::Move, e, parentOf(world, parentOf(world, e)), nextSibling(world, parentOf(world, e), e) };
            ImGui::EndPopup();
        }

        if (open && !leaf) {
            for (Entity child : kids->second) drawNode(child);
            ImGui::TreePop();
        }
    };

    if (ImGui::BeginChild("##entities")) {
        auto roots = children.find(NullEntity);
        if (roots != children.end())
            for (Entity e : roots->second) drawNode(e);

        // El hueco libre de abajo: soltar ahi saca a la entidad de su grupo (queda
        // ultima entre las raices); un clic quita la seleccion.
        ImVec2 rest = ImGui::GetContentRegionAvail();
        ImGui::Dummy(ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, ImGui::GetFrameHeight())));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD))
                act = { Action::Move, *static_cast<const Entity*>(payload->Data), NullEntity, NullEntity };
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered())
            selected = NullEntity;
    }
    ImGui::EndChild();

    if (hierarchyPress != NullEntity && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();
        const bool dragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= io.MouseDragThreshold * io.MouseDragThreshold;
        if (!dragged && world.isAlive(hierarchyPress)) selected = hierarchyPress;
        hierarchyPress = NullEntity;
    }
    revealedSelection = selected;

    switch (act.kind) {
    case Action::Duplicate:   duplicateSelected(); break;
    case Action::Delete:      deleteSelected(); break;
    case Action::CreateChild: createEntity(act.e); break;
    case Action::Move:
        if (world.isAlive(act.e)) {
            // Soltar donde ya estaba no es un cambio (ni un paso de deshacer).
            const bool wasPending = changePending;
            const std::vector<Entity> oldOrder = world.entities();
            const Entity oldParent = parentOf(world, act.e);
            beginChange();
            if (moveInHierarchy(world, act.e, act.parent, act.before)) selected = act.e;
            if (!wasPending && world.entities() == oldOrder && parentOf(world, act.e) == oldParent) cancelChange();
        }
        break;
    case Action::None: break;
    }
}

Entity Editor::createEntity(Entity parent) {
    World& world = app.world();
    beginChange();
    Entity e = world.create();
    world.add<EntityInfo>(e).name = "Entity " + std::to_string(e);
    Transform t;
    const Transform* pt = world.get<Transform>(parent);
    if (pt) {
        // Una hija nace donde esta su padre.
        t.position = pt->position;
    } else {
        // Aparece en el centro de la vista del editor (en la grilla, si se ajusta).
        t.position = sceneView.center;
        if (snapToGrid) {
            t.position.x = std::round(t.position.x / gridSize) * gridSize;
            t.position.y = std::round(t.position.y / gridSize) * gridSize;
        }
    }
    world.add<Transform>(e, t);
    if (world.isAlive(parent)) moveInHierarchy(world, e, parent);
    selected = e;
    return e;
}

void Editor::duplicateSelected() {
    World& world = app.world();
    if (!world.isAlive(selected)) return;
    beginChange();
    const ComponentRegistry& registry = app.registry();

    // La entidad y toda su rama. Copiar = serializar y deserializar en entidades
    // nuevas: copia exactamente lo que se guarda en el archivo, ni mas ni menos (una
    // instancia sigue enlazada).
    std::vector<Entity> branch = descendantsOf(world, selected);
    branch.insert(branch.begin(), selected);
    std::unordered_map<Entity, Entity> copyOf; // original -> copia
    for (Entity src : branch) copyOf[src] = world.create();
    for (Entity src : branch) {
        Entity dst = copyOf[src];
        Json copy = serializeEntity(world, registry, &app.prefabs(), src);
        copy.erase("parent"); // el de la copia se pone abajo
        deserializeEntity(world, registry, &app.prefabs(), dst, copy);
        if (src != selected) setParent(world, dst, copyOf[parentOf(world, src)]);
        // Lo que apuntaba dentro de la rama pasa a apuntar a la copia; lo de afuera
        // sigue apuntando a lo mismo.
        forEachEntityField(registry, world, dst, [&](Entity& ref) {
            auto it = copyOf.find(ref);
            if (it != copyOf.end()) ref = it->second;
        });
        if (Transform* t = world.get<Transform>(dst)) {
            t->position.x += gridSize; // corrida un paso, para que se vea que hay dos
            t->position.y += gridSize;
        }
    }
    Entity root = copyOf[selected];
    if (EntityInfo* info = world.get<EntityInfo>(root)) info->name += " (copy)";
    // Justo despues del original, entre sus hermanos.
    moveInHierarchy(world, root, parentOf(world, selected), nextSibling(world, selected, root));
    selected = root;
}

void Editor::deleteSelected() {
    World& world = app.world();
    if (!world.isAlive(selected)) return;
    beginChange();
    for (Entity d : descendantsOf(world, selected)) world.destroyNow(d);
    world.destroyNow(selected);
    selected = NullEntity;
}

void Editor::moveBranch(Entity e, Vec2 delta) {
    // Jugando no: los hijos no siguen al padre en el juego, y en el editor tampoco.
    if (!app.isEditing() || (delta.x == 0.0f && delta.y == 0.0f)) return;
    World& world = app.world();
    const bool hud = isHudEntity(e);
    for (Entity d : descendantsOf(world, e)) {
        Transform* t = world.get<Transform>(d);
        if (!t || isHudEntity(d) != hud) continue;
        t->position.x += delta.x;
        t->position.y += delta.y;
    }
}

// --- Inspector -------------------------------------------------------------------------

void Editor::drawInspector() {
    World& world = app.world();
    const ComponentRegistry& registry = app.registry();

    if (!world.isAlive(selected)) {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::TextDisabled("Click an entity in the view");
        ImGui::TextDisabled("or in the Hierarchy.");
        return;
    }

    ImGui::Text("Entity %u", selected);
    if (Entity parent = parentOf(world, selected)) {
        ImGui::SameLine();
        ImGui::TextDisabled("(child of %s)", entityLabel(parent).c_str());
    }
    drawPrefabHeader();
    ImGui::Separator();

    // Si es instancia de un prefab, lo que difiere de el (la forma en que se guarda en
    // la escena): esos campos se resaltan.
    Json overrides;
    if (const PrefabInstance* link = world.get<PrefabInstance>(selected))
        if (app.prefabs().find(link->path))
            overrides = serializeEntity(world, registry, &app.prefabs(), selected)["components"];

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
            // Mover al padre desde el Inspector mueve tambien a sus hijos.
            Transform* moved = info.typeId == componentTypeId<Transform>() ? static_cast<Transform*>(component) : nullptr;
            const Vec2 before = moved ? moved->position : Vec2{};
            auto changed = overrides.find(info.name); // sin prefab, overrides es null: end()
            for (const FieldInfo& f : info.fields) {
                bool over = changed != overrides.end() && changed->contains(f.name);
                if (over) ImGui::PushStyleColor(ImGuiCol_Text, OVERRIDE_COLOR);
                drawField(f, f.in(component));
                if (over) ImGui::PopStyleColor();
            }
            if (info.fields.empty()) ImGui::TextDisabled("(no fields)");
            if (moved)
                moveBranch(selected, { moved->position.x - before.x, moved->position.y - before.y });
        }
        if (!keep) toRemove = &info;
        ImGui::PopID();
    }
    if (toRemove) {
        beginChange();
        toRemove->remove(world, selected);
    }

    // Excepciones al Inspector generico: la cabecera de prefab (arriba) y la paleta
    // para pintar un Tilemap.
    drawTilePalette();

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Add component", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##add");
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
        if (!any) ImGui::TextDisabled("It already has every component");
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
        bool edited = false;
        if (f.hint == FieldHint::TexturePath) edited = drawFilePicker(label, s, v, false);
        else if (f.hint == FieldHint::AudioPath) edited = drawFilePicker(label, s, v, true);
        else if (f.hint == FieldHint::Choice && f.choices) edited = drawChoice(label, f.choices(app.world(), selected), s, v);
        else edited = ImGui::InputText(label, &v);
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
    case FieldType::Entity: {
        Entity& ref = *static_cast<Entity*>(value);
        Entity v = ref;
        if (drawEntityPicker(label, ref, v) && v != ref) { beginChange(); ref = v; changed = true; }
        break;
    }
    case FieldType::List:
        // La ayuda va en la cabecera de la lista (dentro, cada campo tiene la suya).
        return drawList(f, value);
    }

    if (!f.tooltip.empty()) ImGui::SetItemTooltip("%s", f.tooltip.c_str());
    return changed;
}

// Lista de structs: una cabecera plegable por elemento, con sus campos dentro y los
// botones para reordenar, duplicar y quitar. Las acciones se aplican despues de
// recorrer la lista (cambiarla en medio moveria los elementos que se estan dibujando).
bool Editor::drawList(const FieldInfo& f, void* value) {
    const std::size_t count = f.listSize(value);
    std::string header = f.name + " (" + std::to_string(count) + ")###" + f.name;
    bool open = ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    if (!f.tooltip.empty()) ImGui::SetItemTooltip("%s", f.tooltip.c_str());
    if (!open) return false;

    enum class Action { None, Up, Down, Duplicate, Remove, Add } action = Action::None;
    std::size_t target = 0;
    bool changed = false;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonW = ImGui::GetFrameHeight();

    for (std::size_t i = 0; i < count; ++i) {
        ImGui::PushID((int)i);
        void* item = f.listElement(value, i);
        // El titulo es el primer campo de texto del elemento (el nombre del clip).
        std::string title = "#" + std::to_string(i);
        for (const FieldInfo& ef : f.elementFields)
            if (ef.type == FieldType::String) {
                const std::string& s = *static_cast<const std::string*>(ef.in(static_cast<const void*>(item)));
                if (!s.empty()) title += "  " + s;
                break;
            }
        title += "###item";

        // Los botones a la derecha de la cabecera: se dibujan despues, en la misma linea.
        const float buttonsW = buttonW * 4.0f + style.ItemInnerSpacing.x * 3.0f;
        bool itemOpen = ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - buttonsW));
        ImGui::BeginDisabled(i == 0);
        if (ImGui::Button("^", ImVec2(buttonW, 0))) { action = Action::Up; target = i; }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Move up");
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::BeginDisabled(i + 1 == count);
        if (ImGui::Button("v", ImVec2(buttonW, 0))) { action = Action::Down; target = i; }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Move down");
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        if (ImGui::Button("+", ImVec2(buttonW, 0))) { action = Action::Duplicate; target = i; }
        ImGui::SetItemTooltip("Duplicate");
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        if (ImGui::Button("x", ImVec2(buttonW, 0))) { action = Action::Remove; target = i; }
        ImGui::SetItemTooltip("Remove");

        if (itemOpen) {
            for (const FieldInfo& ef : f.elementFields)
                if (drawField(ef, ef.in(item))) changed = true;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (ImGui::Button("Add")) action = Action::Add;
    ImGui::TreePop();

    if (action != Action::None) {
        beginChange();
        switch (action) {
        case Action::Up:        f.listSwap(value, target, target - 1); break;
        case Action::Down:      f.listSwap(value, target, target + 1); break;
        case Action::Duplicate: f.listDuplicate(value, target); break;
        case Action::Remove:    f.listErase(value, target); break;
        case Action::Add:       f.listResize(value, count + 1); break;
        case Action::None:      break;
        }
        changed = true;
    }
    return changed;
}

// Lista desplegable con los valores sugeridos ("" se ve como "(none)"). Un valor
// que no esta en la lista (escrito a mano en el archivo) se sigue viendo, marcado.
bool Editor::drawChoice(const char* label, const std::vector<std::string>& options,
                        const std::string& current, std::string& out) {
    out = current;
    bool known = std::find(options.begin(), options.end(), current) != options.end();
    std::string preview = current.empty() ? "(none)" : current;
    if (!known) preview += "  (?)";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const std::string& option : options) {
            if (ImGui::Selectable(option.empty() ? "(none)" : option.c_str(), option == current)) {
                out = option;
                changed = true;
            }
        }
        if (options.empty()) ImGui::TextDisabled("(no options)");
        ImGui::EndCombo();
    }
    if (!known) ImGui::SetItemTooltip("'%s' is not in the list", current.c_str());
    return changed;
}

// --- Selector de archivos (imagenes y sonidos) -----------------------------------------

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static bool isAudioFile(const std::string& ext) {
    return ext == ".wav" || ext == ".ogg" || ext == ".opus" || ext == ".mp3" || ext == ".flac";
}

void Editor::scanAssetFiles() {
    namespace fs = std::filesystem;
    textureFiles.clear();
    audioFiles.clear();
    std::error_code ec;
    for (fs::recursive_directory_iterator it("assets", ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string ext = toLower(it->path().extension().string());
        std::string path = it->path().generic_string(); // con "/" tambien en Windows
        if (ext == ".png") textureFiles.push_back(path);
        else if (isAudioFile(ext)) audioFiles.push_back(path);
    }
    std::sort(textureFiles.begin(), textureFiles.end());
    std::sort(audioFiles.begin(), audioFiles.end());
    assetFilesScanned = true;
    app.assets().forgetFailures(); // una imagen nueva puede ser la que faltaba
}

// --- Selector de entidades (campos Entity) ---------------------------------------------

bool Editor::drawEntityPicker(const char* label, Entity current, Entity& out) {
    World& world = app.world();
    out = current;
    bool changed = false;
    auto nameOf = [&](Entity e) { return entityLabel(e) + "  #" + std::to_string(e); };

    std::string preview = current == NullEntity ? "(none)"
                        : world.isAlive(current) ? nameOf(current)
                        : "(missing: #" + std::to_string(current) + ")";
    if (ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::IsWindowAppearing()) {
            entityFilter.clear();
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "filter...", &entityFilter);
        if (ImGui::Selectable("(none)", current == NullEntity)) { out = NullEntity; changed = true; }
        // Sangria segun la profundidad en la Jerarquia (el World ya esta en ese orden
        // despues de cualquier cambio de jerarquia).
        const std::string filter = toLower(entityFilter);
        for (Entity e : world.entities()) {
            std::string name = nameOf(e);
            if (!filter.empty() && toLower(name).find(filter) == std::string::npos) continue;
            int depth = 0;
            for (Entity p = parentOf(world, e); p != NullEntity; p = parentOf(world, p)) ++depth;
            ImGui::PushID((int)e);
            if (filter.empty() && depth > 0) ImGui::Indent(depth * ImGui::GetStyle().IndentSpacing);
            if (ImGui::Selectable(name.c_str(), e == current)) { out = e; changed = true; }
            if (filter.empty() && depth > 0) ImGui::Unindent(depth * ImGui::GetStyle().IndentSpacing);
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Pick from the list, or drop here an entity dragged from the Hierarchy");
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD)) {
            out = *static_cast<const Entity*>(payload->Data);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}

bool Editor::drawFilePicker(const char* label, const std::string& current, std::string& out, bool audio) {
    bool changed = false;
    ImGui::PushID(label);

    // [ruta][...] (y en los sonidos [>], para escucharlo) etiqueta
    const ImGuiStyle& style = ImGui::GetStyle();
    float buttonW = ImGui::CalcTextSize("...").x + style.FramePadding.x * 2.0f;
    float playW = audio ? ImGui::GetFrameHeight() + style.ItemInnerSpacing.x : 0.0f;
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - buttonW - playW - style.ItemInnerSpacing.x);
    out = current;
    if (ImGui::InputText("##path", &out)) changed = true;
    if (!current.empty()) ImGui::SetItemTooltip("%s", current.c_str());
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    if (ImGui::Button("...")) {
        if (!assetFilesScanned) scanAssetFiles();
        ImGui::OpenPopup("##pick");
    }
    if (audio) {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::BeginDisabled(current.empty());
        if (ImGui::Button(">", ImVec2(ImGui::GetFrameHeight(), 0))) app.audio().playSound(current);
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Listen");
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);

    if (ImGui::BeginPopup("##pick")) {
        ImGui::SetNextItemWidth(320.0f * uiScale);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##filter", "filter...", &fileFilter);
        ImGui::SameLine();
        if (ImGui::Button("Rescan")) scanAssetFiles();

        std::string filter = toLower(fileFilter);
        const std::vector<std::string>& files = audio ? audioFiles : textureFiles;
        if (ImGui::BeginChild("##files", ImVec2(560.0f * uiScale, 320.0f * uiScale))) {
            if (files.empty()) ImGui::TextDisabled(audio ? "No sounds in assets/" : "No images in assets/");
            int row = 0;
            for (const std::string& file : files) {
                if (!filter.empty() && toLower(file).find(filter) == std::string::npos) continue;
                ImGui::PushID(row++);
                if (audio) {
                    // Escuchar antes de elegir.
                    if (ImGui::SmallButton(">")) app.audio().playSound(file);
                    ImGui::SameLine();
                }
                if (ImGui::Selectable(file.c_str(), file == current)) {
                    out = file;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                // Imagenes: vista previa al pasar el raton.
                if (!audio && ImGui::IsItemHovered()) {
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
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}
