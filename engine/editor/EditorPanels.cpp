// The editor's Hierarchy and Inspector panels. The Inspector knows no component: it
// walks the fields each one declared to the reflection and draws one widget per type.

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

// Prefab instances in the Hierarchy (the same color as in EditorPrefabs.cpp) and
// fields where an instance differs from its prefab.
static const ImVec4 PREFAB_COLOR(0.45f, 0.75f, 1.0f, 1.0f);
static const ImVec4 OVERRIDE_COLOR(1.0f, 0.72f, 0.3f, 1.0f);
// Type of the drag of an entity from the Hierarchy (onto another entity or onto an
// Entity field in the Inspector). The payload is the Entity.
static const char* ENTITY_PAYLOAD = "ENTITY";

// --- Hierarchy -------------------------------------------------------------------------

std::string Editor::entityLabel(Entity e) {
    const EntityInfo* info = app.world().get<EntityInfo>(e);
    if (info && !info->name.empty()) return info->name;
    return "Entity " + std::to_string(e);
}

// The sibling that follows e, skipping the ones in 'skip', or NullEntity if it is the last one.
static Entity nextSibling(const World& world, Entity e, const std::vector<Entity>& skip) {
    std::vector<Entity> siblings = childrenOf(world, parentOf(world, e));
    auto it = std::find(siblings.begin(), siblings.end(), e);
    if (it == siblings.end()) return NullEntity;
    for (++it; it != siblings.end(); ++it)
        if (std::find(skip.begin(), skip.end(), *it) == skip.end()) return *it;
    return NullEntity;
}

// --- Selection -------------------------------------------------------------------------

void Editor::syncSelection() {
    const World& world = app.world();
    if (!world.isAlive(selected)) selected = NullEntity;
    if (selected == NullEntity) {
        selection.clear();
    } else if (std::find(selection.begin(), selection.end(), selected) == selection.end()) {
        selection.assign(1, selected);
    } else {
        selection.erase(std::remove_if(selection.begin(), selection.end(),
                                       [&](Entity e) { return !world.isAlive(e); }),
                        selection.end());
    }
}

bool Editor::isSelected(Entity e) const {
    if (e == NullEntity) return false;
    if (e == selected) return true;
    // Until syncSelection runs, a list without the active entity is stale: it no longer counts.
    return std::find(selection.begin(), selection.end(), selected) != selection.end() &&
           std::find(selection.begin(), selection.end(), e) != selection.end();
}

void Editor::selectOnly(Entity e) {
    selected = e;
    selection.clear();
    if (e != NullEntity) selection.push_back(e);
}

void Editor::toggleSelected(Entity e) {
    syncSelection();
    if (e == NullEntity) return;
    auto it = std::find(selection.begin(), selection.end(), e);
    if (it == selection.end()) {
        selection.push_back(e);
        selected = e;
    } else {
        selection.erase(it);
        if (selected == e) selected = selection.empty() ? NullEntity : selection.back();
    }
}

std::vector<Entity> Editor::selectedRoots() {
    syncSelection();
    const World& world = app.world();
    std::vector<Entity> roots;
    for (Entity e : world.entities()) {
        if (!isSelected(e)) continue;
        bool nested = false;
        for (Entity p = parentOf(world, e); p != NullEntity && !nested; p = parentOf(world, p))
            nested = isSelected(p);
        if (!nested) roots.push_back(e);
    }
    return roots;
}

// --- Hierarchy panel -------------------------------------------------------------------

