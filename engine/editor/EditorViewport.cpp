// The editor's central view: "Scene" tab (the editor's free camera, with
// selection and dragging) and "Game" tab (what the player sees). Each view is drawn
// on its own render texture and ImGui shows it as an image.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <vector>

#include "app/Application.h"
#include "app/GameScreen.h"
#include "components/CoreComponents.h"
#include "components/MotionComponents.h"
#include "components/PhysicsComponents.h"
#include "components/PresentationComponents.h"
#include "platform/AssetManager.h"
#include "platform/GameCanvas.h"
#include "systems/Physics.h"
#include "systems/Tilemaps.h"

static const float MIN_ZOOM = 0.1f;
static const float MAX_ZOOM = 16.0f;
static const float DRAG_THRESHOLD = 3.0f; // screen px before a click becomes a drag
static const float ICON_SIZE = 14.0f;     // screen px of the marker for entities without a sprite

static ImVec2 toIm(Vec2 v) { return ImVec2(v.x, v.y); }

// --- Render textures -------------------------------------------------------------------

SDL_Texture* Editor::ensureTarget(SDL_Texture*& tex, int& curW, int& curH, int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (tex && curW == w && curH == h) return tex;
    if (tex) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(app.renderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!tex) {
        SDL_Log("Editor: could not create the view texture: %s", SDL_GetError());
        curW = curH = 0;
        return nullptr;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST); // pixel art: no smoothing
    curW = w;
    curH = h;
    return tex;
}

// --- Tabs ------------------------------------------------------------------------------

void Editor::drawViewport() {
    if (!ImGui::BeginTabBar("##views")) return;

    ImGuiTabItemFlags sceneFlags = wantSceneTab ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags gameFlags = wantGameTab ? ImGuiTabItemFlags_SetSelected : 0;
    wantSceneTab = wantGameTab = false;

    mouseInScene = false;
    if (ImGui::BeginTabItem("Scene", nullptr, sceneFlags)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        drawSceneView({ pos.x, pos.y }, { size.x, size.y });
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Game", nullptr, gameFlags)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        drawGameView({ pos.x, pos.y }, { size.x, size.y });
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

// --- Scene view ------------------------------------------------------------------------

void Editor::drawSceneView(Vec2 pos, Vec2 size) {
    // First the invisible button that covers the view: it provides the "hover" and
    // captures the mouse, so a drag that leaves the view is not taken by another panel.
    ImGui::InvisibleButton("##scene", toIm(size),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();

    sceneView.width = (int)size.x;
    sceneView.height = (int)size.y;
    handleSceneInput(pos, hovered);

    // A prefab dragged from the Prefabs panel: it is placed where it is dropped.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PREFAB")) {
            ImVec2 m = ImGui::GetIO().MousePos;
            Vec2 p = sceneView.screenToWorld({ m.x - pos.x, m.y - pos.y });
            if (snapToGrid && !ImGui::GetIO().KeyAlt) {
                p.x = std::round(p.x / gridSize) * gridSize;
                p.y = std::round(p.y / gridSize) * gridSize;
            }
            placePrefab(static_cast<const char*>(payload->Data), p);
        }
        ImGui::EndDragDropTarget();
    }

    // Then the scene is drawn with the view already updated.
    SDL_Texture* target = ensureTarget(sceneTarget, sceneTargetW, sceneTargetH, (int)size.x, (int)size.y);
    if (!target) return;
    SDL_Renderer* r = app.renderer();
    SDL_SetRenderTarget(r, target);
    SDL_SetRenderDrawColor(r, 48, 48, 56, 255);
    SDL_RenderClear(r);
    renderWorld(app.world(), app.assets(), r, sceneView);
    // The HUD, over the game camera's frame: where the player will see it.
    renderScreenTexts(app.world(), app.assets(), r, hudView());
    drawGizmos();
    SDL_SetRenderTarget(r, nullptr);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = toIm(pos);
    ImVec2 p1(pos.x + size.x, pos.y + size.y);
    dl->AddImage((ImTextureID)(intptr_t)target, p0, p1);

    // View data in the corner: zoom, world point under the mouse and, while painting,
    // the tilemap cell.
    char info[160];
    int len = SDL_snprintf(info, sizeof(info), "zoom %d%%", (int)std::lround(sceneView.zoom * 100.0f));
    if (mouseInScene)
        len += SDL_snprintf(info + len, sizeof(info) - len, "   x %.0f  y %.0f", mouseWorld.x, mouseWorld.y);
    if (mouseInScene && hoverCell)
        SDL_snprintf(info + len, sizeof(info) - len, "   cell %d, %d", hoverCol, hoverRow);
    ImVec2 tp(p0.x + 8.0f * uiScale, p0.y + 6.0f * uiScale);
    dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 200), info);
    dl->AddText(tp, IM_COL32(230, 230, 230, 255), info);

    const char* help = "Click: select / drag   Right or middle click: pan   Wheel: zoom   F: focus";
    if (tileTool == TileTool::Brush)
        help = "Click: paint   Alt+click: pick tile   Right or middle click: pan   V: back to Move";
    else if (tileTool == TileTool::Erase)
        help = "Click: erase cells   Alt+click: pick tile   Right or middle click: pan   V: back to Move";
    else if (tileTool == TileTool::Fill)
        help = "Click: fill area   Alt+click: pick tile   Right or middle click: pan   V: back to Move";
    ImVec2 hs = ImGui::CalcTextSize(help);
    ImVec2 hp(p0.x + 8.0f * uiScale, p1.y - hs.y - 6.0f * uiScale);
    dl->AddText(ImVec2(hp.x + 1, hp.y + 1), IM_COL32(0, 0, 0, 200), help);
    dl->AddText(hp, IM_COL32(200, 200, 200, 180), help);
}

