// Tilemap editing: the tileset palette (in the Inspector) and the tools to paint in
// the scene view. Maps are built ONLY here: the cells are saved in the scene (there
// is no separate map file or external editor).
//
// Painting ALWAYS goes to the selected layer (the selected entity). Every cell
// change goes through setCell, which calls beginChange before writing: a whole stroke
// (from press to release) is a single undo step.

#include "editor/Editor.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "app/Application.h"
#include "components/CoreComponents.h"
#include "platform/AssetManager.h"
#include "systems/Tilemaps.h"

// --- Selection and tool ----------------------------------------------------------------

Tilemap* Editor::selectedTilemap() {
    World& world = app.world();
    if (!world.has<Transform>(selected)) return nullptr;
    return world.get<Tilemap>(selected);
}

void Editor::setTileTool(TileTool tool) {
    if (tool != TileTool::Move && !selectedTilemap()) {
        setStatus("To paint, select an entity with a Tilemap");
        return;
    }
    tileTool = tool;
    painting = false;
}

// --- Palette ---------------------------------------------------------------------------

void Editor::drawTilePalette() {
    Tilemap* map = selectedTilemap();
    if (!map) return;

    ImGui::Spacing();
    if (!ImGui::CollapsingHeader("Paint tiles", ImGuiTreeNodeFlags_DefaultOpen)) return;

    // Tools: the active one's button looks pressed.
    auto toolButton = [&](const char* label, TileTool tool, const char* tip) {
        bool active = tileTool == tool;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label)) setTileTool(tool);
        if (active) ImGui::PopStyleColor();
        ImGui::SetItemTooltip("%s", tip);
        ImGui::SameLine();
    };
    toolButton("Move", TileTool::Move, "V: select and drag entities");
    toolButton("Brush", TileTool::Brush, "B: paints the tiles picked in the palette");
    toolButton("Eraser", TileTool::Erase, "E: clears cells");
    toolButton("Fill", TileTool::Fill, "G: fills the area of the same tile with the picked ones");
    ImGui::NewLine();

    ImGui::Checkbox("Show solids", &showSolids);
    ImGui::SetItemTooltip("Highlights the blocking cells in the view");
    ImGui::SameLine();
    ImGui::Checkbox("Mark solids", &markSolids);
    ImGui::SetItemTooltip("Click a tile in the palette to mark or unmark it as solid");

    int columns = 0, rows = 0;
    if (!tilesetGrid(app.assets(), *map, columns, rows)) {
        ImGui::TextDisabled("Pick the tileset (the image) in the 'tileset'");
        ImGui::TextDisabled("field of the Tilemap component.");
        return;
    }
    SDL_Texture* tex = app.assets().loadTexture(map->tileset);
    float texW = 0, texH = 0;
    SDL_GetTextureSize(tex, &texW, &texH);

    // The tileset may have changed: the brush cannot be left outside the image.
    brush.x = std::clamp(brush.x, 0, columns - 1);
    brush.y = std::clamp(brush.y, 0, rows - 1);
    brush.w = std::clamp(brush.w, 1, columns - brush.x);
    brush.h = std::clamp(brush.h, 1, rows - brush.y);

    ImGui::SetNextItemWidth(120.0f * uiScale);
    ImGui::SliderInt("zoom##palette", &paletteZoom, 1, 6, "%dx");

    // The image goes in a child with scrollbars: a large tileset can be scrolled without
    // enlarging the panel.
    const float zoom = (float)paletteZoom;
    const float cellW = map->tileWidth * zoom, cellH = map->tileHeight * zoom;
    const ImVec2 imageSize(columns * cellW, rows * cellH);
    const ImGuiStyle& style = ImGui::GetStyle();
    float childH = std::min(imageSize.y + style.ScrollbarSize + style.WindowPadding.y * 2.0f, 420.0f * uiScale);
    if (ImGui::BeginChild("##palette", ImVec2(0, childH), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##tiles", imageSize);
        bool hovered = ImGui::IsItemHovered();
        ImVec2 p1(p0.x + imageSize.x, p0.y + imageSize.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 36, 255));
        // Only the part of the image that forms whole tiles.
        ImVec2 uv1(columns * map->tileWidth / texW, rows * map->tileHeight / texH);
        dl->AddImage((ImTextureID)(intptr_t)tex, p0, p1, ImVec2(0, 0), uv1);

        if (cellW >= 6.0f) {
            for (int c = 1; c < columns; ++c)
                dl->AddLine(ImVec2(p0.x + c * cellW, p0.y), ImVec2(p0.x + c * cellW, p1.y), IM_COL32(255, 255, 255, 30));
            for (int r = 1; r < rows; ++r)
                dl->AddLine(ImVec2(p0.x, p0.y + r * cellH), ImVec2(p1.x, p0.y + r * cellH), IM_COL32(255, 255, 255, 30));
        }
        // Solid tiles: reddish with a border.
        for (int tile : map->solidTiles) {
            if (tile < 0 || tile >= columns * rows) continue;
            ImVec2 a(p0.x + tile % columns * cellW, p0.y + tile / columns * cellH);
            ImVec2 b(a.x + cellW, a.y + cellH);
            dl->AddRectFilled(a, b, IM_COL32(255, 60, 60, 70));
            dl->AddRect(a, b, IM_COL32(255, 80, 80, 200));
        }
        // What the brush carries.
        ImVec2 sa(p0.x + brush.x * cellW, p0.y + brush.y * cellH);
        ImVec2 sb(sa.x + brush.w * cellW, sa.y + brush.h * cellH);
        dl->AddRect(sa, sb, IM_COL32(0, 0, 0, 255), 0.0f, 0, 3.0f * uiScale);
        dl->AddRect(sa, sb, IM_COL32(80, 220, 255, 255), 0.0f, 0, 1.5f * uiScale);

        ImVec2 mouse = ImGui::GetIO().MousePos;
        int mc = std::clamp((int)std::floor((mouse.x - p0.x) / cellW), 0, columns - 1);
        int mr = std::clamp((int)std::floor((mouse.y - p0.y) / cellH), 0, rows - 1);
        int tile = mr * columns + mc;

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (markSolids) {
                beginChange();
                auto it = std::find(map->solidTiles.begin(), map->solidTiles.end(), tile);
                if (it != map->solidTiles.end()) map->solidTiles.erase(it);
                else {
                    map->solidTiles.push_back(tile);
                    std::sort(map->solidTiles.begin(), map->solidTiles.end());
                }
            } else {
                // Pressing and dragging picks a rectangle of tiles (a "stamp").
                paletteSelecting = true;
                paletteStartCol = mc;
                paletteStartRow = mr;
                if (tileTool == TileTool::Move || tileTool == TileTool::Erase) tileTool = TileTool::Brush;
            }
        }
        if (paletteSelecting) {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) paletteSelecting = false;
            brush.x = std::min(paletteStartCol, mc);
            brush.y = std::min(paletteStartRow, mr);
            brush.w = std::abs(mc - paletteStartCol) + 1;
            brush.h = std::abs(mr - paletteStartRow) + 1;
        }
        if (hovered) {
            bool solid = isSolidTile(*map, tile);
            ImGui::SetTooltip("tile %d%s", tile, solid ? "  (solid)" : "");
        }
    }
    ImGui::EndChild();

    if (brush.w == 1 && brush.h == 1)
        ImGui::Text("Brush: tile %d", brush.y * columns + brush.x);
    else
        ImGui::Text("Brush: %d x %d tiles", brush.w, brush.h);
    ImGui::TextDisabled("Drag in the palette to pick several.");
    ImGui::TextDisabled("Alt+click in the view picks a cell's tile.");
}

