#include "newspaper_scene.h"
#include "drawing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace {
constexpr float ReadTime = 1.0f;
constexpr float RippleDuration = 1.42f;
constexpr float HoldAfterRipple = 0.65f;
constexpr float Pi = 3.14159265f;

// Fixed camera. World X/Z lie on the pavement and Y is the height above it.
constexpr float EyeHeight = 155.0f;
constexpr float Down = 0.700f;
constexpr float Forward = 0.714f;
constexpr float FocalLength = 238.0f;
constexpr float HorizonX = 160.0f;
constexpr float CenterY = 117.0f;

constexpr u32 Ink = rgb(31, 31, 31);
constexpr u32 Paper = rgb(225, 225, 225);
constexpr u32 Asphalt = rgb(55, 59, 65);
constexpr u32 Water = rgb(56, 76, 84);
constexpr u32 WaterRim = rgb(99, 118, 119);

struct GroundPoint { float x, z; };
struct Point3 { float x, y, z; };
struct ScreenPoint { float x, y, inverseDepth; };

struct Footfall {
    GroundPoint entry, plant, exit;
    float swingStart, contact, toeOff, liftOff, gone;
};

constexpr Footfall First{
    {-244.0f, 332.0f}, {-58.0f, 163.0f}, {76.0f, -9.0f},
    0.0f, 0.56f, 0.91f, 1.15f, 1.76f
};
constexpr Footfall Second{
    {-248.0f, 312.0f}, {21.0f, 101.0f}, {82.0f, -11.0f},
    0.47f, 1.12f, 1.38f, 1.56f, 2.14f
};

u32 stillFrame[Height][Width];
u32 pageArt[Height][Width];
GroundPoint groundAtPixel[Height][Width];
bool wet[Height][Width];
u16 rippleDistance[2][Height][Width];
float depthBuffer[Height][Width];

ScreenPoint project(Point3 point)
{
    const float drop = EyeHeight - point.y;
    const float depth = Down * drop + Forward * point.z;
    if (depth < 8.0f) {
        return {};
    }
    const float scale = FocalLength / depth;
    return {HorizonX + point.x * scale,
            CenterY + (Forward * drop - Down * point.z) * scale,
            1.0f / depth};
}

GroundPoint unprojectGround(int x, int y)
{
    const float ray = (y + 0.5f - CenterY) / FocalLength;
    const float depth = EyeHeight / (Forward * ray + Down);
    return {depth * (x + 0.5f - HorizonX) / FocalLength, depth * (Forward - Down * ray)};
}

u32 tint(u32 color, u32 reflection, int amount)
{
    // Mix each byte of the Wii's packed color separately.
    u32 result = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const int value = (color >> shift) & 255;
        const int light = (reflection >> shift) & 255;
        result |= static_cast<u32>(value + (light - value) * amount / 256) << shift;
    }
    return result;
}

unsigned grain(int x, int z)
{
    unsigned seed = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(z) * 668265263u;
    seed = (seed ^ (seed >> 13)) * 1274126177u;
    return seed ^ (seed >> 16);
}

u32 drawGroundPixel(GroundPoint p, bool& inWater)
{
    const int x = static_cast<int>(std::floor(p.x));
    const int z = static_cast<int>(std::floor(p.z));
    const unsigned grit = grain(x / 2, z / 2);
    const int variation = static_cast<int>(grit % 12) - 5;
    const float u = (p.x - 7.0f - 15.0f * std::sin(p.z * 0.033f)) / 158.0f;
    const float v = (p.z - 169.0f) / 108.0f;
    const float edge = u * u + v * v + 0.033f * std::sin(p.z * 0.29f + p.x * 0.04f);
    inWater = edge < 0.974f;
    if (inWater) {
        if (edge > 0.934f) {
            return tint(WaterRim, Water, 105 + grit % 80);
        }
        const int reflection = static_cast<int>(p.x + 0.38f * p.z + 4 * std::sin(p.z * 0.31f)) + 500;
        const int stripe = reflection % 44;
        const int gleam = stripe < 11 && z % 19 > 3 ? 12 : 0;
        return rgb(49 + variation + gleam, 66 + variation + gleam, 77 + variation + gleam);
    }
    if (edge < 1.025f) {
        return rgb(43 + variation, 52 + variation, 56 + variation);
    }
    const int seam = ((x + z / 5) % 122 + 122) % 122;
    if (seam < 2 || z % 83 < 2) {
        return rgb(41 + variation, 46 + variation, 50 + variation);
    }
    const int fleck = grit % 97 == 0 ? 25 : 0;
    return rgb(66 + variation + fleck, 68 + variation + fleck, 73 + variation + fleck);
}