void Editor::handleSceneInput(Vec2 imagePos, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    World& world = app.world();
    Vec2 local{ io.MousePos.x - imagePos.x, io.MousePos.y - imagePos.y };
    mouseWorld = sceneView.screenToWorld(local);
    mouseInScene = hovered;

    // Wheel: zoom toward the cursor (the point under the mouse stays still).
    if (hovered && io.MouseWheel != 0.0f) {
        Vec2 before = mouseWorld;
        sceneView.zoom *= std::pow(1.15f, io.MouseWheel);
        sceneView.zoom = std::clamp(sceneView.zoom, MIN_ZOOM, MAX_ZOOM);
        Vec2 after = sceneView.screenToWorld(local);
        sceneView.center.x += before.x - after.x;
        sceneView.center.y += before.y - after.y;
        mouseWorld = before;
    }

    // Right or middle click drag: pan the view.
    if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
        panning = true;
    if (panning) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            sceneView.center.x -= io.MouseDelta.x / sceneView.zoom;
            sceneView.center.y -= io.MouseDelta.y / sceneView.zoom;
        } else {
            panning = false;
        }
    }

    // With a tile tool, the left click paints instead of selecting.
    if (handleTilemapInput(hovered)) return;

    // Left click: select the topmost entity under the mouse (or none).
    // A HUD text is dragged in game screen px, not world px.
    const View hud = hudView();
    auto pointerFor = [&](Entity e) { return isHudEntity(e) ? hud.screenToWorld(local) : mouseWorld; };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Entity picked = pickAt(mouseWorld);
        pressOnEntity = pressKeepsSelection = false;
        if (io.KeyCtrl || io.KeyShift) {
            // Ctrl or Shift: add it to or remove it from the selection (no dragging).
            toggleSelected(picked);
        } else {
            // Pressing one of several selected keeps them all, to drag them together;
            // a click without dragging leaves only that one (on release).
            syncSelection();
            if (isSelected(picked) && selection.size() > 1) {
                selected = picked;
                pressKeepsSelection = true;
            } else {
                selectOnly(picked);
            }
            if (const Transform* t = world.get<Transform>(picked)) {
                pressOnEntity = true;
                pressMouse = { io.MousePos.x, io.MousePos.y };
                Vec2 pointer = pointerFor(picked);
                dragOffset = { pointer.x - t->position.x, pointer.y - t->position.y };
            }
        }
    }

    // Dragging: starts after passing the threshold (a click without moving moves nothing).
    if (pressOnEntity) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !world.isAlive(selected)) {
            if (pressKeepsSelection && !dragging) selectOnly(selected);
            pressOnEntity = dragging = pressKeepsSelection = false;
        } else {
            if (!dragging) {
                float dx = io.MousePos.x - pressMouse.x, dy = io.MousePos.y - pressMouse.y;
                if (dx * dx + dy * dy > DRAG_THRESHOLD * DRAG_THRESHOLD) {
                    beginChange();
                    dragging = true;
                }
            }
            if (dragging) {
                Vec2 pointer = pointerFor(selected);
                Vec2 p{ pointer.x - dragOffset.x, pointer.y - dragOffset.y };
                // Snap to the grid unless Alt is held.
                if (snapToGrid && !io.KeyAlt) {
                    p.x = std::round(p.x / gridSize) * gridSize;
                    p.y = std::round(p.y / gridSize) * gridSize;
                }
                if (Transform* t = world.get<Transform>(selected)) {
                    // Every selected branch moves by the same amount (the children go
                    // with the parent). HUD texts and world entities are in different
                    // units: only the ones of the same kind as the dragged one move.
                    const Vec2 delta{ p.x - t->position.x, p.y - t->position.y };
                    const bool hud = isHudEntity(selected);
                    for (Entity root : selectedRoots()) {
                        Transform* rt = world.get<Transform>(root);
                        if (!rt || isHudEntity(root) != hud) continue;
                        rt->position.x += delta.x;
                        rt->position.y += delta.y;
                        moveBranch(root, delta);
                    }
                    // In case its root did not carry it (a HUD text under a world parent,
                    // or a child while playing).
                    t->position = p;
                }
            }
        }
    }
}