// --- Painting in the view --------------------------------------------------------------

bool Editor::handleTilemapInput(bool hovered) {
    hoverCell = false;
    if (tileTool == TileTool::Move) return false;
    Tilemap* map = selectedTilemap();
    if (!map) return false;
    const Transform& t = *app.world().get<Transform>(selected);

    int col, row;
    tilemapWorldToCell(*map, t, mouseWorld, col, row);
    hoverCell = hovered;
    hoverCol = col;
    hoverRow = row;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Eyedropper: that cell's tile becomes the brush.
        if (ImGui::GetIO().KeyAlt) {
            int columns, rows;
            int tile = map->cells.get(col, row);
            if (tile >= 0 && tilesetGrid(app.assets(), *map, columns, rows) && tile < columns * rows) {
                brush = { tile % columns, tile / columns, 1, 1 };
                tileTool = TileTool::Brush;
            }
            return true;
        }
        if (tileTool == TileTool::Fill) {
            fillFrom(*map, col, row);
            return true;
        }
        painting = true;
        lastCol = col;
        lastRow = row;
        paintCell(*map, col, row);
        return true;
    }

    if (painting) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            painting = false;
        } else if (col != lastCol || row != lastRow) {
            // The mouse may jump several cells in one frame: every cell on the line between
            // the previous and the current one is painted (Bresenham), with no gaps in the stroke.
            int x = lastCol, y = lastRow;
            int dx = std::abs(col - x), sx = x < col ? 1 : -1;
            int dy = -std::abs(row - y), sy = y < row ? 1 : -1;
            int err = dx + dy;
            while (x != col || y != row) {
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x += sx; }
                if (e2 <= dx) { err += dx; y += sy; }
                paintCell(*map, x, y);
            }
            lastCol = col;
            lastRow = row;
        }
    }
    return true;
}

