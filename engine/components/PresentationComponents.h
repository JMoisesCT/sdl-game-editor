#pragma once
#include <string>
#include <vector>

#include "core/Entity.h"
#include "core/Math.h"

class ComponentRegistry;
class ConditionRegistry;

// Componentes de PRESENTACION: animacion, camara que sigue, fondos con parallax y
// texto. Como todos, son SOLO datos; los mueven los sistemas de systems/Animation.h y
// systems/Presentation.h, y los dibuja RenderSystem.

// --- Animacion ---------------------------------------------------------------------------

// Un clip: una secuencia de cuadros recortados de una imagen en grilla. Los cuadros
// se numeran por filas desde arriba a la izquierda, como los tiles de un Tilemap (con
// 4 columnas, el 4 es el primero de la segunda fila).
//
// Sirve para las tres formas tipicas de organizar una animacion:
//   - una tira por archivo (Pixel Adventure: "Run (32x32).png"): texture + frames vacio;
//   - una fila o columna de una hoja (Ninja Adventure: la columna 0 mira hacia abajo):
//     frames = las celdas de esa linea ("0, 4, 8, 12");
//   - celdas sueltas de la imagen del Sprite: texture vacio + frames.
struct AnimationClip {
    std::string name = "clip";
    std::string texture;      // vacio = la textura que tenia el Sprite al empezar a jugar
    int frameWidth = 32;      // tamano de un cuadro en la imagen (px)
    int frameHeight = 32;
    std::vector<int> frames;  // celdas en orden; vacio = todas las de la imagen
    float fps = 20.0f;
    bool loop = true;
    std::string next;         // sin loop: al terminar, pasa a este clip (vacio = se queda en el ultimo cuadro)
    bool destroyOnEnd = false; // sin loop: al terminar, destruye la entidad (efectos, frutas recogidas)
};

// Reproduce clips cuadro a cuadro. NO dibuja: escribe la textura y el recorte del
// Sprite de la misma entidad, y RenderSystem dibuja el Sprite como siempre.
//
// Quien elige el clip: el AnimatorStateMachine de la entidad (si tiene), o cualquier
// sistema con playAnimation (Animation.h). Sin nadie que elija, suena startClip.
//
// Avisos de un frame (como justLanded): justFinished, cuando termina un clip sin loop.
struct SpriteAnimator {
    std::string startClip;    // clip al empezar a jugar (vacio = el primero)
    float speed = 1.0f;       // 2 = el doble de rapido; 0 = congelado
    std::vector<AnimationClip> clips;

    // --- Estado de ejecucion (lo escribe SpriteAnimatorSystem) ---
    std::string current;      // clip que suena
    int frame = 0;            // posicion dentro de los cuadros del clip
    float timer = 0.0f;       // tiempo acumulado del cuadro actual
    bool finished = false;    // clip sin loop que ya llego al ultimo cuadro
    bool justFinished = false; // ...y llego en este frame
    std::string baseTexture;  // la textura del Sprite al empezar (para los clips sin textura)
    bool started = false;
};

// Una regla del AnimatorStateMachine: si la condicion se cumple, suena ese clip.
struct AnimationRule {
    std::string clip;
    std::string condition;    // nombre de una condicion registrada (ConditionRegistry)
};

// Elige que clip suena a partir de REGLAS en vez de una cascada de if. Cada frame se
// recorren EN ORDEN y gana la PRIMERA cuya condicion se cumpla; si no se cumple
// ninguna, suena defaultClip. El orden ES la prioridad:
//     hit  si dead
//     jump si rising
//     fall si airborne
//     run  si moving
//     (si no, idle)
// Las condiciones son funciones de C++ con nombre (la logica sigue en C++): el motor
// trae las basicas (grounded, rising, dead...) y el juego suma las suyas. Ver
// systems/Conditions.h.
//
// Solo cambia de clip cuando cambia la regla ganadora: asi un clip puede encadenar con
// su 'next' (bandera que sale -> bandera ondeando) sin que la maquina lo reinicie.
struct AnimatorStateMachine {
    std::string defaultClip = "idle";
    std::vector<AnimationRule> rules;