void Editor::drawHierarchy() {
    World& world = app.world();
    syncSelection();

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
    ImGui::SetItemTooltip("Ctrl+click: add to or remove from the selection.\n"
                          "Shift+click: select a range.\n"
                          "Ctrl+G: put the selection into a new group.\n"
                          "Drag an entity onto another: it becomes its child.\n"
                          "Onto the top or bottom edge: places it before or after.\n"
                          "Into the empty space below: takes it out of its group.\n"
                          "Dragging a selected entity takes the whole selection.\n"
                          "Moving a parent also moves its children (only while editing).");
    ImGui::Separator();

    // Actions are applied after drawing the tree: deleting, duplicating or moving
    // inside it would change the list being walked.
    enum class Action { None, Duplicate, Delete, Group, Ungroup, CreateChild, Move };
    struct HierarchyAction {
        Action kind = Action::None;
        Entity e = NullEntity, parent = NullEntity, before = NullEntity;
    } act;

    // Parent -> children, in World order (NullEntity = the roots).
    std::unordered_map<Entity, std::vector<Entity>> children;
    for (Entity e : world.entities()) children[parentOf(world, e)].push_back(e);

    // If the selection changed (e.g. it was picked in the view), its branch is opened
    // and brought into view once.
    const bool reveal = selected != revealedSelection && world.isAlive(selected);
    std::unordered_set<Entity> toOpen;
    if (reveal)
        for (Entity p = parentOf(world, selected); p != NullEntity; p = parentOf(world, p)) toOpen.insert(p);

    // The rows in the order they are drawn (without the ones inside closed branches):
    // a Shift+click range is taken from here.
    std::vector<Entity> rows;
    // What a drag takes: the dragged entity alone or, if it is selected, the whole
    // selection (each branch once, in World order).
    auto draggedSet = [&](Entity dragged) {
        if (isSelected(dragged) && selection.size() > 1) return selectedRoots();
        return std::vector<Entity>{ dragged };
    };

    const ImU32 dropColor = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
    std::function<void(Entity)> drawNode = [&](Entity e) {
        rows.push_back(e);
        auto kids = children.find(e);
        const bool leaf = kids == children.end();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_DefaultOpen;
        if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (isSelected(e)) flags |= ImGuiTreeNodeFlags_Selected;
        if (toOpen.count(e)) ImGui::SetNextItemOpen(true);

        std::string label = entityLabel(e) + "###e" + std::to_string(e);
        const PrefabInstance* link = world.get<PrefabInstance>(e);
        if (link) ImGui::PushStyleColor(ImGuiCol_Text, PREFAB_COLOR);
        const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (link) ImGui::PopStyleColor();

        // A click selects (except on the arrow, which opens or closes), but on RELEASE and
        // only if there was no drag: that way an entity can be dragged to an Entity field in
        // the Inspector without the Inspector switching to the dragged one, and a drag
        // from one of several selected takes them all. Double-click focuses.
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

        // Dragging: onto another entity in the Hierarchy or onto an Entity field in the
        // Inspector. The payload is the dragged entity; the rest of the selection is
        // added when it is dropped in the Hierarchy.
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(ENTITY_PAYLOAD, &e, sizeof(Entity));
            const size_t count = draggedSet(e).size();
            if (count > 1) ImGui::Text("%s and %d more", entityLabel(e).c_str(), (int)count - 1);
            else ImGui::TextUnformatted(entityLabel(e).c_str());
            ImGui::EndDragDropSource();
        }
        // Dropping: the top quarter = before, the bottom one = after, the middle = child.
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                ENTITY_PAYLOAD, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
            Entity dragged = payload ? *static_cast<const Entity*>(payload->Data) : NullEntity;
            const std::vector<Entity> moving = payload ? draggedSet(dragged) : std::vector<Entity>{};
            // An entity cannot be dropped onto itself or onto its own branch.
            bool valid = payload != nullptr;
            for (Entity m : moving)
                if (m == e || isAncestorOf(world, m, e)) valid = false;
            if (valid) {
                ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
                float y = ImGui::GetIO().MousePos.y, quarter = (r1.y - r0.y) * 0.25f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                Entity parent = parentOf(world, e), before = NullEntity;
                if (y < r0.y + quarter) {
                    before = e;
                    dl->AddLine(ImVec2(r0.x, r0.y), ImVec2(r1.x, r0.y), dropColor, 2.0f);
                } else if (y > r1.y - quarter) {
                    before = nextSibling(world, e, moving);
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
            // Right-click on one of the selected keeps the selection; on another one,
            // it becomes the only one.
            if (!isSelected(e)) selectOnly(e);
            const std::vector<Entity> roots = selectedRoots();
            bool anyInGroup = false;
            for (Entity r : roots) anyInGroup = anyInGroup || parentOf(world, r) != NullEntity;
            if (roots.size() > 1) ImGui::TextDisabled("%d selected", (int)roots.size());
            else if (ImGui::MenuItem("Create child")) act = { Action::CreateChild, e };
            if (ImGui::MenuItem("Group", "Ctrl+G")) act = { Action::Group };
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) act = { Action::Duplicate };
            if (ImGui::MenuItem("Delete", "Del")) act = { Action::Delete };
            if (anyInGroup && ImGui::MenuItem("Remove from group")) act = { Action::Ungroup };
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

        // The empty space below: dropping there takes the entity (or the selection) out
        // of its group (last among the roots); a click clears the selection.
        ImVec2 rest = ImGui::GetContentRegionAvail();
        ImGui::Dummy(ImVec2(std::max(rest.x, 1.0f), std::max(rest.y, ImGui::GetFrameHeight())));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ENTITY_PAYLOAD))
                act = { Action::Move, *static_cast<const Entity*>(payload->Data), NullEntity, NullEntity };
            ImGui::EndDragDropTarget();
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered())
            selectOnly(NullEntity);
    }
    ImGui::EndChild();

    // Selecting on release (see above): plain click = only this one, Ctrl = add or
    // remove it, Shift = the range from the last plain or Ctrl click (the anchor).
    if (hierarchyPress != NullEntity && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();
        const bool dragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= io.MouseDragThreshold * io.MouseDragThreshold;
        const Entity e = hierarchyPress;
        hierarchyPress = NullEntity;
        if (!dragged && world.isAlive(e)) {
            auto from = std::find(rows.begin(), rows.end(), selectionAnchor);
            auto to = std::find(rows.begin(), rows.end(), e);
            if (io.KeyShift && from != rows.end() && to != rows.end()) {
                if (from > to) std::swap(from, to);
                if (!io.KeyCtrl) selection.clear(); // Ctrl+Shift adds the range to what there was
                for (auto it = from; it != to + 1; ++it)
                    if (std::find(selection.begin(), selection.end(), *it) == selection.end()) selection.push_back(*it);
                selected = e;
            } else if (io.KeyCtrl) {
                toggleSelected(e);
                selectionAnchor = e;
            } else {
                selectOnly(e);
                selectionAnchor = e;
            }
        }
    }
    revealedSelection = selected;

    switch (act.kind) {
    case Action::Duplicate:   duplicateSelected(); break;
    case Action::Delete:      deleteSelected(); break;
    case Action::Group:       groupSelected(); break;
    case Action::Ungroup:     ungroupSelected(); break;
    case Action::CreateChild: createEntity(act.e); break;
    case Action::Move:
        if (world.isAlive(act.e)) {
            const bool wasSelected = isSelected(act.e);
            moveEntities(draggedSet(act.e), act.parent, act.before);
            if (!wasSelected) selectOnly(act.e); // dragging an unselected entity selects it
        }
        break;
    case Action::None: break;
    }
}