bool Editor::entityBounds(Entity e, Vec2& center, Vec2& size) {
    World& world = app.world();
    const Transform* t = world.get<Transform>(e);
    if (!t) return false;
    if (const Tilemap* map = world.get<Tilemap>(e)) {
        // The whole map (its Transform is the corner, not the center).
        Vec2 min, max;
        tilemapBounds(*map, *t, min, max);
        center = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        size = { max.x - min.x, max.y - min.y };
        return true;
    }
    if (!world.has<Sprite>(e) && textBounds(world, app.assets(), e, center, size)) {
        if (!isHudEntity(e)) return true;
        // HUD text: its rectangle on the game screen, taken to the world through the
        // Scene view (so it matches where it is drawn).
        const View hud = hudView();
        Vec2 a = sceneView.screenToWorld(hud.worldToScreen({ center.x - size.x * 0.5f, center.y - size.y * 0.5f }));
        Vec2 b = sceneView.screenToWorld(hud.worldToScreen({ center.x + size.x * 0.5f, center.y + size.y * 0.5f }));
        center = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        size = { b.x - a.x, b.y - a.y };
        return true;
    }
    center = t->position;
    if (!spriteWorldSize(world, app.assets(), e, size)) {
        // Without a sprite: a marker with a fixed screen size.
        float s = ICON_SIZE * uiScale / sceneView.zoom;
        size = { s, s };
    }
    return true;
}

Entity Editor::pickAt(Vec2 p) {
    World& world = app.world();
    Entity best = NullEntity;
    int bestOrder = INT_MIN;
    for (Entity e : world.entities()) {
        Vec2 c, s;
        if (!entityBounds(e, c, s)) continue;
        if (p.x < c.x - s.x * 0.5f || p.x > c.x + s.x * 0.5f ||
            p.y < c.y - s.y * 0.5f || p.y > c.y + s.y * 0.5f)
            continue;
        // The one drawn on top wins: higher order and, on a tie, the newest. Those
        // without a sprite (cameras, points) go above everything, like their markers.
        // A tilemap is only selected by its painted cells: the air between the
        // platforms does not hide what is behind.
        int order = INT_MAX;
        if (const Tilemap* map = world.get<Tilemap>(e)) {
            int col, row;
            if (!map->visible || !tilemapWorldToCell(*map, *world.get<Transform>(e), p, col, row) ||
                map->cells.get(col, row) < 0)
                continue;
            order = map->order;
        } else if (const Sprite* sp = world.get<Sprite>(e)) {
            if (sp->visible && !sp->texture.empty()) order = sp->order;
        } else if (const Text* tx = world.get<Text>(e)) {
            // The HUD is drawn above the world: it also wins when selecting.
            if (tx->visible) order = tx->screenSpace ? INT_MAX - 1 : tx->order;
        }
        if (best == NullEntity || order > bestOrder || (order == bestOrder && e > best)) {
            best = e;
            bestOrder = order;
        }
    }
    return best;
}

