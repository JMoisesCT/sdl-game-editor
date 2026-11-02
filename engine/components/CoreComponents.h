#pragma once
#include <string>

#include "core/Math.h"

class ComponentRegistry;

// Componentes basicos del motor. Son SOLO datos: no tienen metodos de ciclo de vida
// ni tocan SDL. Lo que hacen lo deciden los sistemas (RenderSystem los dibuja).
// Sus campos se declaran a la reflexion en CoreComponents.cpp: un campo nuevo que se
// quiera guardar y ver en el editor tambien se agrega ahi.

// Nombre y etiqueta. El nombre identifica a una entidad para las personas (la
// Jerarquia del editor); la etiqueta la clasifica para el codigo ("Player", "Enemy").
// Regla de siempre: el codigo filtra por tag, nunca por name.
struct EntityInfo {
    std::string name = "Entidad";
    std::string tag;
};

// Posicion, rotacion y escala. La posicion es el CENTRO de la entidad en el mundo
// (pixeles), no su esquina.
struct Transform {
    Vec2 position;
    float rotation = 0.0f;    // grados, sentido horario (como SDL)
    Vec2 scale{ 1.0f, 1.0f }; // 1 = tamano original de la imagen
};

// Imagen dibujada centrada en el Transform. Tamano en el mundo = recorte (o la
// textura entera si no hay recorte) por la escala del Transform.
struct Sprite {
    std::string texture;    // ruta de la imagen, relativa a la carpeta del ejecutable
    IntRect source;         // recorte dentro de la textura; w o h = 0 -> textura completa
    Color tint;             // blanco = sin tenir
    bool flipX = false;
    bool flipY = false;
    bool visible = true;
    int order = 0;          // capa de dibujo: menor = mas al fondo (a igualdad, el id)
};

// Lo que ve el jugador. La posicion la da el Transform de la misma entidad: ese punto
// queda en el centro de la vista. Si hay varias, el juego usa la primera que encuentre.
struct Camera {
    float zoom = 1.0f;      // 2 = todo se ve el doble de grande
    Color background{ 32, 32, 40, 255 }; // color con que se limpia la vista del juego
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// antes de que el juego registre los suyos.
void registerCoreComponents(ComponentRegistry& registry);
