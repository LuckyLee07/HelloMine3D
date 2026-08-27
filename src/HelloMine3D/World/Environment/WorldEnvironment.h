#ifndef WORLD_ENVIRONMENT_H_INCLUDED
#define WORLD_ENVIRONMENT_H_INCLUDED

#include "../../Maths/glm.h"

struct WorldEnvironmentState {
    static constexpr int AtmosphereContractVersion = 1;

    float cycle = 0.f;
    float daylight = 1.f;
    float fogDensity = 0.0015f;
    glm::vec3 fogColour{0.58f, 0.75f, 0.92f};
    glm::vec3 fogSunwardColour{0.78f, 0.68f, 0.56f};
    float fogDirectionalStrength = 0.12f;
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
    float cloudBaseHeight = 168.f;
    float cloudThickness = 24.f;
    float cloudHorizontalScale = 92.f;
    glm::vec2 cloudVelocity{1.6f, 0.55f};
    float cloudMaxDistance = 2400.f;
    glm::vec3 waterShallowColour{0.12f, 0.46f, 0.56f};
    glm::vec3 waterDeepColour{0.025f, 0.18f, 0.28f};
};

struct CloudRayInterval {
    bool visible = false;
    bool cameraInside = false;
    float nearDistance = 0.f;
    float farDistance = 0.f;
};

class WorldEnvironment {
  public:
    static constexpr int TicksPerDay = 24000;

    static WorldEnvironmentState evaluate(float worldTime);
    static glm::vec3 directionalFogColour(
        const WorldEnvironmentState &state,
        const glm::vec3 &viewDirection);
    static glm::vec2 cloudOffset(
        const WorldEnvironmentState &state, float elapsedSeconds);
    static CloudRayInterval cloudRayInterval(
        const WorldEnvironmentState &state,
        const glm::vec3 &cameraPosition,
        const glm::vec3 &viewDirection);
};

#endif
