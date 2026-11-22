// 320x170 RGB565 frame with clipped lines and a 5x7 bitmap font.
#pragma once
#include <cstdint>

namespace bz {

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 170;

using Color = uint16_t;

constexpr Color rgb(int r, int g, int b) {
    return static_cast<Color>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

constexpr Color BLACK = rgb(0, 0, 0);
constexpr Color AMBER = rgb(255, 176, 0);
constexpr Color AMBER_DIM = rgb(130, 88, 0);
constexpr Color AMBER_DARK = rgb(70, 48, 0);
constexpr Color CYAN = rgb(0, 220, 255);
constexpr Color CYAN_DIM = rgb(0, 100, 120);
constexpr Color RED = rgb(255, 70, 20);
constexpr Color WHITE = rgb(255, 255, 255);
constexpr Color VIOLET = rgb(205, 130, 255);

struct Frame {
    Color px[SCREEN_W * SCREEN_H];

    void clear(Color c = BLACK);
    void pixel(int x, int y, Color c) {
        if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) px[y * SCREEN_W + x] = c;
    }
    Color at(int x, int y) const { return px[y * SCREEN_W + x]; }
    void line(float x0, float y0, float x1, float y1, Color c);
    void fillRect(int x, int y, int w, int h, Color c);
    void rect(int x, int y, int w, int h, Color c);
    // Draws upper-case text; returns the x just past the last glyph.
    int text(int x, int y, const char* s, Color c, int scale = 1);
    void textCentered(int cx, int y, const char* s, Color c, int scale = 1);
    static int textWidth(const char* s, int scale = 1);
};

} // namespace bz
