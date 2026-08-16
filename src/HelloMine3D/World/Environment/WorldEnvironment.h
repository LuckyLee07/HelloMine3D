#ifndef WORLD_ENVIRONMENT_H_INCLUDED
#define WORLD_ENVIRONMENT_H_INCLUDED

#include "../../Maths/glm.h"

struct WorldEnvironmentState {
    float cycle = 0.f;
    float daylight = 1.f;
    float fogDensity = 0.0015f;
    glm::vec3 fogColour{0.58f, 0.75f, 0.92f};
    glm::vec3 skyZenithColour{0.16f, 0.5f, 0.9f};
    glm::vec3 skyHorizonColour{0.58f, 0.75f, 0.92f};
    glm::vec3 sunDirection{0.f, 1.f, 0.f};
    glm::vec3 sunColour{1.f, 0.9f, 0.7f};
    float sunIntensity = 1.f;
    float moonIntensity = 0.f;
    float starIntensity = 0.f;
};

class WorldEnvironment {
  public:
    static constexpr int TicksPerDay = 24000;

    static WorldEnvironmentState evaluate(float worldTime);
};

#endif