void drawStorefrontPhoto()
{
    constexpr u32 PhotoDark = rgb(46, 46, 44);
    constexpr u32 PhotoLight = rgb(160, 159, 150);
    rectangle(49, 62, 164, 49, PhotoDark);
    for (int shop = 0; shop < 3; ++shop) {
        const int x = 54 + shop * 52;
        const int y = 68 + shop * 2;
        rectangle(x, y, 48, 33, rgb(112, 113, 107));
        rectangle(x + 2, y, 43, 4, PhotoLight);
        rectangle(x + 3, y + 7, 39, 21, rgb(67, 69, 65));
        for (int slat = 8; slat < 28; slat += 3) {
            rectangle(x + 3, y + slat, 39, 1, rgb(124, 124, 116));
        }
        rectangle(x + 11, y + 14, 21, 6, PhotoLight);
        rectangle(x + 13, y + 16, 16, 1, PhotoDark);
    }
    line(50, 105, 211, 106, PhotoLight);
    for (int i = 0; i < 178; ++i) {
        const int x = 50 + (i * 37 + i / 7) % 162;
        const int y = 63 + (i * 17 + i / 9) % 47;
        rectangle(x, y, 1 + i % 2, 1, i % 4 == 0 ? PhotoLight : PhotoDark);
    }
}

void drawPaperArtwork()
{
    rectangle(43, 36, 180, 165, rgb(20, 47, 60));
    rectangle(39, 32, 180, 165, rgb(169, 169, 169));
    rectangle(39, 32, 179, 163, Paper);
    rectangle(40, 193, 177, 2, rgb(194, 194, 194));

    for (int i = 0; i < 135; ++i) {
        const int x = 42 + (i * 39) % 152;
        const int y = 46 + (i * 17) % 141;
        rectangle(x, y, 1 + i % 4, 1, rgb(198, 198, 198));
    }

    rectangle(48, 40, 163, 17, Ink);
    drawText("MARROW POST", 67, 42, 2, Paper);
    rectangle(48, 59, 163, 1, Ink);
    drawStorefrontPhoto();
    drawText("MASSACRE", 55, 116, 3, Ink);
    drawText("MASSACRE", 56, 116, 3, Ink);
    rectangle(48, 143, 163, 2, Ink);
    drawText("CITY HALL SCANDAL", 48, 148, 1, Ink);
    for (int row = 0; row < 9; ++row) {
        rectangle(48, 158 + row * 3, 34 - (row * 7) % 10, 1, rgb(109, 109, 109));
    }
    rectangle(85, 157, 1, 34, rgb(136, 136, 136));
    rectangle(89, 157, 126, 35, Ink);
    rectangle(90, 158, 124, 33, Paper);
    drawText("WELCOME TO", 94, 160, 2, Ink);
    drawText("MARROW BAY!", 91, 176, 2, Ink);
}

u32 paperPixel(GroundPoint p, u32 ground)
{
    // Map the entire rectangular print onto the same ground plane as the puddle.
    constexpr float AcrossX = 0.862f;
    constexpr float AcrossZ = 0.507f;
    const float x = p.x + 50.0f;
    const float z = p.z - 147.0f;
    const int pageX = 129 + std::lround((AcrossX * x + AcrossZ * z) * 1.19f);
    const int pageY = 115 + std::lround((AcrossZ * x - AcrossX * z) * 1.26f);
    if (pageX >= 0 && pageX < Width && pageY >= 0 && pageY < Height) {
        const u32 print = pageArt[pageY][pageX];
        if (print != 0) {
            return print;
        }
    }
    return ground;
}

void drawRipples(float elapsed)
{
    constexpr float Contacts[] = {First.contact, Second.contact};
    for (int i = 0; i < 2; ++i) {
        const float age = elapsed - Contacts[i];
        if (age < 0.0f || age >= RippleDuration) {
            continue;
        }
        const float progress = age / RippleDuration;
        const int front = 9 + std::lround(240 * progress);
        const float strength = (1.0f - progress) * (i == 0 ? 0.63f : 0.9f);
        for (int y = 0; y < Height; ++y) {
            for (int x = 0; x < Width; ++x) {
                const int behind = front - rippleDistance[i][y][x];
                if (behind < 0 || behind >= 34 || !wet[y][x]) {
                    continue;
                }
                const int crest = behind % 12;
                const float fade = strength * (1.0f - behind / 36.0f);
                const int shift = std::lround((crest < 5 ? 4 : -2) * fade * (0.55f + y / 260.0f));
                const int sample = std::clamp(x + shift, 0, Width - 1);
                const int shine = std::lround((crest < 3 ? 175 : 27) * fade);
                pixels[y][x] = tint(stillFrame[y][sample],
                                    crest < 3 ? rgb(172, 204, 201) : Water, shine);
            }
        }
    }
}

