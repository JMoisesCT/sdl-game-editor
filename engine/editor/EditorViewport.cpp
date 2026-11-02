// Vista central del editor: pestanas "Escena" (camara libre del editor, con
// seleccion y arrastre) y "Juego" (lo que ve el jugador). Cada vista se dibuja en su
// propia textura de render y ImGui la muestra como una imagen.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <climits>
#include <cmath>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "components/PhysicsComponents.h"
#include "components/PresentationComponents.h"
#include "platform/AssetManager.h"
#include "platform/GameCanvas.h"
#include "systems/Physics.h"
#include "systems/Tilemaps.h"

static const float MIN_ZOOM = 0.1f;
static const float MAX_ZOOM = 16.0f;
static const float DRAG_THRESHOLD = 3.0f; // px de pantalla antes de que un clic sea arrastre
static const float ICON_SIZE = 14.0f;     // px de pantalla del marcador de entidades sin sprite

static ImVec2 toIm(Vec2 v) { return ImVec2(v.x, v.y); }

// --- Texturas de render ----------------------------------------------------------------

SDL_Texture* Editor::ensureTarget(SDL_Texture*& tex, int& curW, int& curH, int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (tex && curW == w && curH == h) return tex;
    if (tex) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(app.renderer(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!tex) {
        SDL_Log("Editor: no se pudo crear la textura de la vista: %s", SDL_GetError());
        curW = curH = 0;
        return nullptr;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST); // pixel art: sin suavizado
    curW = w;
    curH = h;
    return tex;
}

// --- Pestanas --------------------------------------------------------------------------

void Editor::drawViewport() {
    if (!ImGui::BeginTabBar("##views")) return;

    ImGuiTabItemFlags sceneFlags = wantSceneTab ? ImGuiTabItemFlags_SetSelected : 0;
    ImGuiTabItemFlags gameFlags = wantGameTab ? ImGuiTabItemFlags_SetSelected : 0;
    wantSceneTab = wantGameTab = false;

    mouseInScene = false;
    if (ImGui::BeginTabItem("Escena", nullptr, sceneFlags)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        drawSceneView({ pos.x, pos.y }, { size.x, size.y });
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Juego", nullptr, gameFlags)) {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        drawGameView({ pos.x, pos.y }, { size.x, size.y });
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

// --- Vista de escena -------------------------------------------------------------------

void Editor::drawSceneView(Vec2 pos, Vec2 size) {
    // Primero el boton invisible que ocupa la vista: da el "hover" y captura el raton,
    // asi un arrastre que sale de la vista no se lo queda otro panel.
    ImGui::InvisibleButton("##scene", toIm(size),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                           ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();

    sceneView.width = (int)size.x;
    sceneView.height = (int)size.y;
    handleSceneInput(pos, hovered);

    // Un prefab arrastrado desde el panel Prefabs: se coloca donde se suelta.
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

    // Despues se dibuja la escena con la vista ya actualizada.
    SDL_Texture* target = ensureTarget(sceneTarget, sceneTargetW, sceneTargetH, (int)size.x, (int)size.y);
    if (!target) return;
    SDL_Renderer* r = app.renderer();
    SDL_SetRenderTarget(r, target);
    SDL_SetRenderDrawColor(r, 48, 48, 56, 255);
    SDL_RenderClear(r);
    renderWorld(app.world(), app.assets(), r, sceneView);
    // El HUD, sobre el recuadro de la camara del juego: donde lo vera el jugador.
    renderScreenTexts(app.world(), app.assets(), r, hudView());
    drawGizmos();
    SDL_SetRenderTarget(r, nullptr);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = toIm(pos);
    ImVec2 p1(pos.x + size.x, pos.y + size.y);
    dl->AddImage((ImTextureID)(intptr_t)target, p0, p1);

    // Datos de la vista en la esquina: zoom, punto del mundo bajo el raton y, pintando,
    // la celda del tilemap.
    char info[160];
    int len = SDL_snprintf(info, sizeof(info), "zoom %d%%", (int)std::lround(sceneView.zoom * 100.0f));
    if (mouseInScene)
        len += SDL_snprintf(info + len, sizeof(info) - len, "   x %.0f  y %.0f", mouseWorld.x, mouseWorld.y);
    if (mouseInScene && hoverCell)
        SDL_snprintf(info + len, sizeof(info) - len, "   celda %d, %d", hoverCol, hoverRow);
    ImVec2 tp(p0.x + 8.0f * uiScale, p0.y + 6.0f * uiScale);
    dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 200), info);
    dl->AddText(tp, IM_COL32(230, 230, 230, 255), info);

    const char* help = "Clic: elegir / arrastrar   Clic derecho o medio: mover vista   Rueda: zoom   F: centrar";
    if (tileTool == TileTool::Brush)
        help = "Clic: pintar   Alt+clic: tomar el tile   Clic derecho o medio: mover vista   V: volver a mover";
    else if (tileTool == TileTool::Erase)
        help = "Clic: vaciar celdas   Alt+clic: tomar el tile   Clic derecho o medio: mover vista   V: volver a mover";
    else if (tileTool == TileTool::Fill)
        help = "Clic: rellenar la zona   Alt+clic: tomar el tile   Clic derecho o medio: mover vista   V: volver a mover";
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

    // Rueda: zoom hacia el cursor (el punto bajo el raton se queda quieto).
    if (hovered && io.MouseWheel != 0.0f) {
        Vec2 before = mouseWorld;
        sceneView.zoom *= std::pow(1.15f, io.MouseWheel);
        sceneView.zoom = std::clamp(sceneView.zoom, MIN_ZOOM, MAX_ZOOM);
        Vec2 after = sceneView.screenToWorld(local);
        sceneView.center.x += before.x - after.x;
        sceneView.center.y += before.y - after.y;
        mouseWorld = before;
    }

    // Clic derecho o medio arrastrado: mover la vista.
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

    // Con una herramienta de tiles, el clic izquierdo pinta en vez de elegir.
    if (handleTilemapInput(hovered)) return;

    // Clic izquierdo: elegir la entidad de arriba bajo el raton (o ninguna).
    // Un texto del HUD se arrastra en px de la pantalla del juego, no del mundo.
    const View hud = hudView();
    auto pointerFor = [&](Entity e) { return isHudEntity(e) ? hud.screenToWorld(local) : mouseWorld; };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selected = pickAt(mouseWorld);
        pressOnEntity = false;
        if (const Transform* t = world.get<Transform>(selected)) {
            pressOnEntity = true;
            pressMouse = { io.MousePos.x, io.MousePos.y };
            Vec2 pointer = pointerFor(selected);
            dragOffset = { pointer.x - t->position.x, pointer.y - t->position.y };
        }
    }

    // Arrastre: empieza al pasar el umbral (un clic sin mover no mueve nada).
    if (pressOnEntity) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !world.isAlive(selected)) {
            pressOnEntity = dragging = false;
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
                // Ajustar a la grilla salvo con Alt pulsado.
                if (snapToGrid && !io.KeyAlt) {
                    p.x = std::round(p.x / gridSize) * gridSize;
                    p.y = std::round(p.y / gridSize) * gridSize;
                }
                if (Transform* t = world.get<Transform>(selected)) t->position = p;
            }
        }
    }
}

