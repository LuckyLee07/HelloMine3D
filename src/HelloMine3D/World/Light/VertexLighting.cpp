#include "VertexLighting.h"

#include <algorithm>
#include <cmath>

namespace {
float diagonalError(const VertexLightCorner &first,
                    const VertexLightCorner &second) noexcept
{
    const float smoothError =
        std::abs(first.smoothLight - second.smoothLight);
    const float aoError = VertexLighting::AmbientOcclusionStep *
                          std::abs(static_cast<float>(first.ambientOcclusion) -
                                   static_cast<float>(second.ambientOcclusion));
    const float finalError = std::abs(first.finalLight - second.finalLight);
    return smoothError + aoError + finalError;
}
} // namespace

std::uint8_t VertexLighting::ambientOcclusion(bool sideUOccludes,
                                              bool sideVOccludes,
                                              bool diagonalOccludes) noexcept
{
    if (sideUOccludes && sideVOccludes) {
        return 3;
    }

    return static_cast<std::uint8_t>(sideUOccludes) +
           static_cast<std::uint8_t>(sideVOccludes) +
           static_cast<std::uint8_t>(diagonalOccludes);
}

VertexLightCorner VertexLighting::evaluateCorner(
    float cardinalLight, const VertexLightCornerSamples &samples,
    bool ambientOcclusionEnabled) noexcept
{
    const std::array<LightLevel, 4> levels = {
        samples.centre,
        samples.sideU,
        samples.sideV,
        samples.diagonal,
    };
    const std::array<bool, 4> occluding = {
        false,
        samples.sideUOccludes,
        samples.sideVOccludes,
        samples.diagonalOccludes,
    };

    float brightnessSum = 0.f;
    int brightnessCount = 0;
    for (std::size_t index = 0; index < levels.size(); ++index) {
        // Opaque neighbours contribute AO instead of also injecting their
        // usually-zero interior light into the average. This keeps the two
        // effects independent and prevents over-dark corners.
        if (!occluding[index]) {
            brightnessSum += lightLevelToBrightness(levels[index]);
            ++brightnessCount;
        }
    }

    VertexLightCorner result;
    result.smoothLight = brightnessCount == 0
                             ? MIN_TERRAIN_BRIGHTNESS
                             : brightnessSum /
                                   static_cast<float>(brightnessCount);
    result.ambientOcclusion = ambientOcclusionEnabled
                                  ? ambientOcclusion(
                                        samples.sideUOccludes,
                                        samples.sideVOccludes,
                                        samples.diagonalOccludes)
                                  : 0;
    const float aoFactor =
        1.f - AmbientOcclusionStep * result.ambientOcclusion;
    result.finalLight = std::clamp(
        cardinalLight * result.smoothLight * aoFactor, 0.f, 1.f);
    return result;
}

bool VertexLighting::shouldFlipDiagonal(
    const std::array<VertexLightCorner, 4> &corners) noexcept
{
    const float legacyError = diagonalError(corners[0], corners[2]);
    const float flippedError = diagonalError(corners[1], corners[3]);
    constexpr float epsilon = 0.000001f;
    return flippedError + epsilon < legacyError;
}

float VertexLighting::interpolateQuad(const std::array<float, 4> &corners,
                                      bool flipDiagonal, float x,
                                      float y) noexcept
{
    x = std::clamp(x, 0.f, 1.f);
    y = std::clamp(y, 0.f, 1.f);

    if (!flipDiagonal) {
        if (x >= y) {
            return corners[0] * (1.f - x) + corners[1] * (x - y) +
                   corners[2] * y;
        }
        return corners[0] * (1.f - y) + corners[2] * x +
               corners[3] * (y - x);
    }

    if (x + y <= 1.f) {
        return corners[0] * (1.f - x - y) + corners[1] * x +
               corners[3] * y;
    }
    return corners[1] * (1.f - y) + corners[2] * (x + y - 1.f) +
           corners[3] * (1.f - x);
}