void Editor::moveEntities(const std::vector<Entity>& entities, Entity parent, Entity before) {
    World& world = app.world();
    // Dropping where they already were is not a change (nor an undo step).
    const bool wasPending = changePending;
    const std::vector<Entity> oldOrder = world.entities();
    std::vector<Entity> oldParents;
    for (Entity e : entities) oldParents.push_back(parentOf(world, e));
    beginChange();
    // Each one goes right before the same 'before': they end up in the order of the list.
    for (Entity e : entities)
        if (world.isAlive(e)) moveInHierarchy(world, e, parent, before);
    bool same = world.entities() == oldOrder;
    for (size_t i = 0; same && i < entities.size(); ++i) same = parentOf(world, entities[i]) == oldParents[i];
    if (!wasPending && same) cancelChange();
}

Entity Editor::createEntity(Entity parent) {
    World& world = app.world();
    beginChange();
    Entity e = world.create();
    world.add<EntityInfo>(e).name = "Entity " + std::to_string(e);
    Transform t;
    const Transform* pt = world.get<Transform>(parent);
    if (pt) {
        // A child is born where its parent is.
        t.position = pt->position;
    } else {
        // It appears at the center of the editor view (on the grid, if snapping).
        t.position = sceneView.center;
        if (snapToGrid) {
            t.position.x = std::round(t.position.x / gridSize) * gridSize;
            t.position.y = std::round(t.position.y / gridSize) * gridSize;
        }
    }
    world.add<Transform>(e, t);
    if (world.isAlive(parent)) moveInHierarchy(world, e, parent);
    selectOnly(e);
    selectionAnchor = e;
    return e;
}

