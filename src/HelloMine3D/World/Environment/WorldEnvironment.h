#ifndef WORLD_ENVIRONMENT_H_INCLUDED
#define WORLD_ENVIRONMENT_H_INCLUDED

#include "../../Maths/glm.h"

struct WorldEnvironmentState {
    float cycle = 0.f;
    float daylight = 1.f;
    float fogDensity = 0.0015f;
    glm::vec3 fogColour{0.58f, 0.75f, 0.92f};
    glm::vec3 skyTint{1.f};
};

class WorldEnvironment {
  public:
    static constexpr int TicksPerDay = 24000;

    static WorldEnvironmentState evaluate(float worldTime);
};

#endif
