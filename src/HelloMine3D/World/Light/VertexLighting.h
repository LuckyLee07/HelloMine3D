#ifndef VERTEXLIGHTING_H_INCLUDED
#define VERTEXLIGHTING_H_INCLUDED

#include "LightLevel.h"

#include <array>
#include <cstdint>

/// The four light samples around one face vertex. The centre sample is the
/// voxel immediately outside the visible face; the other samples extend from
/// it along the two face tangents.
struct VertexLightCornerSamples {
    LightLevel centre = MIN_LIGHT_LEVEL;
    LightLevel sideU = MIN_LIGHT_LEVEL;
    LightLevel sideV = MIN_LIGHT_LEVEL;
    LightLevel diagonal = MIN_LIGHT_LEVEL;
    bool sideUOccludes = false;
    bool sideVOccludes = false;
    bool diagonalOccludes = false;
};

struct VertexLightCorner {
    float smoothLight = MIN_TERRAIN_BRIGHTNESS;
    float finalLight = MIN_TERRAIN_BRIGHTNESS;
    std::uint8_t ambientOcclusion = 0;
};

/// CPU-only V10A vertex-lighting result in canonical quad order:
/// bottom-left, bottom-right, top-right, top-left. It is derived mesh data and
/// is deliberately not part of the save, terrain, settings or vertex format.
struct VertexLightingQuad {
    std::array<VertexLightCorner, 4> corners{};
    bool flipDiagonal = false;
};

class VertexLighting {
  public:
    static constexpr int ContractVersion = 1;
    static constexpr float AmbientOcclusionStep = 0.12f;

    static std::uint8_t ambientOcclusion(bool sideUOccludes,
                                         bool sideVOccludes,
                                         bool diagonalOccludes) noexcept;

    static VertexLightCorner
    evaluateCorner(float cardinalLight,
                   const VertexLightCornerSamples &samples,
                   bool ambientOcclusionEnabled = true) noexcept;

    /// Chooses the 1-3 diagonal when it has lower endpoint interpolation
    /// error than the legacy 0-2 diagonal. Exact ties retain 0-2.
    static bool shouldFlipDiagonal(
        const std::array<VertexLightCorner, 4> &corners) noexcept;

    /// Piecewise-linear interpolation over the selected quad triangles.
    /// Corners use canonical order and x/y are normalized to [0, 1].
    static float interpolateQuad(const std::array<float, 4> &corners,
                                 bool flipDiagonal, float x,
                                 float y) noexcept;
};

#endif // VERTEXLIGHTING_H_INCLUDED
