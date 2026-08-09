#ifndef BLOCKTEXTURECOORDINATES_H_INCLUDED
#define BLOCKTEXTURECOORDINATES_H_INCLUDED

#include <array>

namespace BlockTextureCoordinates
{
    inline std::array<float, 8> get(int x, int y) noexcept
    {
        constexpr float imageSize = 256.f;
        constexpr float individualTextureSize = 16.f;
        constexpr float texturesPerRow = imageSize / individualTextureSize;
        constexpr float normalizedTextureSize = 1.f / texturesPerRow;
        constexpr float pixelSize = 1.f / imageSize;

        const float xMin = x * normalizedTextureSize + 0.5f * pixelSize;
        const float yMin = y * normalizedTextureSize + 0.5f * pixelSize;
        const float xMax = xMin + normalizedTextureSize - pixelSize;
        const float yMax = yMin + normalizedTextureSize - pixelSize;

        return {xMax, yMax, xMin, yMax, xMin, yMin, xMax, yMin};
    }
} // namespace BlockTextureCoordinates

#endif // BLOCKTEXTURECOORDINATES_H_INCLUDED
