#include "systems/RenderSystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/CoreComponents.h"
#include "components/PresentationComponents.h"
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
    return findScaledCameraView(world, width, height, 1.0f, false, view, background);
}

bool findScaledCameraView(World& world, int width, int height, float scale, bool pixelPerfect,
                          View& view, Color* background) {
    view = View{};
    view.width = width;
    view.height = height;
    bool found = false;
    world.each<Camera, Transform>([&](Entity, Camera& cam, Transform& t) {
        if (found) return;
        found = true;
        view.zoom = (cam.zoom > 0.0f ? cam.zoom : 1.0f) * (scale > 0.0f ? scale : 1.0f);
        if (pixelPerfect) {
            // screen px per art pixel, rounded to the nearest integer.
            const float pixel = cam.pixelSize > 0.0f ? cam.pixelSize : 1.0f;
            const float screenPixels = std::fmax(1.0f, std::round(view.zoom * pixel));
            view.zoom = screenPixels / pixel;
        }
        // The center, in WHOLE screen pixels. A camera moving in fractions of a pixel
        // (the FollowCamera smoothing) would make each object round its edge on a
        // different frame: the player and the ground would shift a pixel separately and
        // everything would shake. This way everything that is still moves together.
        view.center = { std::round(t.position.x * view.zoom) / view.zoom,
                        std::round(t.position.y * view.zoom) / view.zoom };
        if (background) *background = cam.background;
    });
    return found;
}

// Effective crop: the Sprite's, or the whole texture if it has none.
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