bool Editor::entityBounds(Entity e, Vec2& center, Vec2& size) {
    World& world = app.world();
    const Transform* t = world.get<Transform>(e);
    if (!t) return false;
    if (const Tilemap* map = world.get<Tilemap>(e)) {
        // El mapa entero (su Transform es la esquina, no el centro).
        Vec2 min, max;
        tilemapBounds(*map, *t, min, max);
        center = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };
        size = { max.x - min.x, max.y - min.y };
        return true;
    }
    if (!world.has<Sprite>(e) && textBounds(world, app.assets(), e, center, size)) {
        if (!isHudEntity(e)) return true;
        // Texto del HUD: su rectangulo en la pantalla del juego, llevado al mundo a
        // traves de la vista Escena (asi coincide con donde se dibuja).
        const View hud = hudView();
        Vec2 a = sceneView.screenToWorld(hud.worldToScreen({ center.x - size.x * 0.5f, center.y - size.y * 0.5f }));
        Vec2 b = sceneView.screenToWorld(hud.worldToScreen({ center.x + size.x * 0.5f, center.y + size.y * 0.5f }));
        center = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        size = { b.x - a.x, b.y - a.y };
        return true;
    }
    center = t->position;
    if (!spriteWorldSize(world, app.assets(), e, size)) {
        // Sin sprite: un marcador de tamano fijo en pantalla.
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
        // Gana la que se dibuja encima: mayor order y, a igualdad, la mas nueva. Las
        // que no tienen sprite (camaras, puntos) van por encima de todo, como sus marcas.
        // Un tilemap solo se elige por sus celdas pintadas: el aire entre las
        // plataformas no tapa lo que hay detras.
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
            // El HUD se dibuja encima del mundo: tambien gana al elegir.
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

    // Grilla (solo si las lineas no quedan pegadas) y ejes del origen. Pintando tiles
    // se ve la grilla del tilemap en su lugar (drawTilemapGizmos).
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

    // Camaras: el recuadro de lo que vera el jugador (resolucion del juego / zoom).
    const AppConfig& cfg = app.config();
    // Elegida, una FollowCamera muestra tambien su zona muerta.
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

    // Tilemaps: celdas solidas, grilla y vista previa del pincel.
    drawTilemapGizmos();

    // Cajas de colision: verde las solidas, amarillo los triggers. La de la seleccion
    // se ve siempre.
    world.each<BoxCollider, Transform>([&](Entity e, BoxCollider& box, Transform&) {
        if (!showColliders && e != selected) return;
        Vec2 min, max;
        if (!colliderBounds(world, e, min, max)) return;
        Vec2 a = v.worldToScreen(min), b = v.worldToScreen(max);
        SDL_FRect rect{ std::round(a.x), std::round(a.y), std::round(b.x - a.x), std::round(b.y - a.y) };
        if (box.isTrigger) SDL_SetRenderDrawColor(r, 255, 220, 60, 220);
        else SDL_SetRenderDrawColor(r, 90, 255, 120, 220);
        SDL_RenderRect(r, &rect);
    });

    // Entidades sin sprite ni tilemap: un rombo, para poder verlas y elegirlas.
    for (Entity e : world.entities()) {
        const Transform* t = world.get<Transform>(e);
        if (!t || world.has<Tilemap>(e)) continue;
        Vec2 size, textCenter;
        if (spriteWorldSize(world, app.assets(), e, size)) continue;
        if (textBounds(world, app.assets(), e, textCenter, size)) continue; // su rectangulo ya se ve
        Vec2 c = v.worldToScreen(t->position);
        float h = ICON_SIZE * uiScale * 0.5f;
        SDL_FPoint pts[5] = { { c.x, c.y - h }, { c.x + h, c.y }, { c.x, c.y + h }, { c.x - h, c.y }, { c.x, c.y - h } };
        SDL_SetRenderDrawColor(r, 180, 180, 255, 220);
        SDL_RenderLines(r, pts, 5);
    }

    // Seleccion: recuadro y cruz en la posicion del Transform (el centro de un sprite,
    // la esquina de un tilemap).
    Vec2 c, s;
    if (entityBounds(selected, c, s)) {
        Vec2 a = v.worldToScreen({ c.x - s.x * 0.5f, c.y - s.y * 0.5f });
        Vec2 b = v.worldToScreen({ c.x + s.x * 0.5f, c.y + s.y * 0.5f });
        SDL_FRect rect{ std::round(a.x) - 1, std::round(a.y) - 1, std::round(b.x - a.x) + 2, std::round(b.y - a.y) + 2 };
        SDL_SetRenderDrawColor(r, 80, 220, 255, 255);
        SDL_RenderRect(r, &rect);
        const Vec2 anchor = world.get<Transform>(selected)->position;
        Vec2 m = isHudEntity(selected) ? hudView().worldToScreen(anchor) : v.worldToScreen(anchor);
        SDL_RenderLine(r, m.x - 5, m.y, m.x + 5, m.y);
        SDL_RenderLine(r, m.x, m.y - 5, m.x, m.y + 5);
    }
}