void drawSplash(GroundPoint origin, float age, float size)
{
    if (age < 0.0f || age >= 0.30f) {
        return;
    }
    const float t = age / 0.30f;
    for (int i = 0; i < 11; ++i) {
        const float angle = i * 2.4f;
        const float spread = (14 + t * (19 + i % 4 * 5)) * size;
        const float height = (12 + i % 4 * 3) * 4 * t * (1.0f - t) * size;
        const float x = origin.x + std::cos(angle) * spread;
        const float z = origin.z + std::sin(angle) * spread;
        const ScreenPoint drop = project({x, height, z});
        const ScreenPoint tail = project({x - std::cos(angle) * 4, height - 2.0f, z});
        if (drop.inverseDepth > 0.0f && tail.inverseDepth > 0.0f) {
            const u32 color = tint(rgb(186, 211, 205), Water, std::lround(t * 205));
            line(std::lround(tail.x), std::lround(tail.y),
                 std::lround(drop.x), std::lround(drop.y), color);
        }
    }
}

float edge(ScreenPoint a, ScreenPoint b, float x, float y)
{
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

void triangle(ScreenPoint a, ScreenPoint b, ScreenPoint c, u32 color)
{
    if (a.inverseDepth == 0.0f || b.inverseDepth == 0.0f || c.inverseDepth == 0.0f) {
        return;
    }
    const float area = edge(a, b, c.x, c.y);
    if (std::abs(area) < 0.01f) {
        return;
    }
    if (area < 0.0f) {
        std::swap(b, c);
    }

    const int left = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int right = std::min(Width, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int top = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int bottom = std::min(Height, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
    const float areaInverse = 1.0f / std::abs(area);
    for (int y = top; y < bottom; ++y) {
        const float row = y + 0.5f;
        float wa = edge(b, c, left + 0.5f, row);
        float wb = edge(c, a, left + 0.5f, row);
        float wc = edge(a, b, left + 0.5f, row);
        for (int x = left; x < right; ++x) {
            if (wa >= 0.0f && wb >= 0.0f && wc >= 0.0f) {
                const float depth = (wa * a.inverseDepth + wb * b.inverseDepth + wc * c.inverseDepth) * areaInverse;
                if (depth > depthBuffer[y][x]) {
                    depthBuffer[y][x] = depth;
                    pixels[y][x] = color;
                }
            }
            wa += b.y - c.y;
            wb += c.y - a.y;
            wc += a.y - b.y;
        }
    }
}

struct BootPose {
    GroundPoint heel;
    float lift;
    float pitch;
    float shinLean;
};

constexpr float AcrossX = 0.898f;
constexpr float AcrossZ = 0.44f;
constexpr float AheadX = 0.44f;
constexpr float AheadZ = -0.898f;

ScreenPoint bootVertex(const BootPose& boot, float across, float along, float up)
{
    // During push-off the toe is the pivot, so the heel lifts without dragging.
    const float pivot = boot.pitch < 0.0f ? 40.0f : -19.0f;
    const float fromPivot = along - pivot;
    const float cosine = std::cos(boot.pitch);
    const float sine = std::sin(boot.pitch);
    const float forward = pivot + fromPivot * cosine - up * sine;
    const float height = boot.lift + up * cosine + fromPivot * sine;
    return project({boot.heel.x + across * AcrossX + forward * AheadX, height,
                    boot.heel.z + across * AcrossZ + forward * AheadZ});
}

constexpr unsigned RingSize = 12;
using Ring = std::array<ScreenPoint, RingSize>;

struct OutlinePoint { float across, along; };
constexpr OutlinePoint Footprint[RingSize] = {
    {-8,-20}, {7,-20}, {13,-15}, {15,-4}, {18,17}, {17,31},
    {9,40}, {-5,42}, {-14,36}, {-17,25}, {-15,9}, {-13,-11}
};

Ring soleRing(const BootPose& pose, float size, float up, float vamp = 0.0f)
{
    Ring ring;
    for (unsigned i = 0; i < RingSize; ++i) {
        const OutlinePoint p = Footprint[i];
        const float top = up + vamp * std::clamp((37.0f - p.along) / 53.0f, 0.0f, 1.0f);
        ring[i] = bootVertex(pose, p.across * size, p.along * size, top);
    }
    return ring;
}

Ring calfRing(const BootPose& pose, float width, float length, float along, float height, float fold = 0.0f)
{
    Ring ring;
    for (unsigned i = 0; i < RingSize; ++i) {
        const float angle = 2.0f * Pi * i / RingSize - 2.09f;
        const float x = std::cos(angle);
        const float z = std::sin(angle);
        ring[i] = bootVertex(pose, width * x, along + length * z,
                             height + fold * std::sin(angle * 3.0f + 0.4f));
    }
    return ring;
}

void band(const Ring& lower, const Ring& upper, u32 light, u32 dark)
{
    constexpr int Shade[RingSize] = {186, 202, 180, 132, 72, 22, 13, 24, 59, 90, 125, 159};
    for (unsigned i = 0; i < RingSize; ++i) {
        const unsigned next = (i + 1) % RingSize;
        const u32 color = tint(light, dark, Shade[i]);
        triangle(lower[i], lower[next], upper[next], color);
        triangle(lower[i], upper[next], upper[i], color);
    }
}

void cap(const Ring& ring, ScreenPoint center, u32 color)
{
    for (unsigned i = 0; i < RingSize; ++i) {
        triangle(center, ring[i], ring[(i + 1) % RingSize], color);
    }
}

void drawBoot(const BootPose& boot)
{
    const Ring tread = soleRing(boot, 0.96f, 0.5f);
    const Ring bumper = soleRing(boot, 1.03f, 4.5f);
    const Ring welt = soleRing(boot, 1.00f, 9.0f);
    const Ring rubber = soleRing(boot, 0.94f, 15.0f, 8.0f);
    const Ring vamp = soleRing(boot, 0.76f, 21.0f, 10.0f);
    band(tread, bumper, rgb(60, 54, 47), rgb(23, 23, 22));
    band(bumper, welt, rgb(91, 79, 63), rgb(37, 36, 31));
    band(welt, rubber, rgb(67, 91, 84), rgb(25, 44, 44));
    band(rubber, vamp, rgb(88, 112, 99), rgb(33, 54, 51));
    cap(vamp, bootVertex(boot, 0, 5, 30), rgb(67, 92, 83));

    const Ring ankle = calfRing(boot, 12, 13, -7, 25);
    const Ring shin = calfRing(boot, 15, 13, -12, 46);
    const Ring cuffBase = calfRing(boot, 18, 15, -15, 53, 1.4f);
    const Ring cuffTop = calfRing(boot, 20, 16, -18, 64, 1.6f);
    const Ring inner = calfRing(boot, 16, 13, -18, 62);
    band(ankle, shin, rgb(78, 103, 93), rgb(31, 51, 50));
    band(shin, cuffBase, rgb(53, 77, 74), rgb(23, 36, 38));
    band(cuffBase, cuffTop, rgb(83, 105, 95), rgb(36, 53, 48));
    band(cuffTop, inner, rgb(112, 132, 119), rgb(43, 62, 59));
    cap(inner, bootVertex(boot, 0, -18, 62), rgb(22, 35, 36));

    const float lean = boot.shinLean;
    const Ring trouserHem = calfRing(boot, 12, 10, -19, 59, 1.8f);
    const Ring trouserFold = calfRing(boot, 15, 12, -22 + lean * 0.33f, 79, 3.2f);
    const Ring shinFold = calfRing(boot, 17, 13, -24 + lean * 0.60f, 99, 3.5f);
    const Ring knee = calfRing(boot, 20, 15, -25 + lean, 133, 2.3f);
    const Ring frameCut = calfRing(boot, 19, 15, -27 + lean, 167);
    band(trouserHem, trouserFold, rgb(174, 165, 143), rgb(97, 92, 79));
    band(trouserFold, shinFold, rgb(209, 199, 171), rgb(122, 113, 91));
    band(shinFold, knee, rgb(190, 181, 154), rgb(108, 105, 88));
    band(knee, frameCut, rgb(199, 190, 165), rgb(116, 108, 91));
}

void drawBootShadow(const BootPose& boot)
{
    const ScreenPoint center = project({boot.heel.x + 8.0f, 0, boot.heel.z - 4.0f});
    if (center.inverseDepth == 0.0f) {
        return;
    }
    const int radius = std::lround((46.0f + boot.lift * 0.35f) * FocalLength * center.inverseDepth);
    const int left = std::max(0, static_cast<int>(center.x) - radius);
    const int right = std::min(Width, static_cast<int>(center.x) + radius);
    const int top = std::max(0, static_cast<int>(center.y) - radius);
    const int bottom = std::min(Height, static_cast<int>(center.y) + radius);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const GroundPoint p = groundAtPixel[y][x];
            const float dx = p.x - boot.heel.x - 4 - boot.lift * 0.32f;
            const float dz = p.z - boot.heel.z + 6 + boot.lift * 0.3f;
            const float across = dx * AcrossX + dz * AcrossZ;
            const float along = dx * AheadX + dz * AheadZ;
            const float distance = across * across / 560.0f + along * along / 1730.0f;
            if (distance < 1.0f) {
                pixels[y][x] = tint(pixels[y][x], Black,
                                    std::lround((1.0f - distance) * (172.0f - 2.6f * boot.lift)));
            }
        }
    }
}

float ease(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

GroundPoint interpolate(GroundPoint from, GroundPoint to, float t)
{
    return {from.x + (to.x - from.x) * t, from.z + (to.z - from.z) * t};
}

BootPose poseAt(const Footfall& foot, float elapsed)
{
    if (elapsed < foot.contact) {
        const float t = ease((elapsed - foot.swingStart) / (foot.contact - foot.swingStart));
        const float swing = std::sin(Pi * t);
        return {interpolate(foot.entry, foot.plant, t),
                18.0f * swing, 0.30f * swing * (1.0f - 2.0f * t), -6.0f + 11.0f * t};
    }
    if (elapsed < foot.liftOff) {
        const float roll = ease((elapsed - foot.toeOff) / (foot.liftOff - foot.toeOff));
        return {foot.plant, 0.0f, -0.36f * roll, 5.0f + 9.0f * roll};
    }
    const float t = ease((elapsed - foot.liftOff) / (foot.gone - foot.liftOff));
    return {interpolate(foot.plant, foot.exit, t),
            19.0f * std::sin(Pi * t), -0.36f * (1.0f - t), 14.0f - 10.0f * t};
}

void drawFootstep(float elapsed)
{
    drawRipples(elapsed);
    const bool showFirst = elapsed < First.gone;
    const bool showSecond = elapsed < Second.gone;
    if (showFirst || showSecond) {
        const BootPose firstBoot = poseAt(First, elapsed);
        const BootPose secondBoot = poseAt(Second, elapsed);
        if (showFirst) drawBootShadow(firstBoot);
        if (showSecond) drawBootShadow(secondBoot);
        std::memset(depthBuffer, 0, sizeof depthBuffer);
        if (showFirst) drawBoot(firstBoot);
        if (showSecond) drawBoot(secondBoot);
    }
    drawSplash(First.plant, elapsed - First.contact, 0.57f);
    drawSplash(Second.plant, elapsed - Second.contact, 0.86f);
}

void startFootstep(NewspaperScene& scene)
{
    scene.footstepStarted = true;
    scene.footstepElapsed = 0.0f;
}
} // namespace

void prepareNewspaper()
{
    rectangle(0, 0, Width, Height, 0);
    drawPaperArtwork();
    std::memcpy(pageArt, pixels, sizeof pageArt);

    constexpr GroundPoint Contacts[] = {First.plant, Second.plant};
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            const GroundPoint p = unprojectGround(x, y);
            groundAtPixel[y][x] = p;
            const u32 ground = drawGroundPixel(p, wet[y][x]);
            stillFrame[y][x] = paperPixel(p, ground);
            for (int i = 0; i < 2; ++i) {
                const float dx = p.x - Contacts[i].x;
                const float dz = p.z - Contacts[i].z;
                rippleDistance[i][y][x] = std::lround(std::sqrt(dx * dx + dz * dz));
            }
        }
    }
}

void updateNewspaper(NewspaperScene& scene, float dt)
{
    scene.elapsed += dt;
    if (scene.footstepStarted) {
        constexpr float EndTime = Second.contact + RippleDuration + HoldAfterRipple;
        scene.footstepElapsed = std::min(scene.footstepElapsed + dt, EndTime);
    }

    if (scene.elapsed >= ReadTime && !scene.footstepStarted) {
        startFootstep(scene);
    }
}

void drawNewspaper(const NewspaperScene& scene)
{
    std::memcpy(pixels, stillFrame, sizeof pixels);
    if (scene.footstepStarted) {
        drawFootstep(scene.footstepElapsed);
    }
}
