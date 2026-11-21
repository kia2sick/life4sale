#include <algorithm>
#include <cmath>
#include <cstring>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

namespace {
constexpr int Width = 320;
constexpr int Height = 240;

// Animation controls. Heights are measured in pixel-art pixels.
constexpr float BobHeight = 1.5f;
constexpr float WaveSpeed = 2.3f;       // Radians per second
constexpr float LetterPhase = 0.42f;   // Phase difference between neighbors
constexpr float PauseSeconds = 0.28f;
constexpr float JawOpenSpeed = 1.18f;   // Fraction of a full opening per second
constexpr float SwallowSeconds = 0.44f;

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
constexpr u32 TitleRed = rgb(231, 42, 48);
constexpr u32 TitleShadow = rgb(126, 23, 49);
constexpr u32 TitleHighlight = rgb(255, 114, 99);
constexpr u32 MenuInk = rgb(23, 42, 62);
constexpr u32 MenuAccent = rgb(255, 201, 83);
constexpr u32 Pole = rgb(52, 70, 89);

u32 pixels[Height][Width];
u32 seaside[Height][Width];

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

int textWidth(const char* text, int scale)
{
    int width = 0;
    for (; *text; ++text) {
        width += advance(*text, scale);
    }
    return width - scale;
}

// Offsetting each column gives the jaws a pixel-stepped tilt.
float distanceFromHinge(int x)
{
    return std::clamp((214 - x) / 196.0f, 0.0f, 1.0f);
}

int jawOffset(int x, float amount)
{
    return std::lround(distanceFromHinge(x) * amount);
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

void drawText(const char* text, int x, int y, int scale, u32 color)
{
    for (; *text; ++text) {
        if (*text != ' ') {
            drawLetter(*text, x, y, scale, color);
        }
        x += advance(*text, scale);
    }
}

void cloud(int x, int y, int width)
{
    rectangle(x, y + 8, width, 7, rgb(208, 237, 244));
    rectangle(x + 4, y + 5, width - 8, 7, rgb(255, 251, 230));
    rectangle(x + 11, y + 2, width - 25, 10, rgb(255, 251, 230));
    rectangle(x + 18, y, width - 41, 9, rgb(255, 251, 230));
}

void drawSeaside()
{
    constexpr u32 Sky[] = {rgb(80, 162, 218), rgb(104, 183, 230),
                           rgb(133, 203, 237), rgb(164, 221, 242),
                           rgb(193, 231, 241)};
    for (int band = 0; band < 5; ++band) {
        rectangle(0, band * 28, Width, 28, Sky[band]);
    }
    cloud(-10, 26, 73);
    cloud(111, 26, 63);
    cloud(246, 17, 91);
    cloud(274, 115, 58);

    rectangle(0, 137, Width, 2, rgb(244, 246, 205));
    rectangle(0, 139, Width, 15, rgb(37, 114, 168));
    rectangle(0, 154, Width, 15, rgb(42, 149, 186));
    rectangle(0, 169, Width, 16, rgb(59, 175, 189));
    for (int i = 0; i < 106; ++i) {
        const int y = 141 + (i * 17) % 42;
        const int width = 2 + (i * 5) % (y < 156 ? 7 : 16);
        const u32 water = i % 4 == 0 ? rgb(181, 236, 220) : rgb(92, 194, 206);
        rectangle((i * 103 + i / 5 * 13) % Width, y, width, 1, water);
    }
    // A low sea wall and a rail between the road and the water.
    rectangle(0, 181, Width, 8, rgb(190, 217, 205));
    rectangle(0, 189, Width, 5, rgb(249, 223, 174));
    rectangle(0, 173, Width, 1, rgb(77, 120, 131));
    rectangle(0, 178, Width, 1, rgb(117, 155, 155));
    for (int x = 7; x < Width; x += 17) {
        rectangle(x, 170, 2, 12, rgb(57, 92, 112));
    }

    rectangle(0, 194, Width, 46, rgb(67, 78, 94));
    rectangle(0, 196, Width, 1, rgb(128, 141, 141));
    rectangle(0, 234, Width, 2, rgb(43, 58, 75));
    rectangle(0, 236, Width, 4, rgb(196, 206, 195));
    for (int i = 0; i < 160; ++i) {
        rectangle((i * 79 + i / 7) % Width, 198 + i * 13 % 35, 1, 1, rgb(86, 97, 109));
    }
    for (int x = 4; x < Width; x += 29) {
        rectangle(x, 214, 15, 2, rgb(227, 188, 108));
    }
    // Cover the lane markings where the crosswalk meets the road.
    for (int row = 0; row < 38; ++row) {
        const int y = 196 + row;
        const int half = 24 + row / 2;
        const int middle = 172 - row / 4;
        rectangle(middle - half, y, 2 * half, 1,
                  row % 7 < 3 ? rgb(240, 235, 207) : rgb(67, 78, 94));
    }

    // Roadside lamps and a wire across the open sky.
    line(15, 9, 99, 32, rgb(72, 118, 151));
    line(99, 32, 208, 44, rgb(72, 118, 151));
    line(208, 44, 303, 34, rgb(72, 118, 151));
    rectangle(11, 9, 5, 183, Pole);
    rectangle(12, 10, 1, 179, rgb(104, 130, 141));
    rectangle(14, 34, 21, 3, Pole);
    rectangle(29, 35, 11, 4, Pole);
    rectangle(31, 39, 7, 2, Cream);
    rectangle(301, 34, 4, 157, Pole);
    rectangle(282, 52, 21, 2, Pole);
    rectangle(280, 53, 11, 4, Pole);
    rectangle(282, 57, 7, 2, Cream);
    rectangle(302, 156, 1, 31, rgb(104, 130, 141));

    std::memcpy(seaside, pixels, sizeof seaside);
}

enum class Screen { Title, Pause, Opening, Swallow, Dark };
struct TitleState {
    Screen screen = Screen::Title;
    int selected = 0;
    float waveTime = 0.0f;
    float sceneTime = 0.0f;
};

struct MenuOption {
    const char* label;
    bool enabled;
};

constexpr MenuOption Menu[] = {
    {"NEW GAME", true},
    {"LOAD GAME", false},
    {"SETTINGS", false},
    {"EXIT", true},
};
constexpr int MenuCount = sizeof Menu / sizeof Menu[0];

u32 readButtons()
{
    WPAD_ScanPads();
    PAD_ScanPads();
    u32 buttons = 0;
    for (int channel = 0; channel < 4; ++channel) {
        buttons |= WPAD_ButtonsDown(channel);
        const u16 pad = PAD_ButtonsDown(channel);
        if (pad & PAD_BUTTON_UP) buttons |= WPAD_BUTTON_UP;
        if (pad & PAD_BUTTON_DOWN) buttons |= WPAD_BUTTON_DOWN;
        if (pad & PAD_BUTTON_A) buttons |= WPAD_BUTTON_A;
        if (pad & PAD_BUTTON_B) buttons |= WPAD_BUTTON_B;
        if (pad & PAD_BUTTON_START) buttons |= WPAD_BUTTON_HOME;
    }
    return buttons;
}

// Returning false exits the application.
bool updateTitle(TitleState& title, u32 buttons, float dt)
{
    if (buttons & WPAD_BUTTON_HOME) {
        return false;
    }
    if (title.screen == Screen::Title) {
        title.waveTime += dt;
        if (buttons & WPAD_BUTTON_UP) {
            title.selected = (title.selected + MenuCount - 1) % MenuCount;
        }
        if (buttons & WPAD_BUTTON_DOWN) {
            title.selected = (title.selected + 1) % MenuCount;
        }
        if ((buttons & WPAD_BUTTON_A) && Menu[title.selected].enabled) {
            if (title.selected == MenuCount - 1) {
                return false;
            }
            title.screen = Screen::Pause;
            title.sceneTime = 0.0f;
        }
    } else if (title.screen == Screen::Dark) {
        if (buttons & WPAD_BUTTON_B) {
            title = TitleState{};
        }
    } else {
        title.sceneTime += dt;
        if (title.screen == Screen::Pause && title.sceneTime >= PauseSeconds) {
            title.screen = Screen::Opening;
            title.sceneTime = 0.0f;
        } else if (title.screen == Screen::Opening && title.sceneTime * JawOpenSpeed >= 1.0f) {
            title.screen = Screen::Swallow;
            title.sceneTime = 0.0f;
        } else if (title.screen == Screen::Swallow && title.sceneTime >= SwallowSeconds) {
            title.screen = Screen::Dark;
        }
    }
    return true;
}

int letterBob(float waveTime, int letterIndex)
{
    const float phase = waveTime * WaveSpeed - letterIndex * LetterPhase;
    return std::lround(std::sin(phase) * BobHeight);
}

// Each letter is drawn in a 24-by-36 design grid, then rasterized at its actual size.
bool roundedBox(float x, float y, float left, float top, float right, float bottom, float radius)
{
    const float dx = x - std::clamp(x, left + radius, right - radius);
    const float dy = y - std::clamp(y, top + radius, bottom - radius);
    return dx * dx + dy * dy <= radius * radius;
}

bool logoShape(char letter, float x, float y)
{
    const auto box = [x, y](float left, float top, float right, float bottom, float radius) {
        return roundedBox(x, y, left, top, right, bottom, radius);
    };
    switch (letter) {
    case 'L':
        return box(0, 0, 10, 36, 4) || box(0, 26, 24, 36, 4);
    case 'I':
        return box(0, 0, 24, 9, 4) || box(7, 0, 17, 36, 4) || box(0, 27, 24, 36, 4);
    case 'F':
        return box(0, 0, 10, 36, 4) || box(0, 0, 24, 10, 4) || box(1, 14, 20, 23, 4);
    case 'E':
        return box(0, 0, 10, 36, 4) || box(0, 0, 24, 10, 4) ||
               box(1, 14, 21, 23, 4) || box(0, 26, 24, 36, 4);
    case 'O':
        return box(0, 0, 24, 36, 10) && !box(8, 8, 16, 28, 4);
    case 'R':
        return box(0, 0, 9, 36, 4) ||
               (box(0, 0, 24, 23, 8) && !box(9, 7, 16, 15, 3)) ||
               roundedBox(x - (y - 19) * 0.48f, y, 8, 17, 17, 36, 4);
    case 'S':
        return box(0, 0, 24, 36, 7) &&
               !box(9, 8, 28, 15, 3) && !box(-4, 22, 15, 28, 3);
    case 'A':
        return box(0, 0, 24, 36, 10) &&
               !box(8, 8, 16, 18, 4) && !box(8, 25, 16, 41, 3);
    case '!':
        return box(1, 0, 23, 25, 7) || box(2, 28, 22, 36, 4);
    }
    return false;
}

constexpr int LogoLetterCount = 12;
constexpr int LogoMaxWidth = 49;
constexpr int LogoMaxHeight = 74;
constexpr int UpperJawTop = 46;
constexpr int LowerJawTop = 126;

struct LogoLetter {
    int x;
    int width;
    int height;
    bool mask[LogoMaxWidth][LogoMaxHeight];
};

LogoLetter logo[LogoLetterCount];

void prepareLogo()
{
    constexpr char Letters[] = "LIFEFORSALE!";
    constexpr int Widths[] = {45, 35, 48, 49, 22, 24, 24, 24, 25, 21, 23, 10};
    int x = 20;
    for (int i = 0; i < LogoLetterCount; ++i) {
        if (i == 4) x = 20; // Start the lower jaw on its own line.
        if (i == 7) x += 7; // The space between FOR and SALE.
        LogoLetter& letter = logo[i];
        letter.x = x;
        letter.width = Widths[i];
        letter.height = i < 4 ? LogoMaxHeight : 40;
        for (int column = 0; column < letter.width; ++column) {
            for (int row = 0; row < letter.height; ++row) {
                letter.mask[column][row] = logoShape(Letters[i],
                    24.0f * (column + 0.5f) / letter.width,
                    36.0f * (row + 0.5f) / letter.height);
            }
        }
        x += letter.width + (i < 4 ? 5 : 2);
    }
}

void drawLogo(float waveTime, float opening, float upperTilt, float lowerTilt)
{
    for (int i = 0; i < LogoLetterCount; ++i) {
        const LogoLetter& letter = logo[i];
        const bool upper = i < 4;
        const int jawTop = upper ? UpperJawTop : LowerJawTop;
        const float lift = (upper ? -10.0f : 12.0f) * opening;
        const float tilt = upper ? upperTilt : lowerTilt;
        // Keep the frozen bob initially, then settle into the shape of each jaw.
        const int top = jawTop + std::lround(letterBob(waveTime, i) * (1.0f - opening) + lift);

        for (int column = 0; column < letter.width; ++column) {
            const int x = letter.x + column;
            const int y = top + jawOffset(x, tilt);
            for (int row = 0; row < letter.height; ++row) {
                if (letter.mask[column][row]) {
                    rectangle(x + 2, y + row + 4, 1, 1, TitleShadow);
                }
            }
        }
        for (int column = 0; column < letter.width; ++column) {
            const int x = letter.x + column;
            const int y = top + jawOffset(x, tilt);
            for (int row = 0; row < letter.height; ++row) {
                if (letter.mask[column][row]) {
                    const bool upperEdge = row < 2 || !letter.mask[column][row - 2];
                    rectangle(x, y + row, 1, 1, upperEdge ? TitleHighlight : TitleRed);
                }
            }
        }
    }
}

void drawMenu(int selected)
{
    rectangle(225, 61, 83, 72, rgb(81, 130, 162));
    rectangle(222, 58, 83, 72, MenuInk);
    rectangle(225, 61, 77, 1, rgb(105, 163, 184));
    rectangle(225, 126, 77, 1, rgb(55, 88, 111));

    for (int i = 0; i < MenuCount; ++i) {
        const int y = 68 + 14 * i;
        if (i == selected) {
            rectangle(226, y - 2, 75, 11, rgb(65, 91, 108));
            rectangle(229, y + 1, 2, 5, MenuAccent);
            rectangle(231, y + 2, 2, 3, MenuAccent);
            rectangle(233, y + 3, 1, 1, MenuAccent);
        }
        const u32 color = Menu[i].enabled ? Cream : rgb(148, 169, 181);
        drawText(Menu[i].label, 238, y, 1, color);
        if (!Menu[i].enabled) {
            rectangle(295, y + 3, 4, 1, color);
        }
    }
}

void drawMouth(float opening, float upperTilt, float lowerTilt)
{
    const int left = 22 - std::lround(5 * opening);
    for (int x = left; x < 214; ++x) {
        const float hinge = distanceFromHinge(x);
        const int upper = UpperJawTop + LogoMaxHeight - std::lround(10 * opening) + jawOffset(x, upperTilt);
        const int lower = LowerJawTop + std::lround(12 * opening) + jawOffset(x, lowerTilt);
        const int taper = x < left + 12 ? (left + 12 - x) / 2 : 0;
        rectangle(x, upper + taper, 1, lower - upper - 2 * taper, Black);

        // One little triangle per tooth, using columns inside the mouth.
        const int toothColumn = (x - left) % 15;
        if (toothColumn < 9) {
            const int depth = std::lround((5 - std::abs(4 - toothColumn)) * 2 * opening * hinge);
            rectangle(x, upper + taper, 1, depth, Cream);
        }
        const int bottomColumn = (x - left + 7) % 15;
        if (bottomColumn < 9) {
            const int depth = std::lround((5 - std::abs(4 - bottomColumn)) * opening * hinge);
            rectangle(x, lower - taper - depth, 1, depth, rgb(209, 192, 161));
        }
    }
}

void drawTitle(const TitleState& title)
{
    if (title.screen == Screen::Dark) {
        rectangle(0, 0, Width, Height, Black);
        constexpr char Prompt[] = "PRESS B TO RETURN";
        drawText(Prompt, (Width - textWidth(Prompt, 2)) / 2, 113, 2, Cream);
        return;
    }

    std::memcpy(pixels, seaside, sizeof pixels);
    if (title.screen == Screen::Title || title.screen == Screen::Pause) {
        drawMenu(title.selected);
    }

    float opening = 0.0f;
    if (title.screen == Screen::Opening) {
        const float t = std::clamp(title.sceneTime * JawOpenSpeed, 0.0f, 1.0f);
        opening = t * t * (3.0f - 2.0f * t);
    } else if (title.screen == Screen::Swallow) {
        opening = 1.0f;
    }

    const float upperTilt = -31.0f * opening;
    const float lowerTilt = 29.0f * opening;
    if (opening > 0.0f) {
        drawMouth(opening, upperTilt, lowerTilt);
    }
    drawLogo(title.waveTime, opening, upperTilt, lowerTilt);

    if (title.screen == Screen::Swallow) {
        // Expand the darkness from inside the mouth until it covers the display.
        const float t = std::clamp(title.sceneTime / SwallowSeconds, 0.0f, 1.0f);
        constexpr int MouthX = 104;
        constexpr int MouthY = 118;
        const int left = MouthX - std::lround(MouthX * t);
        const int right = MouthX + std::lround((Width - MouthX) * t);
        const int top = MouthY - std::lround(MouthY * t);
        const int bottom = MouthY + std::lround((Height - MouthY) * t);
        rectangle(left, top, right - left, bottom - top, Black);
    }
}

void present(const GXRModeObj& mode, u32* framebuffer)
{
    const int stride = VIDEO_PadFramebufferWidth(mode.fbWidth) / 2;
    const int pairs = mode.fbWidth / 2;
    for (int y = 0; y < mode.xfbHeight; ++y) {
        u32* row = framebuffer + y * stride;
        const u32* source = pixels[y * Height / mode.xfbHeight];
        if (pairs == Width) {
            std::memcpy(row, source, Width * sizeof(u32));
        } else {
            for (int x = 0; x < pairs; ++x) {
                row[x] = source[x * Width / pairs];
            }
        }
    }
    VIDEO_SetNextFramebuffer(framebuffer);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}
} // namespace

int main()
{
    VIDEO_Init();
    GXRModeObj* videoMode = VIDEO_GetPreferredMode(nullptr);
    u32* framebuffers[] = {
        static_cast<u32*>(MEM_K0_TO_K1(SYS_AllocateFramebuffer(videoMode))),
        static_cast<u32*>(MEM_K0_TO_K1(SYS_AllocateFramebuffer(videoMode))),
    };
    VIDEO_Configure(videoMode);
    WPAD_Init();
    PAD_Init();

    drawSeaside();
    prepareLogo();
    TitleState title;
    int backBuffer = 1;
    drawTitle(title);
    VIDEO_SetBlack(FALSE);
    present(*videoMode, framebuffers[0]);

    u64 previousTime = gettime();
    while (true) {
        const u64 now = gettime();
        const float dt = std::min(ticks_to_microsecs(now - previousTime) / 1000000.0f, 0.1f);
        previousTime = now;

        const u32 buttons = readButtons();
        if (!updateTitle(title, buttons, dt)) {
            break;
        }
        drawTitle(title);
        present(*videoMode, framebuffers[backBuffer]);
        backBuffer = 1 - backBuffer;
    }
    return 0;
}
