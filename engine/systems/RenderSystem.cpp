#include "systems/RenderSystem.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/CoreComponents.h"
#include "core/World.h"
#include "platform/AssetManager.h"

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

void renderSprites(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view) {
    // Primero se juntan y ordenan: el pool guarda los sprites en cualquier orden.
    struct Item { int order; Entity e; Sprite* sprite; Transform* transform; };
    std::vector<Item> items;
    world.each<Sprite, Transform>([&](Entity e, Sprite& s, Transform& t) {
        if (s.visible && !s.texture.empty()) items.push_back({ s.order, e, &s, &t });
    });
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.order != b.order ? a.order < b.order : a.e < b.e;
    });

    for (const Item& it : items) {
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