void Editor::setCell(Tilemap& map, int col, int row, int tile) {
    if (!map.cells.inside(col, row) || map.cells.get(col, row) == tile) return;
    beginChange();
    map.cells.set(col, row, tile);
}

void Editor::paintCell(Tilemap& map, int col, int row) {
    if (tileTool == TileTool::Erase) {
        setCell(map, col, row, -1);
        return;
    }
    int columns, rows;
    if (!tilesetGrid(app.assets(), map, columns, rows)) return;
    // The whole stamp, with its top left corner on the cell under the mouse.
    for (int dy = 0; dy < brush.h; ++dy)
        for (int dx = 0; dx < brush.w; ++dx)
            setCell(map, col + dx, row + dy, (brush.y + dy) * columns + brush.x + dx);
}

void Editor::fillFrom(Tilemap& map, int col, int row) {
    int columns, rows;
    if (!map.cells.inside(col, row) || !tilesetGrid(app.assets(), map, columns, rows)) return;

    // The area: the neighboring cells (no diagonals) with the same tile as the pressed one.
    const int target = map.cells.get(col, row);
    std::vector<char> seen(map.cells.cells.size(), 0);
    std::vector<std::pair<int, int>> pending{ { col, row } }, zone;
    seen[(std::size_t)row * map.cells.width + col] = 1;
    while (!pending.empty()) {
        auto [x, y] = pending.back();
        pending.pop_back();
        zone.push_back({ x, y });
        const int nx[4] = { x + 1, x - 1, x, x }, ny[4] = { y, y, y + 1, y - 1 };
        for (int i = 0; i < 4; ++i) {
            if (!map.cells.inside(nx[i], ny[i]) || map.cells.get(nx[i], ny[i]) != target) continue;
            char& s = seen[(std::size_t)ny[i] * map.cells.width + nx[i]];
            if (!s) { s = 1; pending.push_back({ nx[i], ny[i] }); }
        }
    }
    // With a multi-tile stamp, the area is covered by repeating it as a pattern
    // that starts at the pressed cell.
    auto wrap = [](int v, int n) { return ((v % n) + n) % n; };
    for (auto [x, y] : zone) {
        int dx = wrap(x - col, brush.w), dy = wrap(y - row, brush.h);
        setCell(map, x, y, (brush.y + dy) * columns + brush.x + dx);
    }
}

// --- Visual aids in the scene view -----------------------------------------------------

