#include "drawing.h"

#include <algorithm>
#include <cstdlib>

u32 pixels[Height][Width];

void rectangle(int x, int y, int width, int height, u32 color)
{
    const int left = std::max(0, x);
    const int right = std::min(Width, x + width);
    if (left >= right) {
        return;
    }
    for (int row = std::max(0, y); row < std::min(Height, y + height); ++row) {
        std::fill(pixels[row] + left, pixels[row] + right, color);
    }
}

void line(int x1, int y1, int x2, int y2, u32 color)
{
    const int steps = std::max(std::abs(x2 - x1), std::abs(y2 - y1));
    for (int i = 0; i <= steps; ++i) {
        const int divisor = std::max(1, steps);
        rectangle(x1 + (x2 - x1) * i / divisor,
                  y1 + (y2 - y1) * i / divisor, 1, 1, color);
    }
}

namespace {
// Seven rows of five bits for each glyph, from A through Z, then !.
constexpr u8 Glyphs[][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {15,16,16,16,16,16,15},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {15,16,16,23,17,17,15}, {17,17,17,31,17,17,17}, {31,4,4,4,4,4,31},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}, {4,4,4,4,4,0,4}
};

int advance(char letter, int scale)
{
    return (letter == ' ' ? 3 : 6) * scale;
}

void drawLetter(char letter, int x, int y, int scale, u32 color)
{
    const u8* glyph = Glyphs[letter == '!' ? 26 : letter - 'A'];
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if (!(glyph[row] & (1 << (4 - column)))) {
                continue;
            }
            rectangle(x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}
} // namespace

void drawText(const char* text, int x, int y, int scale, u32 color)
{
    for (; *text; ++text) {
        if (*text != ' ') {
            drawLetter(*text, x, y, scale, color);
        }
        x += advance(*text, scale);
    }
}