void Editor::duplicateSelected() {
    const std::vector<Entity> roots = selectedRoots();
    if (roots.empty()) return;
    beginChange();
    // The copies become the selection; the copy of the active one stays active.
    std::vector<Entity> copies;
    Entity active = NullEntity;
    for (Entity root : roots) {
        Entity copy = duplicateBranch(root);
        copies.push_back(copy);
        if (root == selected) active = copy;
    }
    selection = copies;
    selected = active != NullEntity ? active : copies.back();
    selectionAnchor = selected;
}

Entity Editor::duplicateBranch(Entity root) {
    World& world = app.world();
    const ComponentRegistry& registry = app.registry();

    // The entity and its whole branch. Copying = serializing and deserializing into new
    // entities: it copies exactly what is saved in the file, no more and no less (an
    // instance stays linked).
    std::vector<Entity> branch = descendantsOf(world, root);
    branch.insert(branch.begin(), root);
    std::unordered_map<Entity, Entity> copyOf; // original -> copy
    for (Entity src : branch) copyOf[src] = world.create();
    for (Entity src : branch) {
        Entity dst = copyOf[src];
        Json copy = serializeEntity(world, registry, &app.prefabs(), src);
        copy.erase("parent"); // the copy's one is set below
        deserializeEntity(world, registry, &app.prefabs(), dst, copy);
        if (src != root) setParent(world, dst, copyOf[parentOf(world, src)]);
        // What pointed inside the branch now points to the copy; what points outside
        // keeps pointing to the same thing.
        forEachEntityField(registry, world, dst, [&](Entity& ref) {
            auto it = copyOf.find(ref);
            if (it != copyOf.end()) ref = it->second;
        });
        if (Transform* t = world.get<Transform>(dst)) {
            t->position.x += gridSize; // shifted one step, so it is visible that there are two
            t->position.y += gridSize;
        }
    }
    Entity copyRoot = copyOf[root];
    if (EntityInfo* info = world.get<EntityInfo>(copyRoot)) info->name += " (copy)";
    // Right after the original, among its siblings.
    moveInHierarchy(world, copyRoot, parentOf(world, root), nextSibling(world, root, { copyRoot }));
    return copyRoot;
}

void Editor::deleteSelected() {
    const std::vector<Entity> roots = selectedRoots();
    if (roots.empty()) return;
    World& world = app.world();
    beginChange();
    for (Entity root : roots) {
        for (Entity d : descendantsOf(world, root)) world.destroyNow(d);
        world.destroyNow(root);
    }
    selectOnly(NullEntity);
}

void Editor::groupSelected() {
    const std::vector<Entity> roots = selectedRoots();
    if (roots.empty()) return;
    World& world = app.world();
    beginChange();

    // The group takes the place of the first one (same parent, same spot among its
    // siblings), and its Transform goes to the middle of the grouped entities, so its
    // marker shows where the group is. HUD texts are in screen px: they do not count.
    Vec2 sum{ 0.0f, 0.0f };
    int count = 0;
    for (Entity r : roots) {
        const Transform* t = world.get<Transform>(r);
        if (!t || isHudEntity(r)) continue;
        sum.x += t->position.x;
        sum.y += t->position.y;
        ++count;
    }
    Entity group = world.create();
    world.add<EntityInfo>(group).name = "Group";
    Transform t;
    t.position = count > 0 ? Vec2{ std::round(sum.x / count), std::round(sum.y / count) } : sceneView.center;
    world.add<Transform>(group, t);
    moveInHierarchy(world, group, parentOf(world, roots.front()), roots.front());
    for (Entity r : roots) moveInHierarchy(world, r, group);

    // Selected, so it can be renamed right away in the Inspector.
    selectOnly(group);
    selectionAnchor = group;
    setStatus("Grouped " + std::to_string(roots.size()) + (roots.size() == 1 ? " entity" : " entities"));
}

