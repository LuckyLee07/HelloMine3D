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
    glm::vec3 cloudLightColour{0.9f, 0.93f, 0.96f};
    glm::vec3 cloudShadowColour{0.46f, 0.56f, 0.64f};
    float cloudCoverage = 0.46f;
    glm::vec3 waterShallowColour{0.12f, 0.46f, 0.56f};
    glm::vec3 waterDeepColour{0.025f, 0.18f, 0.28f};
};

class WorldEnvironment {
  public:
    static constexpr int TicksPerDay = 24000;

    static WorldEnvironmentState evaluate(float worldTime);
};

#endif
