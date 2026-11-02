#include "systems/Animation.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <set>

#include "components/CoreComponents.h"
#include "components/PresentationComponents.h"
#include "core/World.h"
#include "platform/AssetManager.h"

// --- Reproduccion ------------------------------------------------------------------------

// Seg que se le perdonan a un cuadro para darlo por cumplido (ver update). Mayor que el
// temblor tipico del reloj con VSync (~1 ms) y mucho menor que un frame.
static const float FRAME_TIME_MARGIN = 0.002f;

namespace {
    const AnimationClip* findClip(const SpriteAnimator& a, const std::string& name) {
        for (const AnimationClip& clip : a.clips)
            if (clip.name == name) return &clip;
        return nullptr;
    }

    void startClip(SpriteAnimator& a, const std::string& name) {
        a.current = name;
        a.frame = 0;
        a.timer = 0.0f;
        a.finished = false;
    }

    // La imagen de un clip y su grilla (columnas y filas de cuadros). false si no carga
    // o el cuadro no cabe.
    bool clipGrid(AssetManager& assets, const AnimationClip& clip, const std::string& path,
                  int& columns, int& rows) {
        if (path.empty() || clip.frameWidth <= 0 || clip.frameHeight <= 0) return false;
        SDL_Texture* tex = assets.loadTexture(path);
        if (!tex) return false;
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        columns = (int)tw / clip.frameWidth;
        rows = (int)th / clip.frameHeight;
        return columns > 0 && rows > 0;
    }
}

bool playAnimation(World& world, Entity e, const std::string& clip, bool restart) {
    SpriteAnimator* a = world.get<SpriteAnimator>(e);
    if (!a || !findClip(*a, clip)) return false;
    if (a->current == clip && !restart) return true; // ya suena: no se corta
    startClip(*a, clip);
    return true;
}

void SpriteAnimatorSystem::onPlay(World& world) {
    world.each<SpriteAnimator>([](Entity, SpriteAnimator& a) {
        a.current.clear();
        a.frame = 0;
        a.timer = 0.0f;
        a.finished = a.justFinished = false;
        a.started = false;
    });
}

void SpriteAnimatorSystem::update(World& world, float dt) {
    world.each<SpriteAnimator, Sprite>([&](Entity e, SpriteAnimator& a, Sprite& sprite) {
        a.justFinished = false;
        // Arranque perezoso (vale tambien para una entidad creada jugando): se recuerda
        // la imagen del Sprite, que es la de los clips sin textura propia, y si nadie
        // pidio otro clip todavia, suena el inicial.
        if (!a.started) {
            a.started = true;
            a.baseTexture = sprite.texture;
            if (a.current.empty() && !a.clips.empty()) {
                const AnimationClip* first = findClip(a, a.startClip);
                startClip(a, first ? first->name : a.clips.front().name);
            }
        }

        const AnimationClip* clip = findClip(a, a.current);
        if (!clip) return;
        const std::string& path = clip->texture.empty() ? a.baseTexture : clip->texture;
        int columns = 0, rows = 0;
        if (!clipGrid(assets, *clip, path, columns, rows)) return;
        const int count = clip->frames.empty() ? columns * rows : (int)clip->frames.size();

        // Avanza tantos cuadros como quepan en el tiempo acumulado (a pocos fps, o con
        // un frame lento, pueden ser varios). Con MARGEN: a 20 fps un cuadro dura 50 ms,
        // justo 3 frames de 60 Hz, pero el reloj real tiembla y 3 frames pueden sumar
        // 49,8 ms. Sin margen ese cuadro esperaria un 4.o frame y el siguiente duraria 2:
        // la animacion iria a tirones (3, 4, 2, 3...). Lo que se adelanta se descuenta
        // del cuadro siguiente (timer queda negativo), asi la velocidad media no cambia.
        if (!a.finished && clip->fps > 0.0f && a.speed > 0.0f) {
            const float frameTime = 1.0f / clip->fps;
            const float margin = std::fmin(FRAME_TIME_MARGIN, frameTime * 0.25f);
            a.timer += dt * a.speed;
            while (a.timer >= frameTime - margin) {
                a.timer -= frameTime;
                if (a.frame + 1 < count) {
                    ++a.frame;
                } else if (clip->loop) {
                    a.frame = 0;
                } else {
                    a.finished = a.justFinished = true; // se queda en el ultimo cuadro
                    a.timer = 0.0f;
                    break;
                }
            }
        }
        if (a.frame >= count) a.frame = count - 1; // el clip se edito con menos cuadros

        // El cuadro actual pasa al Sprite: imagen y recorte.
        const int cell = clip->frames.empty() ? a.frame : clip->frames[a.frame];
        const int col = cell % columns, row = cell / columns;
        sprite.texture = path;
        sprite.source = { col * clip->frameWidth, row * clip->frameHeight, clip->frameWidth, clip->frameHeight };

        // Al terminar un clip sin loop: destruir (diferido) o encadenar. Va al final:
        // startClip cambia a.current, y 'clip' y 'path' apuntan a datos del animador.
        if (a.justFinished) {
            if (clip->destroyOnEnd) world.destroy(e);
            else if (!clip->next.empty() && findClip(a, clip->next)) startClip(a, std::string(clip->next));
        }
    });
}

// --- Maquina de estados ------------------------------------------------------------------

void AnimatorStateMachineSystem::onPlay(World& world) {
    world.each<AnimatorStateMachine>([](Entity, AnimatorStateMachine& fsm) { fsm.state.clear(); });
}

void AnimatorStateMachineSystem::update(World& world, float) {
    // Un aviso por nombre desconocido, no uno por frame.
    static std::set<std::string> warned;
    world.each<AnimatorStateMachine>([&](Entity e, AnimatorStateMachine& fsm) {
        const std::string* chosen = &fsm.defaultClip;
        for (const AnimationRule& rule : fsm.rules) {
            const Condition* condition = conditions.find(rule.condition);
            if (!condition) {
                if (warned.insert(rule.condition).second)
                    SDL_Log("AnimatorStateMachine: no existe la condicion '%s' (la regla no se cumple nunca)",
                            rule.condition.c_str());
                continue;
            }
            if ((*condition)(world, e)) { chosen = &rule.clip; break; } // gana la primera
        }
        // Solo al cambiar de regla: dentro de un mismo estado, el clip puede haber
        // encadenado con su 'next' y no hay que reiniciarlo.
        // Sin regla ni defaultClip: el clip sigue como esta, y la proxima regla que
        // gane (aunque sea la misma de antes) lo vuelve a arrancar.
        if (chosen->empty()) {
            fsm.state.clear();
            return;
        }
        if (*chosen == fsm.state) return;
        fsm.state = *chosen;
        playAnimation(world, e, fsm.state, true);
    });
}