void Editor::drawGizmos() {
    SDL_Renderer* r = app.renderer();
    World& world = app.world();
    const View& v = sceneView;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // Grid (only if the lines are not too close together) and the origin axes. While
    // painting tiles, the tilemap's grid is shown instead (drawTilemapGizmos).
    if (showGrid && tileTool == TileTool::Move && gridSize * v.zoom >= 6.0f) {
        Vec2 tl = v.screenToWorld({ 0, 0 });
        Vec2 br = v.screenToWorld({ (float)v.width, (float)v.height });
        SDL_SetRenderDrawColor(r, 255, 255, 255, 18);
        for (float x = std::floor(tl.x / gridSize) * gridSize; x <= br.x; x += gridSize) {
            float sx = std::round(v.worldToScreen({ x, 0 }).x);
            SDL_RenderLine(r, sx, 0, sx, (float)v.height);
        }
        for (float y = std::floor(tl.y / gridSize) * gridSize; y <= br.y; y += gridSize) {
            float sy = std::round(v.worldToScreen({ 0, y }).y);
            SDL_RenderLine(r, 0, sy, (float)v.width, sy);
        }
    }
    Vec2 o = v.worldToScreen({ 0, 0 });
    SDL_SetRenderDrawColor(r, 220, 70, 70, 120);
    SDL_RenderLine(r, 0, std::round(o.y), (float)v.width, std::round(o.y));
    SDL_SetRenderDrawColor(r, 70, 200, 70, 120);
    SDL_RenderLine(r, std::round(o.x), 0, std::round(o.x), (float)v.height);

    // Cameras: the frame of what the player will see (game resolution / zoom).
    const AppConfig& cfg = app.config();
    // When selected, a FollowCamera also shows its dead zone.
    world.each<Camera, Transform>([&](Entity e, Camera& cam, Transform& t) {
        float z = cam.zoom > 0.0f ? cam.zoom : 1.0f;
        Vec2 a = v.worldToScreen({ t.position.x - cfg.gameWidth * 0.5f / z, t.position.y - cfg.gameHeight * 0.5f / z });
        Vec2 b = v.worldToScreen({ t.position.x + cfg.gameWidth * 0.5f / z, t.position.y + cfg.gameHeight * 0.5f / z });
        SDL_FRect rect{ a.x, a.y, b.x - a.x, b.y - a.y };
        SDL_SetRenderDrawColor(r, 255, 210, 80, 200);
        SDL_RenderRect(r, &rect);
        const FollowCamera* follow = world.get<FollowCamera>(e);
        if (follow && e == selected) {
            Vec2 da = v.worldToScreen({ t.position.x - follow->deadZone.x * 0.5f, t.position.y - follow->deadZone.y * 0.5f });
            Vec2 db = v.worldToScreen({ t.position.x + follow->deadZone.x * 0.5f, t.position.y + follow->deadZone.y * 0.5f });
            SDL_FRect dz{ da.x, da.y, db.x - da.x, db.y - da.y };
            SDL_SetRenderDrawColor(r, 255, 210, 80, 90);
            SDL_RenderRect(r, &dz);
        }
    });

    // Tilemaps: solid cells, grid and brush preview.
    drawTilemapGizmos();

    // Collision boxes: green the solid ones, yellow the triggers. The selection's one
    // is always shown.
    world.each<BoxCollider, Transform>([&](Entity e, BoxCollider& box, Transform&) {
        if (!showColliders && !isSelected(e)) return;
        Vec2 min, max;
        if (!colliderBounds(world, e, min, max)) return;
        Vec2 a = v.worldToScreen(min), b = v.worldToScreen(max);
        SDL_FRect rect{ std::round(a.x), std::round(a.y), std::round(b.x - a.x), std::round(b.y - a.y) };
        if (box.isTrigger) SDL_SetRenderDrawColor(r, 255, 220, 60, 220);
        else SDL_SetRenderDrawColor(r, 90, 255, 120, 220);
        SDL_RenderRect(r, &rect);
    });

    // PathMover paths: the selection's always, the others with "Colliders".
    // While playing, the points are measured from where it started (the entity has already moved).
    world.each<PathMover, Transform>([&](Entity e, PathMover& m, Transform& t) {
        if ((!showColliders && !isSelected(e)) || m.points.empty()) return;
        const Vec2 origin = m.started ? m.origin : t.position;
        std::vector<SDL_FPoint> line;
        line.push_back({ std::round(v.worldToScreen(origin).x), std::round(v.worldToScreen(origin).y) });
        for (const PathPoint& p : m.points) {
            Vec2 s = v.worldToScreen({ origin.x + p.offset.x, origin.y + p.offset.y });
            line.push_back({ std::round(s.x), std::round(s.y) });
        }
        if (m.loop) line.push_back(line.front());
        SDL_SetRenderDrawColor(r, 120, 200, 255, isSelected(e) ? 230 : 120);
        SDL_RenderLines(r, line.data(), (int)line.size());
        for (const SDL_FPoint& p : line) {
            SDL_FRect dot{ p.x - 3, p.y - 3, 6, 6 };
            SDL_RenderFillRect(r, &dot);
        }
    });

    // Entities without a sprite or tilemap: a diamond, so they can be seen and selected.
    for (Entity e : world.entities()) {
        const Transform* t = world.get<Transform>(e);
        if (!t || world.has<Tilemap>(e)) continue;
        Vec2 size, textCenter;
        if (spriteWorldSize(world, app.assets(), e, size)) continue;
        if (textBounds(world, app.assets(), e, textCenter, size)) continue; // its rectangle is already visible
        Vec2 c = v.worldToScreen(t->position);
        float h = ICON_SIZE * uiScale * 0.5f;
        SDL_FPoint pts[5] = { { c.x, c.y - h }, { c.x + h, c.y }, { c.x, c.y + h }, { c.x - h, c.y }, { c.x, c.y - h } };
        SDL_SetRenderDrawColor(r, 180, 180, 255, 220);
        SDL_RenderLines(r, pts, 5);
    }

    // Selection: frame and cross at the Transform's position (the center of a sprite,
    // the corner of a tilemap). The active one is brighter than the rest.
    syncSelection();
    for (Entity e : selection) {
        Vec2 c, s;
        if (!entityBounds(e, c, s)) continue;
        Vec2 a = v.worldToScreen({ c.x - s.x * 0.5f, c.y - s.y * 0.5f });
        Vec2 b = v.worldToScreen({ c.x + s.x * 0.5f, c.y + s.y * 0.5f });
        SDL_FRect rect{ std::round(a.x) - 1, std::round(a.y) - 1, std::round(b.x - a.x) + 2, std::round(b.y - a.y) + 2 };
        SDL_SetRenderDrawColor(r, 80, 220, 255, e == selected ? 255 : 150);
        SDL_RenderRect(r, &rect);
        const Vec2 anchor = world.get<Transform>(e)->position;
        Vec2 m = isHudEntity(e) ? hudView().worldToScreen(anchor) : v.worldToScreen(anchor);
        SDL_RenderLine(r, m.x - 5, m.y, m.x + 5, m.y);
        SDL_RenderLine(r, m.x, m.y - 5, m.x, m.y + 5);
    }
}

