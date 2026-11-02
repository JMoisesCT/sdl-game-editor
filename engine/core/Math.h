#pragma once
#include <cstdint>

// Tipos pequenos de valor que usan los componentes. Son datos planos (se copian, se
// comparan y se guardan en JSON) y no dependen de SDL: la traduccion a SDL_FRect o
// SDL_Color ocurre solo en los .cpp que dibujan.

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Color RGBA de 0 a 255 por canal.
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

// Rectangulo en pixeles enteros (recortes de una textura, celdas...).
// w == 0 o h == 0 se usa como "sin recorte" donde tenga sentido.
struct IntRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};
