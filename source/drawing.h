#pragma once

#include <gctypes.h>

constexpr int Width = 320;
constexpr int Height = 240;

// Each framebuffer word represents two horizontal pixels of the same color.
constexpr u32 rgb(int r, int g, int b)
{
    const u32 y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
    const u32 u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
    const u32 v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
    return (y << 24) | (u << 16) | (y << 8) | v;
}


constexpr u32 Black = rgb(0, 0, 0);
constexpr u32 Cream = rgb(255, 245, 208);

extern u32 pixels[Height][Width];

void rectangle(int x, int y, int width, int height, u32 color);
void line(int x1, int y1, int x2, int y2, u32 color);
void drawText(const char* text, int x, int y, int scale, u32 color);