bool Editor::isHudEntity(Entity e) {
    const Text* tx = app.world().get<Text>(e);
    return tx && tx->screenSpace && !app.world().has<Sprite>(e);
}

View Editor::hudView() {
    // Game screen -> world (through the game's camera) -> Scene view, in a single
    // View: a screen point s falls in the world at (s - half) / zc + cc, and from
    // there into the Scene view. Solving it, it is a View with zoom zs / zc.
    const AppConfig& cfg = app.config();
    View cam;
    findCameraView(app.world(), cfg.gameWidth, cfg.gameHeight, cam);
    View hud;
    hud.zoom = sceneView.zoom / cam.zoom;
    hud.center = { cfg.gameWidth * 0.5f - (cam.center.x - sceneView.center.x) * cam.zoom,
                   cfg.gameHeight * 0.5f - (cam.center.y - sceneView.center.y) * cam.zoom };
    hud.width = sceneView.width;
    hud.height = sceneView.height;
    return hud;
}

void Editor::frameEntity(Entity e) {
    Vec2 c, s;
    if (entityBounds(e, c, s)) sceneView.center = c;
}

void Editor::centerOnCamera() {
    View v;
    if (findCameraView(app.world(), 1, 1, v)) sceneView.center = v.center;
    else sceneView.center = { 0, 0 };
}