    // --- Estado de ejecucion ---
    std::string state;        // clip elegido en el ultimo frame
};

// --- Camara ------------------------------------------------------------------------------

// Va en la entidad de la Camera: la mueve para seguir a la primera entidad con la
// etiqueta targetTag, pero SOLO cuando sale de la zona muerta (un rectangulo centrado
// en la camara). Mientras el objetivo se mueve dentro, la camara no se mueve: asi no
// tiembla con cada paso ni con cada salto pequeno.
struct FollowCamera {
    std::string targetTag = "Player";
    Vec2 deadZone{ 160.0f, 120.0f }; // ancho y alto de la zona muerta (px del mundo)
    float smoothing = 6.0f;          // rapidez con que alcanza su destino (0 = de golpe)
    float lookAhead = 0.0f;          // px que adelanta la vista hacia donde va el objetivo
    float lookAheadSpeed = 3.0f;     // rapidez con que cambia ese adelanto
    bool clampToTilemaps = true;     // la vista no se sale de los Tilemap de la escena

    // --- Estado de ejecucion ---
    Entity target = NullEntity;
    float lookOffset = 0.0f;   // adelanto actual
    float lastTargetX = 0.0f;
    bool started = false;      // ya se coloco sobre el objetivo
};

// --- Fondo -------------------------------------------------------------------------------

// Imagen que se repite para llenar la vista y que acompana a la camara solo en parte:
// da sensacion de profundidad. La imagen debe ser TILEABLE (las de Pixel Adventure
// encajan consigo mismas). No usa el Transform: siempre llena toda la vista.
//
// factor: 0 = no se mueve nunca (infinitamente lejos), 0.3 = montanas del fondo,
// 1 = se mueve igual que el mundo. Varias capas con factores distintos dan el efecto
// completo.
struct ParallaxBackground {
    std::string texture;
    Vec2 factor{ 0.3f, 0.3f };
    float scale = 1.0f;      // tamano del mosaico (4 con una imagen de 64 px = 256 px)
    Vec2 scroll;             // desplazamiento propio en px/seg (nubes, agua); solo jugando
    Color tint;
    bool visible = true;
    int order = -100;        // capa de dibujo, la misma escala que Sprite::order

    // --- Estado de ejecucion ---
    Vec2 offset;             // lo que lleva movido por 'scroll'
};

// --- Texto -------------------------------------------------------------------------------

// Una linea de texto con una fuente TTF. El Transform da la posicion (y su escala
// agranda el texto); la rotacion no se aplica.
//
// screenSpace:
//   false -> el texto esta en el MUNDO: pasa por la camara (un cartel en el nivel).
//   true  -> el texto esta en la PANTALLA del juego (HUD): la posicion son pixeles de
//            la resolucion del juego, desde la esquina superior izquierda, y no se
//            mueve con la camara. Se dibuja encima de todo el mundo.
//
// align dice donde cae la posicion respecto al texto, en horizontal. Un contador que
// cambia de largo ("FRUTAS: 9/10") quiere "left": con "center" se recentraria y
// pareceria saltar. En vertical la posicion siempre es el centro del renglon.
//
// Pixel art nitido: el texto se rasteriza sin suavizado (fuentes pixel) y se dibuja
// en coordenadas enteras. El texto lo cambia el juego desde un sistema (ver
// game/PlatformerRules); el dibujo cachea la imagen de cada texto.
struct Text {
    std::string text = "Texto";
    std::string font = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
    int size = 16;             // puntos
    Color color;
    std::string align = "center"; // left, center o right
    bool screenSpace = false;
    bool visible = true;
    int order = 100;           // capa de dibujo (entre los textos del HUD, o con los sprites)
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// despues de registerRuleComponents. 'conditions' da la lista de condiciones que
// ofrece el Inspector en las reglas del AnimatorStateMachine.
void registerPresentationComponents(ComponentRegistry& registry, const ConditionRegistry& conditions);