void Editor::drawTilemapGizmos() {
    SDL_Renderer* r = app.renderer();
    World& world = app.world();
    const View& v = sceneView;

    // Solid cells of every layer with collision, only the visible ones.
    if (showSolids) {
        SDL_SetRenderDrawColor(r, 255, 50, 50, 90);
        world.each<Tilemap, Transform>([&](Entity, Tilemap& map, Transform& t) {
            if (!map.collision || map.solidTiles.empty()) return;
            int c0, r0, c1, r1;
            tilemapWorldToCell(map, t, v.screenToWorld({ 0, 0 }), c0, r0);
            tilemapWorldToCell(map, t, v.screenToWorld({ (float)v.width, (float)v.height }), c1, r1);
            for (int row = std::max(r0, 0); row <= std::min(r1, map.cells.height - 1); ++row)
                for (int col = std::max(c0, 0); col <= std::min(c1, map.cells.width - 1); ++col) {
                    if (!isSolidCell(map, col, row)) continue;
                    Vec2 a = v.worldToScreen(tilemapCellOrigin(map, t, col, row));
                    Vec2 b = v.worldToScreen(tilemapCellOrigin(map, t, col + 1, row + 1));
                    SDL_FRect rect{ std::round(a.x), std::round(a.y), std::round(b.x) - std::round(a.x),
                                    std::round(b.y) - std::round(a.y) };
                    SDL_RenderFillRect(r, &rect);
                }
        });
    }

    if (tileTool == TileTool::Move) return;
    Tilemap* map = selectedTilemap();
    if (!map) return;
    const Transform& t = *world.get<Transform>(selected);

    // The layer's grid (if the cells are not too small).
    Vec2 cell = tilemapCellSize(*map, t);
    Vec2 min, max;
    tilemapBounds(*map, t, min, max);
    Vec2 a = v.worldToScreen(min), b = v.worldToScreen(max);
    if (cell.x * v.zoom >= 6.0f && cell.y * v.zoom >= 6.0f) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 28);
        for (int c = 1; c < map->cells.width; ++c) {
            float x = std::round(v.worldToScreen(tilemapCellOrigin(*map, t, c, 0)).x);
            SDL_RenderLine(r, x, a.y, x, b.y);
        }
        for (int row = 1; row < map->cells.height; ++row) {
            float y = std::round(v.worldToScreen(tilemapCellOrigin(*map, t, 0, row)).y);
            SDL_RenderLine(r, a.x, y, b.x, y);
        }
    }

    // Preview under the mouse: the semi-transparent stamp or the eraser's square.
    if (!hoverCell) return;
    int w = 1, h = 1;
    int columns, rows;
    if (tileTool == TileTool::Brush && tilesetGrid(app.assets(), *map, columns, rows)) {
        w = brush.w;
        h = brush.h;
        SDL_Texture* tex = app.assets().loadTexture(map->tileset);
        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 150);
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx) {
                if (!map->cells.inside(hoverCol + dx, hoverRow + dy)) continue;
                int tile = (brush.y + dy) * columns + brush.x + dx;
                SDL_FRect src{ (float)(tile % columns * map->tileWidth), (float)(tile / columns * map->tileHeight),
                               (float)map->tileWidth, (float)map->tileHeight };
                Vec2 ca = v.worldToScreen(tilemapCellOrigin(*map, t, hoverCol + dx, hoverRow + dy));
                Vec2 cb = v.worldToScreen(tilemapCellOrigin(*map, t, hoverCol + dx + 1, hoverRow + dy + 1));
                SDL_FRect dst{ std::round(ca.x), std::round(ca.y), std::round(cb.x) - std::round(ca.x),
                               std::round(cb.y) - std::round(ca.y) };
                SDL_RenderTexture(r, tex, &src, &dst);
            }
        SDL_SetTextureAlphaMod(tex, 255);
    }
    Vec2 ha = v.worldToScreen(tilemapCellOrigin(*map, t, hoverCol, hoverRow));
    Vec2 hb = v.worldToScreen(tilemapCellOrigin(*map, t, hoverCol + w, hoverRow + h));
    SDL_FRect rect{ std::round(ha.x), std::round(ha.y), std::round(hb.x) - std::round(ha.x),
                    std::round(hb.y) - std::round(ha.y) };
    if (tileTool == TileTool::Erase) SDL_SetRenderDrawColor(r, 255, 90, 90, 255);
    else SDL_SetRenderDrawColor(r, 80, 220, 255, 255);
    SDL_RenderRect(r, &rect);
}
