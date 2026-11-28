#include "title_screen.h"
#include "drawing.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <wiiuse/wpad.h>

namespace {
// Animation controls. Heights are measured in pixel-art pixels.
constexpr float BobHeight = 1.5f;
constexpr float WaveSpeed = 2.3f;       // Radians per second
constexpr float LetterPhase = 0.42f;   // Phase difference between neighbors
constexpr float FirstGrabSeconds = 0.12f;
constexpr float SecondGrabSeconds = 0.60f;
constexpr float StruggleSeconds = 1.23f;
constexpr float MouthBurstSeconds = 0.32f;
constexpr float TongueSweepSeconds = 0.68f;

constexpr u32 TitleRed = rgb(231, 42, 48);
constexpr u32 TitleShadow = rgb(126, 23, 49);
constexpr u32 TitleHighlight = rgb(255, 114, 99);
constexpr u32 MenuInk = rgb(23, 42, 62);
constexpr u32 MenuAccent = rgb(255, 201, 83);
constexpr u32 Pole = rgb(52, 70, 89);

u32 seaside[Height][Width];

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
constexpr int MonsterLetter = 5; // O in FOR
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

struct StrugglePose {
    float leftGrip;
    float rightGrip;
    int stretch;
    int shake;
};

float easeBetween(float time, float start, float finish)
{
    const float t = std::clamp((time - start) / (finish - start), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

StrugglePose strugglePose(float time, float release)
{
    // One hand hooks the rim, slips, and leaves a gap before the second grab.
    const float firstGrab = easeBetween(time, FirstGrabSeconds, FirstGrabSeconds + 0.10f) *
                            (1.0f - easeBetween(time, 0.35f, 0.49f));
    const float pull = easeBetween(time, SecondGrabSeconds + 0.16f, StruggleSeconds - 0.10f);
    const float reach = 0.72f + pull * 0.28f;
    const float left = (firstGrab + reach * easeBetween(time, SecondGrabSeconds, SecondGrabSeconds + 0.12f)) * release;
    const float right = reach * easeBetween(time, SecondGrabSeconds + 0.07f, SecondGrabSeconds + 0.19f) * release;
    return {left, right, static_cast<int>(std::lround(2 * left + 2 * right + 6 * pull * release)),
            static_cast<int>(std::lround(std::sin(time * 65.0f) * 2 * pull * release))};
}

void drawLogo(float waveTime, const StrugglePose& pose)
{
    for (int i = 0; i < LogoLetterCount; ++i) {
        const LogoLetter& letter = logo[i];
        const int jawTop = i < 4 ? UpperJawTop : LowerJawTop;
        const int stretch = i == MonsterLetter ? pose.stretch : 0;
        const int top = jawTop + letterBob(waveTime, i) +
                        (i == MonsterLetter ? pose.shake : 0);
        int left = letter.x - stretch / 2;
        if (i == MonsterLetter - 1) left -= pose.stretch / 2;
        if (i == MonsterLetter + 1) left += pose.stretch / 2;
        const int width = letter.width + stretch;

        for (int column = 0; column < width; ++column) {
            const int source = stretch == 0 ? column : column * letter.width / width;
            for (int row = 0; row < letter.height; ++row) {
                if (letter.mask[source][row]) {
                    rectangle(left + column + 2, top + row + 4, 1, 1, TitleShadow);
                }
            }
        }
        for (int column = 0; column < width; ++column) {
            const int source = stretch == 0 ? column : column * letter.width / width;
            for (int row = 0; row < letter.height; ++row) {
                if (letter.mask[source][row]) {
                    const bool upperEdge = row < 2 || !letter.mask[source][row - 2];
                    rectangle(left + column, top + row, 1, 1, upperEdge ? TitleHighlight : TitleRed);
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

constexpr u32 LipShadow = rgb(56, 9, 31);
constexpr u32 LipRed = rgb(149, 25, 50);
constexpr u32 LipLight = rgb(226, 57, 70);
constexpr u32 TongueDark = rgb(95, 20, 54);
constexpr u32 TonguePink = rgb(217, 56, 98);
constexpr u32 TongueLight = rgb(255, 119, 137);

int roundedHalfSpan(int offset, int halfWidth, int halfHeight, int radius)
{
    // The sides stay flat until the scan line reaches a rounded corner.
    const int corner = std::clamp(std::abs(offset) - (halfHeight - radius), 0, radius);
    return halfWidth - radius + std::lround(std::sqrt(radius * radius - corner * corner));
}

void roundedShape(int x, int y, int rx, int ry, int radius, u32 color)
{
    radius = std::min({radius, rx, ry});
    for (int dy = -ry; dy <= ry; ++dy) {
        const int half = roundedHalfSpan(dy, rx, ry, radius);
        rectangle(x - half, y + dy, half * 2 + 1, 1, color);
    }
}

void drawFuzzyHand(int x, int y, int side, float reach, int stretch)
{
    if (reach < 0.05f) {
        return;
    }
    constexpr u32 FurShade = rgb(22, 74, 45);
    constexpr u32 FurGreen = rgb(112, 174, 61);
    constexpr u32 FurLight = rgb(183, 218, 88);
    const int palmX = x + side * std::lround((11 + stretch / 2) * reach);
    const int palmY = y - std::lround(5 * reach);
    const int palmSize = std::max(1, static_cast<int>(std::lround(5 * reach)));

    // The wrist stays inside the O as the fingers reach over its rim.
    for (int i = 0; i < 7; ++i) {
        const int wristX = x + (palmX - x) * i / 7;
        const int wristY = y + 4 + (palmY - y - 4) * i / 7;
        roundedShape(wristX, wristY, 2, palmSize, 2, FurShade);
    }
    roundedShape(palmX, palmY, palmSize + 1, palmSize + 1, 4, FurShade);
    roundedShape(palmX, palmY - 1, palmSize, palmSize, 3, FurGreen);
    for (int tuft = -1; tuft <= 1; ++tuft) {
        const int tuftX = palmX + tuft * palmSize / 2;
        line(tuftX, palmY - palmSize + 2, tuftX - side * 2, palmY - palmSize - 2, FurLight);
    }

    for (int finger = 0; finger < 3; ++finger) {
        const int fingerY = palmY - palmSize + 1 + finger * palmSize;
        const int tipX = palmX + side * (palmSize + (finger == 1 ? 4 : 2));
        line(palmX, palmY + finger - 1, tipX, fingerY, FurShade);
        line(palmX, palmY + finger - 2, tipX, fingerY - 1, FurGreen);
        rectangle(tipX - 1, fingerY - 1, 3, 2, Cream);
        rectangle(tipX + side, fingerY + 1, 1, 2, Cream);
    }
}

void drawStruggle(float waveTime, const StrugglePose& pose)
{
    if (pose.leftGrip < 0.05f && pose.rightGrip < 0.05f) {
        return;
    }
    const LogoLetter& letter = logo[MonsterLetter];
    const int x = letter.x + letter.width / 2;
    const int y = LowerJawTop + letter.height / 2 + letterBob(waveTime, MonsterLetter) + pose.shake;
    roundedShape(x, y, 3 + pose.stretch / 5, 9, 3, LipShadow);
    drawFuzzyHand(x, y, -1, pose.leftGrip, pose.stretch);
    drawFuzzyHand(x, y, 1, pose.rightGrip, pose.stretch);
}

void drawFang(int x, int root, int width, int length, int direction, int lean)
{
    for (int row = 0; row < length; ++row) {
        const int half = std::max(1, width * (length - row) / length / 2);
        const int center = x + lean * row / length;
        const int y = root + row * direction;
        rectangle(center - half, y, half * 2, 1, rgb(173, 135, 132));
        rectangle(center - half + 1, y, half, 1, Cream);
    }
}

struct Mouth {
    int x;
    int y;
    int width;
    int height;
};

Mouth monsterMouth(float waveTime, float burst, float lunge)
{
    const LogoLetter& letter = logo[MonsterLetter];
    const int x = letter.x + letter.width / 2;
    const int y = LowerJawTop + letter.height / 2 + letterBob(waveTime, MonsterLetter);
    // Break through the O, then grow again as the tongue rushes into the foreground.
    const float growth = easeBetween(burst, 0.0f, 0.86f);
    return {x + std::lround(78 * growth + 18 * lunge),
            y - std::lround(34 * growth - 5 * lunge),
            5 + std::lround(110 * growth + 30 * lunge),
            8 + std::lround(72 * growth + 22 * lunge)};
}

void drawMonster(const Mouth& mouth)
{
    const int x = mouth.x;
    const int y = mouth.y;
    const int rx = mouth.width;
    const int ry = mouth.height;
    roundedShape(x + 3, y + 5, rx + 3, ry + 2, 30, LipShadow);
    roundedShape(x, y, rx, ry, 28, LipRed);
    roundedShape(x - 2, y - ry / 8, rx * 9 / 10, ry * 5 / 6, 24, LipLight);
    const int cavityWidth = std::max(2, rx * 8 / 9);
    const int cavityHeight = std::max(2, ry * 3 / 4);
    const int cavityCorner = std::min({19, cavityWidth, cavityHeight});
    roundedShape(x, y + 2, cavityWidth, cavityHeight, cavityCorner, LipShadow);
    roundedShape(x + 2, y + 1, cavityWidth * 9 / 10, cavityHeight * 7 / 8, 16, Black);

    if (rx < 20) {
        return;
    }
    constexpr int Lengths[] = {14, 31, 17, 12, 15, 27, 17};
    for (int i = 0; i < 7; ++i) {
        const int offset = std::lround(cavityWidth * (i - 3) * 0.27f);
        const int toothX = x + offset;
        const int upperEdge = roundedHalfSpan(offset, cavityHeight, cavityWidth, cavityCorner);
        const int lowerEdge = roundedHalfSpan(offset + 4, cavityHeight, cavityWidth, cavityCorner);
        const int width = std::max(3, rx / (i % 2 == 0 ? 7 : 6));
        const int length = Lengths[i] * ry / 68;
        drawFang(toothX, y + 3 - upperEdge, width, length, 1, (3 - i) * 2);
        drawFang(toothX + 4, y + lowerEdge, width - 2, length * 3 / 4, -1, (3 - i) * 2);
    }
}

void drawTongue(const Mouth& mouth, float sweep)
{
    // The tip gets broader than the root as it gets closer to the viewer.
    const float reach = easeBetween(sweep, 0.0f, 0.48f);
    const int tip = mouth.x + std::lround((Width + 28 - mouth.x) * reach);
    for (int x = mouth.x; x < std::min(tip, Width); ++x) {
        const float along = (x - mouth.x) / static_cast<float>(tip - mouth.x);
        const int y = mouth.y + 29 + std::lround(
            std::sin(along * 3.8f - sweep * 6.0f) * 36.0f * along);
        const float cap = std::sqrt(std::min(1.0f, (1.0f - along) * 9.0f));
        const float thickness = 13 + along * (4 + 62 * reach);
        const int half = std::max(1, static_cast<int>(std::lround(thickness * cap)));
        rectangle(x, y - half, 1, half * 2, TongueDark);
        rectangle(x, y - half + 3, 1, std::max(1, half * 2 - 9), TonguePink);
        rectangle(x, y - half / 2, 1, std::max(1, half / 5), TongueLight);
    }

    // The near edge rolls back across the view from the side the tip reached.
    const float wipe = std::clamp((sweep - 0.39f) / 0.61f, 0.0f, 1.0f);
    if (wipe > 0.0f) {
        for (int y = 0; y < Height; ++y) {
            const float distance = (y - 134) / 134.0f;
            const float bend = distance * distance * 83 + std::sin(y * 0.018f + sweep * 4.0f) * 16;
            const int edge = std::lround(Width + 20 - (Width + 186) * wipe + bend);
            rectangle(edge, y, Width - edge, 1, Black);
            rectangle(edge, y, 65, 1, TongueDark);
            rectangle(edge + 4, y, 49, 1, TonguePink);
            rectangle(edge + 13, y, 9, 1, TongueLight);
        }
    }
}
} // namespace

void prepareTitle()
{
    drawSeaside();
    prepareLogo();
}

// Returning false exits the application.
bool updateTitle(TitleState& title, u32 buttons, float dt)
{
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
            title.screen = Screen::Struggle;
            title.sceneTime = 0.0f;
        }
    } else {
        title.sceneTime += dt;
        if (title.screen == Screen::Struggle && title.sceneTime >= StruggleSeconds) {
            title.screen = Screen::Burst;
            title.sceneTime = 0.0f;
        } else if (title.screen == Screen::Burst && title.sceneTime >= MouthBurstSeconds) {
            title.screen = Screen::Sweep;
            title.sceneTime = 0.0f;
        } else if (title.screen == Screen::Sweep && title.sceneTime >= TongueSweepSeconds) {
            title.screen = Screen::Finished;
        }
    }
    return true;
}

void drawTitle(const TitleState& title)
{
    if (title.screen == Screen::Finished) {
        rectangle(0, 0, Width, Height, Black);
        return;
    }

    std::memcpy(pixels, seaside, sizeof pixels);
    drawMenu(title.selected);
    StrugglePose pose{};
    if (title.screen == Screen::Struggle) {
        pose = strugglePose(title.sceneTime, 1.0f);
    } else if (title.screen == Screen::Burst) {
        const float release = 1.0f - easeBetween(title.sceneTime, 0.0f, MouthBurstSeconds * 0.28f);
        pose = strugglePose(StruggleSeconds, release);
    }
    drawLogo(title.waveTime, pose);
    drawStruggle(title.waveTime, pose);
    if (title.screen == Screen::Burst || title.screen == Screen::Sweep) {
        const bool sweeping = title.screen == Screen::Sweep;
        const float burst = sweeping ? 1.0f : title.sceneTime / MouthBurstSeconds;
        const float sweep = sweeping ? title.sceneTime / TongueSweepSeconds : 0.0f;
        const Mouth mouth = monsterMouth(title.waveTime, burst, easeBetween(sweep, 0.0f, 0.40f));
        drawMonster(mouth);
        if (sweeping) {
            drawTongue(mouth, sweep);
        }
    }
}