bool Editor::isHudEntity(Entity e) {
    const Text* tx = app.world().get<Text>(e);
    return tx && tx->screenSpace && !app.world().has<Sprite>(e);
}

View Editor::hudView() {
    // Pantalla del juego -> mundo (por la camara del juego) -> vista Escena, en una
    // sola View: un punto s de la pantalla cae en el mundo en (s - mitad) / zc + cc, y
    // de ahi en la vista Escena. Despejando, es una View de zoom zs / zc.
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

// --- Vista de juego --------------------------------------------------------------------

void Editor::drawGameView(Vec2 pos, Vec2 size) {
    const AppConfig& cfg = app.config();
    if (!gameCanvas) gameCanvas = std::make_unique<GameCanvas>(app.renderer());
    if (!gameCanvas->begin(cfg.gameWidth, cfg.gameHeight)) return;

    View view;
    Color bg;
    bool hasCamera = findCameraView(app.world(), cfg.gameWidth, cfg.gameHeight, view, &bg);
    SDL_Renderer* r = app.renderer();
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
    SDL_RenderClear(r);
    renderWorld(app.world(), app.assets(), r, view);
    renderScreenTexts(app.world(), app.assets(), r, screenView(cfg.gameWidth, cfg.gameHeight));

    // La resolucion del juego, escalada para que quepa entera (con bandas). El tamano
    // se redondea antes de pedir la textura: es el que se va a dibujar de verdad.
    float k = std::min(size.x / cfg.gameWidth, size.y / cfg.gameHeight);
    if (k <= 0.0f) {
        SDL_SetRenderTarget(r, nullptr);
        return;
    }
    float w = std::round(cfg.gameWidth * k), h = std::round(cfg.gameHeight * k);
    SDL_Texture* target = gameCanvas->scaledFor(w, h);
    if (!target) return;
    ImVec2 p0(std::round(pos.x + (size.x - w) * 0.5f), std::round(pos.y + (size.y - h) * 0.5f));
    ImVec2 p1(p0.x + w, p0.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(toIm(pos), ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(0, 0, 0, 255));
    dl->AddImage((ImTextureID)(intptr_t)target, p0, p1);

    char info[96];
    SDL_snprintf(info, sizeof(info), "%dx%d  (%d%%)%s", cfg.gameWidth, cfg.gameHeight, (int)std::lround(k * 100.0f),
                 hasCamera ? "" : "   sin Camera en la escena");
    ImVec2 tp(p0.x + 8.0f * uiScale, p0.y + 6.0f * uiScale);
    dl->AddText(ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 200), info);
    dl->AddText(tp, IM_COL32(230, 230, 230, 255), info);
}
