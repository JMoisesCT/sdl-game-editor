#pragma once
#include "core/Entity.h"
#include "core/Math.h"

class World;
class AssetManager;
struct SDL_Renderer; // declaracion adelantada: SDL solo aparece en el .cpp

// Una vista del mundo: que punto queda en el centro, con que zoom y en un destino de
// que tamano (en pixeles). La usan la vista del juego (desde una Camera) y la vista
// del editor (que se mueve sola, sin tocar ninguna entidad).
struct View {
    Vec2 center;          // punto del mundo que cae en el centro del destino
    float zoom = 1.0f;
    int width = 0;        // tamano del destino en pixeles
    int height = 0;

    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;
};

// Arma la vista de la PRIMERA entidad con Camera + Transform. false si no hay camara
// (la vista queda centrada en el origen con zoom 1). background recibe el color de
// la camara, si se pasa.
bool findCameraView(World& world, int width, int height, View& view, Color* background = nullptr);

// Tamano del sprite de esa entidad en el mundo (recorte o textura por la escala).
// false si no tiene Transform + Sprite o la textura no carga.
bool spriteWorldSize(World& world, AssetManager& assets, Entity e, Vec2& size);

// Rectangulo de un Text en SU espacio (el mundo, o la pantalla del juego si es
// screenSpace): centro (corrido segun align) y tamano. false si no tiene Transform +
// Text o no se puede dibujar (sin fuente o sin texto).
bool textBounds(World& world, AssetManager& assets, Entity e, Vec2& center, Vec2& size);

// Dibuja el MUNDO: los Sprite, Tilemap, ParallaxBackground y Text (los que no son
// screenSpace) visibles, de menor a mayor order (a igualdad, por id), en el destino de
// render ACTUAL del renderer. De cada Tilemap solo se dibujan las celdas que caen
// dentro de la vista; un ParallaxBackground llena la vista entera.
void renderWorld(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& view);

// La vista que deja la pantalla del juego tal cual (px de la resolucion del juego):
// la que se pasa a renderScreenTexts al dibujar el juego.
View screenView(int width, int height);

// Dibuja el HUD: los Text con screenSpace, por order, encima de lo que ya haya. 'hud'
// pasa de px de la pantalla del juego al destino: en el juego es screenView(); el
// editor la arma para mostrar el HUD sobre el recuadro de la camara en la vista Escena.
void renderScreenTexts(World& world, AssetManager& assets, SDL_Renderer* renderer, const View& hud);
