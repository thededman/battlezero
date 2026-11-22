#include "raster.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace bz {

void Frame::clear(Color c) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; ++i) px[i] = c;
}

namespace {

enum { IN = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

int outcode(float x, float y) {
    int c = IN;
    if (x < 0) c |= LEFT;
    else if (x > SCREEN_W - 1) c |= RIGHT;
    if (y < 0) c |= TOP;
    else if (y > SCREEN_H - 1) c |= BOTTOM;
    return c;
}

// Cohen-Sutherland clip to the screen rectangle. Returns false if nothing is visible.
bool clipLine(float& x0, float& y0, float& x1, float& y1) {
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1))
        return false;
    int c0 = outcode(x0, y0), c1 = outcode(x1, y1);
    for (int guard = 0; guard < 8; ++guard) {
        if (!(c0 | c1)) return true;
        if (c0 & c1) return false;
        int c = c0 ? c0 : c1;
        float x = 0, y = 0;
        if (c & TOP) {
            x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0);
            y = 0;
        } else if (c & BOTTOM) {
            x = x0 + (x1 - x0) * ((SCREEN_H - 1) - y0) / (y1 - y0);
            y = SCREEN_H - 1;
        } else if (c & RIGHT) {
            y = y0 + (y1 - y0) * ((SCREEN_W - 1) - x0) / (x1 - x0);
            x = SCREEN_W - 1;
        } else {
            y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0);
            x = 0;
        }
        if (c == c0) {
            x0 = x;
            y0 = y;
            c0 = outcode(x0, y0);
        } else {
            x1 = x;
            y1 = y;
            c1 = outcode(x1, y1);
        }
    }
    return false;
}

// Classic 5x7 font, one byte per column, bit 0 is the top row.
struct Glyph {
    char ch;
    uint8_t col[5];
};

const Glyph FONT[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}}, {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
    {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}}, {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
    {'/', {0x20, 0x10, 0x08, 0x04, 0x02}}, {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
    {'<', {0x08, 0x14, 0x22, 0x41, 0x00}}, {'>', {0x00, 0x41, 0x22, 0x14, 0x08}},
    {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}}, {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}}, {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}}, {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}}, {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}}, {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}}, {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}}, {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}}, {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}}, {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}}, {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
    {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}}, {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}}, {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
    {'W', {0x3F, 0x40, 0x38, 0x40, 0x3F}}, {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}}, {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
};

const uint8_t* glyphFor(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    for (const Glyph& g : FONT)
        if (g.ch == ch) return g.col;
    return FONT[0].col;
}

} // namespace

void Frame::line(float fx0, float fy0, float fx1, float fy1, Color c) {
    if (!clipLine(fx0, fy0, fx1, fy1)) return;
    int x0 = static_cast<int>(std::lround(fx0)), y0 = static_cast<int>(std::lround(fy0));
    int x1 = static_cast<int>(std::lround(fx1)), y1 = static_cast<int>(std::lround(fy1));
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Frame::fillRect(int x, int y, int w, int h, Color c) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i) pixel(i, j, c);
}

void Frame::rect(int x, int y, int w, int h, Color c) {
    for (int i = x; i < x + w; ++i) {
        pixel(i, y, c);
        pixel(i, y + h - 1, c);
    }
    for (int j = y; j < y + h; ++j) {
        pixel(x, j, c);
        pixel(x + w - 1, j, c);
    }
}

int Frame::text(int x, int y, const char* s, Color c, int scale) {
    for (; *s; ++s) {
        const uint8_t* g = glyphFor(*s);
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if (g[col] & (1 << row)) {
                    if (scale == 1) pixel(x + col, y + row, c);
                    else fillRect(x + col * scale, y + row * scale, scale, scale, c);
                }
        x += 6 * scale;
    }
    return x;
}

int Frame::textWidth(const char* s, int scale) {
    int n = static_cast<int>(std::strlen(s));
    return n ? (n * 6 - 1) * scale : 0;
}

void Frame::textCentered(int cx, int y, const char* s, Color c, int scale) {
    text(cx - textWidth(s, scale) / 2, y, s, c, scale);
}

} // namespace bz
