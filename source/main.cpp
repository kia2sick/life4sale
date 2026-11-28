#include "drawing.h"
#include "newspaper_scene.h"
#include "title_screen.h"

#include <algorithm>
#include <cstring>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

namespace {
#ifdef PREVIEW_NEWSPAPER
constexpr bool PreviewNewspaper = true;
#else
constexpr bool PreviewNewspaper = false;
#endif

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

    if (!PreviewNewspaper) {
        prepareTitle();
    }
    prepareNewspaper();
    TitleState title;
    NewspaperScene newspaper;
    bool showNewspaper = PreviewNewspaper;
    int backBuffer = 1;
    if (showNewspaper) {
        drawNewspaper(newspaper);
    } else {
        drawTitle(title);
    }
    VIDEO_SetBlack(FALSE);
    present(*videoMode, framebuffers[0]);

    u64 previousTime = gettime();
    while (true) {
        const u64 now = gettime();
        const float dt = std::min(ticks_to_microsecs(now - previousTime) / 1000000.0f, 0.1f);
        previousTime = now;

        const u32 buttons = readButtons();
        if (buttons & WPAD_BUTTON_HOME) {
            break;
        }

        if (showNewspaper && (buttons & WPAD_BUTTON_B)) {
            title = TitleState{};
            newspaper = NewspaperScene{};
            showNewspaper = PreviewNewspaper;
        }

        if (showNewspaper) {
            updateNewspaper(newspaper, dt);
            drawNewspaper(newspaper);
        } else {
            if (!updateTitle(title, buttons, dt)) {
                break;
            }
            drawTitle(title);
            // Present the black frame before cutting to the newspaper on the next frame.
            showNewspaper = title.screen == Screen::Finished;
        }
        present(*videoMode, framebuffers[backBuffer]);
        backBuffer = 1 - backBuffer;
    }
    return 0;
}
