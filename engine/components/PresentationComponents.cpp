#include "components/PresentationComponents.h"

#include "core/Reflection.h"
#include "systems/Animation.h"

// Los nombres de los clips del SpriteAnimator de esa entidad (para elegirlos en el
// Inspector). Con 'none', primero "" = ninguno.
static std::vector<std::string> clipNames(const World& world, Entity e, bool none) {
    std::vector<std::string> names;
    if (none) names.push_back("");
    if (const SpriteAnimator* a = world.get<SpriteAnimator>(e))
        for (const AnimationClip& clip : a->clips) names.push_back(clip.name);
    return names;
}

void registerPresentationComponents(ComponentRegistry& registry, const AnimationConditions& conditions) {
    // Las listas van al final: en el archivo ocupan una linea por elemento.
    registry.add<SpriteAnimator>("SpriteAnimator")
        .field("startClip", &SpriteAnimator::startClip)
            .choices([](const World& w, Entity e) { return clipNames(w, e, true); })
            .tooltip("Clip al empezar a jugar (ninguno = el primero). Un AnimatorStateMachine lo cambia")
        .field("speed", &SpriteAnimator::speed).range(0.0f, 10.0f).speed(0.01f).tooltip("Multiplica los fps de todos los clips")
        .list("clips", &SpriteAnimator::clips, [](FieldsBuilder<AnimationClip>& clip) {
            clip.field("name", &AnimationClip::name).tooltip("Con este nombre lo piden las reglas y el codigo")
                .field("texture", &AnimationClip::texture).texture()
                    .tooltip("Imagen con los cuadros (vacio = la del Sprite al empezar)")
                .field("frameWidth", &AnimationClip::frameWidth).range(1.0f, 2048.0f).tooltip("Ancho de un cuadro (px de la imagen)")
                .field("frameHeight", &AnimationClip::frameHeight).range(1.0f, 2048.0f).tooltip("Alto de un cuadro (px de la imagen)")
                .field("frames", &AnimationClip::frames)
                    .tooltip("Celdas en orden, contando por filas (vacio = todas). Una columna de 4: \"0, 4, 8, 12\"")
                .field("fps", &AnimationClip::fps).range(0.0f, 120.0f).speed(0.1f).tooltip("Cuadros por segundo (0 = quieto)")
                .field("loop", &AnimationClip::loop).tooltip("Sin marcar: al terminar se queda en el ultimo cuadro")
                .field("next", &AnimationClip::next)
                    .choices([](const World& w, Entity e) { return clipNames(w, e, true); })
                    .tooltip("Sin loop: clip que sigue al terminar")
                .field("destroyOnEnd", &AnimationClip::destroyOnEnd).tooltip("Sin loop: al terminar destruye la entidad");
        });

    const AnimationConditions* conds = &conditions; // vive en Application, como el registro
    registry.add<AnimatorStateMachine>("AnimatorStateMachine")
        .field("defaultClip", &AnimatorStateMachine::defaultClip)
            .choices([](const World& w, Entity e) { return clipNames(w, e, false); })
            .tooltip("Clip cuando no se cumple ninguna regla")
        .list("rules", &AnimatorStateMachine::rules, [conds](FieldsBuilder<AnimationRule>& rule) {
            rule.field("clip", &AnimationRule::clip)
                    .choices([](const World& w, Entity e) { return clipNames(w, e, false); })
                .field("condition", &AnimationRule::condition)
                    .choices([conds](const World&, Entity) { return conds->names(); })
                    .tooltip("Gana la PRIMERA regla cuya condicion se cumpla");
        });

    registry.add<FollowCamera>("FollowCamera")
        .field("targetTag", &FollowCamera::targetTag).tooltip("Etiqueta de a quien sigue")
        .field("deadZone", &FollowCamera::deadZone).range(0.0f, 4096.0f).speed(1.0f)
            .tooltip("Rectangulo (px del mundo) en que el objetivo se mueve sin mover la camara")
        .field("smoothing", &FollowCamera::smoothing).range(0.0f, 60.0f).speed(0.1f).tooltip("Rapidez de la camara (0 = de golpe)")
        .field("lookAhead", &FollowCamera::lookAhead).range(0.0f, 1000.0f).speed(1.0f)
            .tooltip("Px que adelanta la vista hacia donde va el objetivo")
        .field("lookAheadSpeed", &FollowCamera::lookAheadSpeed).range(0.0f, 60.0f).speed(0.1f)
        .field("clampToTilemaps", &FollowCamera::clampToTilemaps).tooltip("La vista no se sale de los Tilemap de la escena");

    registry.add<ParallaxBackground>("ParallaxBackground")
        .field("texture", &ParallaxBackground::texture).texture().tooltip("Imagen que se repite (tileable)")
        .field("factor", &ParallaxBackground::factor).range(0.0f, 2.0f).speed(0.01f)
            .tooltip("Cuanto acompana a la camara: 0 = quieto, 1 = como el mundo")
        .field("scale", &ParallaxBackground::scale).range(0.05f, 64.0f).speed(0.05f)
        .field("scroll", &ParallaxBackground::scroll).speed(0.5f).tooltip("Desplazamiento propio (px/seg), solo jugando")
        .field("tint", &ParallaxBackground::tint)
        .field("visible", &ParallaxBackground::visible)
        .field("order", &ParallaxBackground::order).tooltip("Capa de dibujo: menor = mas al fondo");

    registry.add<Text>("Text")
        .field("text", &Text::text)
        .field("font", &Text::font).tooltip("Archivo .ttf (ruta desde la carpeta del ejecutable)")
        .field("size", &Text::size).range(1.0f, 512.0f).tooltip("Puntos")
        .field("color", &Text::color)
        .field("align", &Text::align).choices({ "left", "center", "right" })
            .tooltip("Donde cae la posicion respecto al texto (un contador: left)")
        .field("screenSpace", &Text::screenSpace)
            .tooltip("HUD: la posicion es px de la pantalla del juego y no sigue a la camara")
        .field("visible", &Text::visible)
        .field("order", &Text::order).tooltip("Capa de dibujo");
}