// A tile layer: only the visible cells. Each cell is mapped to the screen by its TWO
// corners and each edge is rounded: the right edge of a cell falls on the same integer
// as the left edge of its neighbor, so there are no seams or overlaps when zoomed.
static void drawTilemap(const Tilemap& map, const Transform& t, AssetManager& assets,
                        SDL_Renderer* renderer, const View& view) {
    int columns, rows;
    if (!tilesetGrid(assets, map, columns, rows)) return;
    SDL_Texture* tex = assets.loadTexture(map.tileset);

    // Visible cells: the view's corners converted to cells, clipped to the map.
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
            if (tile < 0 || tile >= tileCount) continue; // empty (or outside the tileset)
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

// Tiled background that fills the view. The pattern is anchored to the world times
// the factor: with factor 1 each copy stays fixed in the world; with 0, fixed in the view.
static void drawParallax(const ParallaxBackground& p, AssetManager& assets, SDL_Renderer* renderer,
                         const View& view) {
    SDL_Texture* tex = assets.loadTexture(p.texture);
    if (!tex) return;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    const float tileW = tw * p.scale * view.zoom;
    const float tileH = th * p.scale * view.zoom;
    if (tileW < 2.0f || tileH < 2.0f) return; // so small it would take thousands of copies

    // Where a copy of the pattern falls in the view, brought into [-tile, 0): that way
    // the first one starts right before the edge and repeats with no gaps.
    float offX = (-view.center.x * p.factor.x + p.offset.x) * view.zoom + view.width * 0.5f;
    float offY = (-view.center.y * p.factor.y + p.offset.y) * view.zoom + view.height * 0.5f;
    offX = std::fmod(offX, tileW);
    if (offX > 0.0f) offX -= tileW;
    offY = std::fmod(offY, tileH);
    if (offY > 0.0f) offY -= tileH;

    SDL_SetTextureColorMod(tex, p.tint.r, p.tint.g, p.tint.b);
    SDL_SetTextureAlphaMod(tex, p.tint.a);
    // Whole-pixel edges, like the tilemap: half a pixel between two copies shows as a seam.
    for (float y = offY; y < view.height; y += tileH) {
        for (float x = offX; x < view.width; x += tileW) {
            float left = std::round(x), top = std::round(y);
            SDL_FRect dst{ left, top, std::round(x + tileW) - left, std::round(y + tileH) - top };
            SDL_RenderTexture(renderer, tex, nullptr, &dst);
        }
    }
}

// Shift of the text center relative to the position, according to align.
static float alignShift(const Text& tx, float width) {
    if (tx.align == "left") return width * 0.5f;
    if (tx.align == "right") return -width * 0.5f;
    return 0.0f; // center (and any unknown value)
}

bool textBounds(World& world, AssetManager& assets, Entity e, Vec2& center, Vec2& size) {
    const Transform* t = world.get<Transform>(e);
    const Text* tx = world.get<Text>(e);
    if (!t || !tx) return false;
    SDL_Texture* tex = assets.textTexture(tx->font, tx->size, tx->text);
    if (!tex) return false;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    size = { tw * std::fabs(t->scale.x), th * std::fabs(t->scale.y) };
    center = { t->position.x + alignShift(*tx, size.x), t->position.y };
    return true;
}

// A text: its image (in white) tinted with the color, in the given view.
static void drawText(const Text& tx, const Transform& t, AssetManager& assets, SDL_Renderer* renderer,
                     const View& view) {
    SDL_Texture* tex = assets.textTexture(tx.font, tx.size, tx.text);
    if (!tex) return;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    const float w = tw * std::fabs(t.scale.x) * view.zoom;
    const float h = th * std::fabs(t.scale.y) * view.zoom;
    Vec2 c = view.worldToScreen(t.position);
    c.x += alignShift(tx, w);

    float left = std::round(c.x - w * 0.5f), right = std::round(c.x + w * 0.5f);
    float top = std::round(c.y - h * 0.5f), bottom = std::round(c.y + h * 0.5f);
    if (right < 0 || bottom < 0 || left > view.width || top > view.height) return; // outside
    SDL_FRect dst{ left, top, right - left, bottom - top };
    SDL_SetTextureColorMod(tex, tx.color.r, tx.color.g, tx.color.b);
    SDL_SetTextureAlphaMod(tex, tx.color.a);
    SDL_RenderTexture(renderer, tex, nullptr, &dst);
}

void renderWorld(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view) {
    // First they are gathered and sorted: the pools store components in any
    // order. Everything that is drawn shares the same order scale.
    enum class Kind { Parallax, Tilemap, Sprite, Text }; // within the same entity, bottom to top
    struct Item { int order; Entity e; Kind kind; const void* data; const Transform* transform; };
    std::vector<Item> items;
    world.each<ParallaxBackground>([&](Entity e, ParallaxBackground& p) {
        if (p.visible && !p.texture.empty()) items.push_back({ p.order, e, Kind::Parallax, &p, nullptr });
    });
    world.each<Tilemap, Transform>([&](Entity e, Tilemap& m, Transform& t) {
        if (m.visible && !m.tileset.empty()) items.push_back({ m.order, e, Kind::Tilemap, &m, &t });
    });
    world.each<Sprite, Transform>([&](Entity e, Sprite& s, Transform& t) {
        if (s.visible && !s.texture.empty()) items.push_back({ s.order, e, Kind::Sprite, &s, &t });
    });
    world.each<Text, Transform>([&](Entity e, Text& tx, Transform& t) {
        if (tx.visible && !tx.screenSpace && !tx.text.empty()) items.push_back({ tx.order, e, Kind::Text, &tx, &t });
    });
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.order != b.order) return a.order < b.order;
        if (a.e != b.e) return a.e < b.e;
        return a.kind < b.kind;
    });

    for (const Item& it : items) {
        if (it.kind == Kind::Parallax) {
            drawParallax(*static_cast<const ParallaxBackground*>(it.data), assets, renderer, view);
            continue;
        }
        if (it.kind == Kind::Tilemap) {
            drawTilemap(*static_cast<const Tilemap*>(it.data), *it.transform, assets, renderer, view);
            continue;
        }
        if (it.kind == Kind::Text) {
            drawText(*static_cast<const Text*>(it.data), *it.transform, assets, renderer, view);
            continue;
        }
        const Sprite& s = *static_cast<const Sprite*>(it.data);
        const Transform& t = *it.transform;
        SDL_Texture* tex = assets.loadTexture(s.texture);
        if (!tex) continue;

        SDL_FRect src = sourceRect(s, tex);
        float w = src.w * std::fabs(t.scale.x) * view.zoom;
        float h = src.h * std::fabs(t.scale.y) * view.zoom;
        Vec2 c = view.worldToScreen(t.position);

        // Edges rounded to integers (crisp pixel art, no seams between neighbors).
        float left = std::round(c.x - w * 0.5f), right = std::round(c.x + w * 0.5f);
        float top = std::round(c.y - h * 0.5f), bottom = std::round(c.y + h * 0.5f);
        if (right < 0 || bottom < 0 || left > view.width || top > view.height) continue; // outside
        SDL_FRect dst{ left, top, right - left, bottom - top };

        // A negative scale also flips (like in Unity).
        bool fx = s.flipX != (t.scale.x < 0.0f);
        bool fy = s.flipY != (t.scale.y < 0.0f);
        SDL_FlipMode flip = (SDL_FlipMode)((fx ? SDL_FLIP_HORIZONTAL : 0) | (fy ? SDL_FLIP_VERTICAL : 0));

        // The texture is shared: the tint is set on each draw.
        SDL_SetTextureColorMod(tex, s.tint.r, s.tint.g, s.tint.b);
        SDL_SetTextureAlphaMod(tex, s.tint.a);
        SDL_RenderTextureRotated(renderer, tex, &src, &dst, t.rotation, nullptr, flip);
    }
}

View screenView(int width, int height) {
    View v;
    v.center = { width * 0.5f, height * 0.5f };
    v.zoom = 1.0f;
    v.width = width;
    v.height = height;
    return v;
}

void renderScreenTexts(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& hud) {
    struct Item { int order; Entity e; const Text* text; const Transform* transform; };
    std::vector<Item> items;
    world.each<Text, Transform>([&](Entity e, Text& tx, Transform& t) {
        if (tx.visible && tx.screenSpace && !tx.text.empty()) items.push_back({ tx.order, e, &tx, &t });
    });
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.order != b.order ? a.order < b.order : a.e < b.e;
    });
    for (const Item& it : items) drawText(*it.text, *it.transform, assets, renderer, hud);
}
