#include "systems/RenderSystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/CoreComponents.h"
#include "core/World.h"
#include "platform/AssetManager.h"
#include "systems/Tilemaps.h"

Vec2 View::worldToScreen(Vec2 w) const {
    return { (w.x - center.x) * zoom + width * 0.5f, (w.y - center.y) * zoom + height * 0.5f };
}

Vec2 View::screenToWorld(Vec2 s) const {
    return { (s.x - width * 0.5f) / zoom + center.x, (s.y - height * 0.5f) / zoom + center.y };
}

bool findCameraView(World& world, int width, int height, View& view, Color* background) {
    view = View{};
    view.width = width;
    view.height = height;
    bool found = false;
    world.each<Camera, Transform>([&](Entity, Camera& cam, Transform& t) {
        if (found) return;
        found = true;
        view.center = t.position;
        view.zoom = cam.zoom > 0.0f ? cam.zoom : 1.0f;
        if (background) *background = cam.background;
    });
    return found;
}

// Recorte efectivo: el del Sprite, o la textura completa si no tiene.
static SDL_FRect sourceRect(const Sprite& s, SDL_Texture* tex) {
    if (s.source.w > 0 && s.source.h > 0)
        return { (float)s.source.x, (float)s.source.y, (float)s.source.w, (float)s.source.h };
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    return { 0, 0, tw, th };
}

bool spriteWorldSize(World& world, AssetManager& assets, Entity e, Vec2& size) {
    const Transform* t = world.get<Transform>(e);
    const Sprite* s = world.get<Sprite>(e);
    if (!t || !s || s->texture.empty()) return false;
    SDL_Texture* tex = assets.loadTexture(s->texture);
    if (!tex) return false;
    SDL_FRect src = sourceRect(*s, tex);
    size = { src.w * std::fabs(t->scale.x), src.h * std::fabs(t->scale.y) };
    return true;
}

// Una capa de tiles: solo las celdas que se ven. Cada celda se pasa a pantalla por sus
// DOS esquinas y se redondea cada borde: el borde derecho de una celda cae en el mismo
// entero que el izquierdo de la vecina, asi no quedan costuras ni solapes con zoom.
static void drawTilemap(const Tilemap& map, const Transform& t, AssetManager& assets,
                        SDL_Renderer* renderer, const View& view) {
    int columns, rows;
    if (!tilesetGrid(assets, map, columns, rows)) return;
    SDL_Texture* tex = assets.loadTexture(map.tileset);

    // Celdas visibles: las esquinas de la vista pasadas a celdas, recortadas al mapa.
    int c0, r0, c1, r1;
    tilemapWorldToCell(map, t, view.screenToWorld({ 0, 0 }), c0, r0);
    tilemapWorldToCell(map, t, view.screenToWorld({ (float)view.width, (float)view.height }), c1, r1);
    c0 = std::max(c0, 0);
    r0 = std::max(r0, 0);
    c1 = std::min(c1, map.cells.width - 1);
    r1 = std::min(r1, map.cells.height - 1);

    SDL_SetTextureColorMod(tex, map.tint.r, map.tint.g, map.tint.b);
    SDL_SetTextureAlphaMod(tex, map.tint.a);
    const int tileCount = columns * rows;
    for (int row = r0; row <= r1; ++row) {
        for (int col = c0; col <= c1; ++col) {
            int tile = map.cells.get(col, row);
            if (tile < 0 || tile >= tileCount) continue; // vacia (o fuera del tileset)
            SDL_FRect src{ (float)(tile % columns * map.tileWidth), (float)(tile / columns * map.tileHeight),
                           (float)map.tileWidth, (float)map.tileHeight };
            Vec2 a = view.worldToScreen(tilemapCellOrigin(map, t, col, row));
            Vec2 b = view.worldToScreen(tilemapCellOrigin(map, t, col + 1, row + 1));
            float left = std::round(a.x), top = std::round(a.y);
            SDL_FRect dst{ left, top, std::round(b.x) - left, std::round(b.y) - top };
            SDL_RenderTexture(renderer, tex, &src, &dst);
        }
    }
}

void renderWorld(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view) {
    // Primero se juntan y ordenan: los pools guardan los componentes en cualquier
    // orden. Sprites y capas de tiles comparten la misma escala de order.
    struct Item { int order; Entity e; Sprite* sprite; Tilemap* tilemap; Transform* transform; };
    std::vector<Item> items;
    world.each<Sprite, Transform>([&](Entity e, Sprite& s, Transform& t) {
        if (s.visible && !s.texture.empty()) items.push_back({ s.order, e, &s, nullptr, &t });
    });
    world.each<Tilemap, Transform>([&](Entity e, Tilemap& m, Transform& t) {
        if (m.visible && !m.tileset.empty()) items.push_back({ m.order, e, nullptr, &m, &t });
    });
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.order != b.order) return a.order < b.order;
        if (a.e != b.e) return a.e < b.e;
        return a.tilemap && !b.tilemap; // misma entidad: el tilemap debajo del sprite
    });

    for (const Item& it : items) {
        if (it.tilemap) {
            drawTilemap(*it.tilemap, *it.transform, assets, renderer, view);
            continue;
        }
        const Sprite& s = *it.sprite;
        const Transform& t = *it.transform;
        SDL_Texture* tex = assets.loadTexture(s.texture);
        if (!tex) continue;

        SDL_FRect src = sourceRect(s, tex);
        float w = src.w * std::fabs(t.scale.x) * view.zoom;
        float h = src.h * std::fabs(t.scale.y) * view.zoom;
        Vec2 c = view.worldToScreen(t.position);

        // Bordes redondeados a enteros (pixel art nitido, sin costuras entre vecinos).
        float left = std::round(c.x - w * 0.5f), right = std::round(c.x + w * 0.5f);
        float top = std::round(c.y - h * 0.5f), bottom = std::round(c.y + h * 0.5f);
        if (right < 0 || bottom < 0 || left > view.width || top > view.height) continue; // fuera
        SDL_FRect dst{ left, top, right - left, bottom - top };

        // Escala negativa tambien voltea (como en Unity).
        bool fx = s.flipX != (t.scale.x < 0.0f);
        bool fy = s.flipY != (t.scale.y < 0.0f);
        SDL_FlipMode flip = (SDL_FlipMode)((fx ? SDL_FLIP_HORIZONTAL : 0) | (fy ? SDL_FLIP_VERTICAL : 0));

        // La textura es compartida: el tinte se pone en cada dibujo.
        SDL_SetTextureColorMod(tex, s.tint.r, s.tint.g, s.tint.b);
        SDL_SetTextureAlphaMod(tex, s.tint.a);
        SDL_RenderTextureRotated(renderer, tex, &src, &dst, t.rotation, nullptr, flip);
    }
}
