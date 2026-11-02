#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

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

// Grilla de enteros de width x height, guardada por filas: la celda (x, y) es
// cells[y * width + x]. La usa el Tilemap para sus celdas. En el archivo se escribe
// como una lista de filas, una por linea.
// Una grilla vacia es 0 x 0 (si una medida es 0, la otra tambien).
struct IntGrid {
    int width = 0;
    int height = 0;
    std::vector<int> cells;

    IntGrid() = default;
    IntGrid(int w, int h, int fill) { resize(w, h, fill); }

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    // Fuera de la grilla devuelve 'outside'.
    int get(int x, int y, int outside = -1) const {
        return inside(x, y) ? cells[(std::size_t)y * width + x] : outside;
    }
    // Fuera de la grilla no hace nada.
    void set(int x, int y, int value) {
        if (inside(x, y)) cells[(std::size_t)y * width + x] = value;
    }

    // Cambia el tamano conservando lo que sigue cabiendo (anclado arriba a la
    // izquierda); lo nuevo queda con 'fill'.
    void resize(int w, int h, int fill) {
        if (w <= 0 || h <= 0) w = h = 0;
        std::vector<int> next((std::size_t)w * h, fill);
        for (int y = 0; y < std::min(h, height); ++y)
            for (int x = 0; x < std::min(w, width); ++x)
                next[(std::size_t)y * w + x] = cells[(std::size_t)y * width + x];
        width = w;
        height = h;
        cells = std::move(next);
    }
};
