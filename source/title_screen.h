#pragma once

#include <gctypes.h>

enum class Screen { Title, Struggle, Burst, Sweep, Finished };

struct TitleState {
    Screen screen = Screen::Title;
    int selected = 0;
    float waveTime = 0.0f;
    float sceneTime = 0.0f;
};

void prepareTitle();
bool updateTitle(TitleState& title, u32 buttons, float dt);
void drawTitle(const TitleState& title);
