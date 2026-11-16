#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// Small value types used by the components. They are plain data (copied, compared
// and saved to JSON) and do not depend on SDL: the conversion to SDL_FRect or
// SDL_Color happens only in the .cpp files that draw.

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// RGBA color, 0 to 255 per channel.
struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

// Rectangle in whole pixels (texture crops, cells...).
// w == 0 or h == 0 means "no crop" where that makes sense.
struct IntRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// Grid of integers, width x height, stored by rows: cell (x, y) is
// cells[y * width + x]. The Tilemap uses it for its cells. In the file it is written
// as a list of rows, one per line.
// An empty grid is 0 x 0 (if one dimension is 0, so is the other).
struct IntGrid {
    int width = 0;
    int height = 0;
    std::vector<int> cells;

    IntGrid() = default;
    IntGrid(int w, int h, int fill) { resize(w, h, fill); }

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    // Outside the grid it returns 'outside'.
    int get(int x, int y, int outside = -1) const {
        return inside(x, y) ? cells[(std::size_t)y * width + x] : outside;
    }
    // Outside the grid it does nothing.
    void set(int x, int y, int value) {
        if (inside(x, y)) cells[(std::size_t)y * width + x] = value;
    }

    // Changes the size keeping whatever still fits (anchored at the top left);
    // the new cells get 'fill'.
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
