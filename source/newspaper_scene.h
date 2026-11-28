#pragma once

struct NewspaperScene {
    float elapsed = 0.0f;
    bool footstepStarted = false;
    float footstepElapsed = 0.0f;
};

void prepareNewspaper();
void updateNewspaper(NewspaperScene& scene, float dt);
void drawNewspaper(const NewspaperScene& scene);