void Editor::ungroupSelected() {
    const std::vector<Entity> roots = selectedRoots();
    World& world = app.world();
    // Each one goes right after its current group, in the group's parent. The spots are
    // worked out before moving anything, so the ones from the same group keep their order.
    struct Move { Entity e, parent, before; };
    std::vector<Move> moves;
    for (Entity r : roots) {
        Entity group = parentOf(world, r);
        if (group != NullEntity) moves.push_back({ r, parentOf(world, group), nextSibling(world, group, roots) });
    }
    if (moves.empty()) return;
    beginChange();
    for (const Move& m : moves) moveInHierarchy(world, m.e, m.parent, m.before);
}

void Editor::moveBranch(Entity e, Vec2 delta) {
    // Not while playing: children do not follow the parent in the game, and not in the editor either.
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
    // With several selected, the Inspector shows (and edits) only the active one.
    if (selection.size() > 1)
        ImGui::TextDisabled("%d selected: editing only this one", (int)selection.size());
    drawPrefabHeader();
    ImGui::Separator();

    // If it is a prefab instance, what differs from it (the way it is saved in the
    // scene): those fields are highlighted.
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
            // Moving the parent from the Inspector also moves its children.
            Transform* moved = info.typeId == componentTypeId<Transform>() ? static_cast<Transform*>(component) : nullptr;
            const Vec2 before = moved ? moved->position : Vec2{};
            auto changed = overrides.find(info.name); // without a prefab, overrides is null: end()
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

    // Exceptions to the generic Inspector: the prefab header (on top) and the palette
    // to paint a Tilemap.
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

// "6, 7 ,8" -> {6, 7, 8}. Anything that is not a number acts as a separator.
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

// Each widget edits a COPY of the value; only if it changed is beginChange called
// (which saves the scene as it was) and then the component is written.
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
        // Text with the numbers separated by commas: "6, 7, 8".
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
        // Only the size (the cells are edited with the palette). InputInt2 applies on
        // confirm (Enter or leaving the field), not on every key: typing "30" would go
        // through "3", and shrinking erases what falls outside.
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
        // The help goes on the list header (inside, each field has its own).
        return drawList(f, value);
    }

    if (!f.tooltip.empty()) ImGui::SetItemTooltip("%s", f.tooltip.c_str());
    return changed;
}

// List of structs: a collapsible header per element, with its fields inside and the
// buttons to reorder, duplicate and remove. Actions are applied after walking
// the list (changing it midway would move the elements being drawn).
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
        // The title is the element's first text field (the clip's name).
        std::string title = "#" + std::to_string(i);
        for (const FieldInfo& ef : f.elementFields)
            if (ef.type == FieldType::String) {
                const std::string& s = *static_cast<const std::string*>(ef.in(static_cast<const void*>(item)));
                if (!s.empty()) title += "  " + s;
                break;
            }
        title += "###item";

        // The buttons to the right of the header: drawn afterwards, on the same line.
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

// Dropdown with the suggested values ("" shows as "(none)"). A value that is
// not in the list (typed by hand in the file) is still shown, flagged.
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

// --- File picker (images and sounds) ---------------------------------------------------

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
        std::string path = it->path().generic_string(); // with "/" on Windows too
        if (ext == ".png") textureFiles.push_back(path);
        else if (isAudioFile(ext)) audioFiles.push_back(path);
    }
    std::sort(textureFiles.begin(), textureFiles.end());
    std::sort(audioFiles.begin(), audioFiles.end());
    assetFilesScanned = true;
    app.assets().forgetFailures(); // a new image may be the one that was missing
}

// --- Entity picker (Entity fields) -----------------------------------------------------

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
        // Indentation by depth in the Hierarchy (the World is already in that order
        // after any hierarchy change).
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

    // [path][...] (and for sounds [>], to listen to it) label
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
                    // Listen before choosing.
                    if (ImGui::SmallButton(">")) app.audio().playSound(file);
                    ImGui::SameLine();
                }
                if (ImGui::Selectable(file.c_str(), file == current)) {
                    out = file;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                // Images: preview on hover.
                if (!audio && ImGui::IsItemHovered()) {
                    if (SDL_Texture* tex = app.assets().loadTexture(file)) {
                        float tw = 0, th = 0;
                        SDL_GetTextureSize(tex, &tw, &th);
                        float k = std::min(1.0f, 256.0f * uiScale / std::max(tw, th));
                        k = std::max(k, std::floor(64.0f * uiScale / std::max(tw, th))); // small ones, enlarged
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
