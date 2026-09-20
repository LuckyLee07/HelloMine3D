#ifndef TERRAIN_ECOLOGY_COLOUR_H_INCLUDED
#define TERRAIN_ECOLOGY_COLOUR_H_INCLUDED

#include "TerrainAppearance.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

// Render-only climate coordinates: dry warmth, and coast (-1) to forest (+1).
// A world-aligned 4 m grid, filtered over an 8 m radius, is independent of
// section residency, height, edits and mesh build order. No World reads here.
class TerrainEcologyColour {
  public:
    static constexpr int Step = 4;
    static constexpr int Edge = 5;
    static glm::vec2 climate(TerrainBiome biome) noexcept
    {
        switch (biome) {
            case TerrainBiome::Desert:
            case TerrainBiome::RockPlateau: return {1.f, 0.f};
            case TerrainBiome::LightForest: return {0.f, .5f};
            case TerrainBiome::TemperateForest:
            case TerrainBiome::Mountain:
            case TerrainBiome::Wetland: return {0.f, 1.f};
            case TerrainBiome::Ocean:
            case TerrainBiome::River:
            case TerrainBiome::Lake: return {0.f, -1.f};
            default: return {0.f, 0.f};
        }
    }

    template<class BiomeAt> void capture(int originX, int originZ, BiomeAt biomeAt)
    {
        std::array<glm::vec2, 81> samples{};
        const auto bounded = [](std::int64_t value) {
            return static_cast<int>(std::max<std::int64_t>(
                std::numeric_limits<int>::min(), std::min<std::int64_t>(
                    std::numeric_limits<int>::max(), value)));
        };
        for (int z = 0; z < 9; ++z)
            for (int x = 0; x < 9; ++x)
                samples[z * 9 + x] = climate(biomeAt(
                    bounded(std::int64_t(originX) + (x - 2) * Step),
                    bounded(std::int64_t(originZ) + (z - 2) * Step)));
        constexpr int weight[5] = {1, 2, 3, 2, 1};
        for (int z = 0; z < Edge; ++z)
            for (int x = 0; x < Edge; ++x) {
                glm::vec2 sum(0.f);
                for (int dz = 0; dz < 5; ++dz)
                    for (int dx = 0; dx < 5; ++dx)
                        sum += samples[(z + dz) * 9 + x + dx] *
                               static_cast<float>(weight[dx] * weight[dz]);
                m_values[z * Edge + x] = sum / 81.f;
            }
    }

    glm::vec2 at(float x, float z) const noexcept
    {
        x = std::max(0.f, std::min(16.f, x)) / Step;
        z = std::max(0.f, std::min(16.f, z)) / Step;
        const int ix = std::min(Edge - 2, static_cast<int>(x));
        const int iz = std::min(Edge - 2, static_cast<int>(z));
        return glm::mix(glm::mix(m_values[iz * Edge + ix],
                                m_values[iz * Edge + ix + 1], x - ix),
                        glm::mix(m_values[(iz + 1) * Edge + ix],
                                 m_values[(iz + 1) * Edge + ix + 1], x - ix), z - iz);
    }

    static bool plantTile(int x, int y) noexcept
    {
        return y >= 3 && y <= 7 &&
               ((x >= 0 && x <= 8) || (x >= 12 && x <= 14));
    }

    // uv0's integer tile identity is unchanged. Only its previously unused
    // fractional part carries climate; uv1 still owns all texture repetition.
    // The .25..75 guard band keeps interpolation safely inside the same tile.
    static glm::vec2 encode(int x, int y, glm::vec2 value, float tiles) noexcept
    {
        return (glm::vec2(x, y) + glm::vec2(.25f + .5f * value.x,
                                          .5f + .25f * value.y)) / tiles;
    }

  private:
    std::array<glm::vec2, Edge * Edge> m_values{};
};

#endif