// --- Game view -------------------------------------------------------------------------

void Editor::drawGameView(Vec2 pos, Vec2 size) {
    const AppConfig& cfg = app.config();
    if (!gameCanvas) gameCanvas = std::make_unique<GameCanvas>(app.renderer());

    // The game according to the chosen scale (see app/GameScreen.h), centered with bars.
    // The edges are in whole px: with "Pixel perfect" the image already measures what is
    // drawn, and half a pixel of offset would smooth it again.
    GameScreen screen;
    const bool ok = drawGameScreen(app.world(), app.assets(), app.renderer(), *gameCanvas, cfg.gameWidth,
                                   cfg.gameHeight, cfg.scale, size.x, size.y, screen);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin(std::round(pos.x), std::round(pos.y));
    dl->AddRectFilled(origin, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(0, 0, 0, 255));
    if (!ok) return;
    ImVec2 p0(origin.x + screen.dstX, origin.y + screen.dstY);
    ImVec2 p1(p0.x + screen.dstW, p0.y + screen.dstH);
    ImVec2 uv0(screen.srcX / screen.textureW, screen.srcY / screen.textureH);
    ImVec2 uv1((screen.srcX + screen.srcW) / screen.textureW, (screen.srcY + screen.srcH) / screen.textureH);
    dl->AddImage((ImTextureID)(intptr_t)screen.texture, p0, p1, uv0, uv1);

    // Information at the top left and the scale selector at the top right.
    char info[128];
    int percent = (int)std::lround(screen.scale * 100.0f);
    if (cfg.scale == GameScale::PixelPerfect && screen.hasCamera)
        SDL_snprintf(info, sizeof(info), "%dx%d  (%d%%, art pixel = %g px)", cfg.gameWidth, cfg.gameHeight,
                     percent, screen.pixel);
    else
        SDL_snprintf(info, sizeof(info), "%dx%d  (%d%%)%s", cfg.gameWidth, cfg.gameHeight, percent,
                     screen.hasCamera ? "" : "   no Camera in the scene");
    ImVec2 tp(std::fmax(p0.x, origin.x) + 8.0f * uiScale, std::fmax(p0.y, origin.y) + 6.0f * uiScale);
    dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 200), info);
    dl->AddText(tp, IM_COL32(230, 230, 230, 255), info);

    const float comboW = 150.0f * uiScale;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x - comboW - 8.0f * uiScale, tp.y - 2.0f * uiScale));
    ImGui::SetNextItemWidth(comboW);
    if (ImGui::BeginCombo("##gameScale", gameScaleLabel(cfg.scale))) {
        for (GameScale mode : { GameScale::PixelPerfect, GameScale::Fit, GameScale::Actual }) {
            if (ImGui::Selectable(gameScaleLabel(mode), mode == cfg.scale)) app.setGameScale(mode);
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("How the game is shown when the tab is not the size of its resolution:\n"
                          "Pixel perfect: each art pixel is a whole number of px (Camera.pixelSize);\n"
                          "  a bit more or less of the world is visible.\n"
                          "Fit: the whole image scaled (blurry when shrunk).\n"
                          "100%%: actual size, cropped if it does not fit.");
}
