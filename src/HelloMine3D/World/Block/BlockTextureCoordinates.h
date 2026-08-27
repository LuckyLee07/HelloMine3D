#ifndef BLOCKTEXTURECOORDINATES_H_INCLUDED
#define BLOCKTEXTURECOORDINATES_H_INCLUDED

#include <array>

#include "TerrainMaterialProfile.h"

namespace BlockTextureCoordinates
{
    inline std::array<float, 8> get(
        int x, int y,
        const TerrainMaterialParameters &parameters) noexcept
    {
        const float imageSize =
            static_cast<float>(parameters.atlasPixels);
        const float texturesPerRow =
            static_cast<float>(parameters.tilesPerRow);
        const float normalizedTextureSize = 1.f / texturesPerRow;
        const float pixelSize = 1.f / imageSize;

        const float xMin = x * normalizedTextureSize + 0.5f * pixelSize;
        const float yMin = y * normalizedTextureSize + 0.5f * pixelSize;
        const float xMax = xMin + normalizedTextureSize - pixelSize;
        const float yMax = yMin + normalizedTextureSize - pixelSize;

        return {xMax, yMax, xMin, yMax, xMin, yMin, xMax, yMin};
    }

    inline std::array<float, 8> get(int x, int y) noexcept
    {
        return get(x, y,
                   runtimeTerrainMaterialProfile().parameters());
    }
} // namespace BlockTextureCoordinates

#endif // BLOCKTEXTURECOORDINATES_H_INCLUDED
