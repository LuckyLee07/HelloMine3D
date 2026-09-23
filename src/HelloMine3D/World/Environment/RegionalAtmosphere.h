#ifndef HELLOMINE3D_REGIONAL_ATMOSPHERE_H
#define HELLOMINE3D_REGIONAL_ATMOSPHERE_H

#include "WorldEnvironment.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

// Presentation-only geography. Four world-aligned corners are queried once
// per 64 m cell; moving within it only interpolates copied scalar values.
// Callers reset this cache when the world changes. No chunk loads or writes.
class RegionalAtmosphere {
  public:
    static constexpr int Step = 64;
    static constexpr int MaximumQueriesPerCapture = 8;
    struct Profile {
        glm::vec3 tint{1.f};
        float density = 1.f;
    };

    void reset() noexcept { m_valid = false; }

    static Profile profile(TerrainBiome biome, int height) noexcept
    {
        switch (biome) {
            case TerrainBiome::Desert: return {{1.07f, 1.015f, .94f}, .92f};
            case TerrainBiome::RockPlateau: return {{1.04f, .99f, .92f}, .90f};
            case TerrainBiome::LightForest: return {{.975f, 1.008f, .985f}, 1.04f};
            case TerrainBiome::TemperateForest: {
                const float highland = ease(std::clamp((float(height) - 90.f) / 40.f, 0.f, 1.f));
                return {glm::mix(glm::vec3(.94f, 1.015f, .97f),
                                 glm::vec3(.94f, 1.005f, 1.035f), highland),
                        1.10f - .10f * highland};
            }
            case TerrainBiome::Wetland: return {{.90f, 1.035f, 1.00f}, 1.30f};
            case TerrainBiome::Ocean: return {{.96f, 1.025f, 1.055f}, 1.04f};
            case TerrainBiome::River:
            case TerrainBiome::Lake: return {{.95f, 1.02f, 1.025f}, 1.14f};
            case TerrainBiome::Mountain: {
                const float alpine = ease(std::clamp((float(height) - 96.f) / 34.f, 0.f, 1.f));
                return {glm::mix(glm::vec3(.95f, 1.01f, .99f),
                                 glm::vec3(.94f, .99f, 1.06f), alpine),
                        1.02f - .22f * alpine};
            }
            case TerrainBiome::Grassland: {
                const float lowland = 1.f - ease(std::clamp((float(height) - 66.f) / 6.f, 0.f, 1.f));
                return {glm::mix(glm::vec3(1.f), glm::vec3(.98f, 1.008f, 1.025f), lowland),
                        1.f + .04f * lowland};
            }
            default: return {};
        }
    }

    template<class BiomeAt, class HeightAt>
    Profile at(double x, double z, BiomeAt biomeAt, HeightAt heightAt)
    {
        x = bounded(x); z = bounded(z);
        const auto cellX = static_cast<std::int64_t>(std::floor(x / Step));
        const auto cellZ = static_cast<std::int64_t>(std::floor(z / Step));
        if (!m_valid || cellX != m_cellX || cellZ != m_cellZ) {
            for (int dz = 0; dz < 2; ++dz)
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = static_cast<int>((cellX + dx) * Step);
                    const int sz = static_cast<int>((cellZ + dz) * Step);
                    m_corners[dz * 2 + dx] = profile(biomeAt(sx, sz), heightAt(sx, sz));
                }
            m_cellX = cellX; m_cellZ = cellZ; m_valid = true;
        }
        const float u = ease(float((x - cellX * Step) / Step));
        const float v = ease(float((z - cellZ * Step) / Step));
        return blend(blend(m_corners[0], m_corners[1], u),
                     blend(m_corners[2], m_corners[3], u), v);
    }

    static WorldEnvironmentState apply(const WorldEnvironmentState& air,
                                       const Profile& profile) noexcept
    {
        auto state = air;
        state.fogColour *= profile.tint;
        state.skyHorizonColour = state.fogColour;
        state.fogSunwardColour *= profile.tint;
        state.fogDensity *= profile.density;
        state.skyZenithColour *= glm::mix(glm::vec3(1.f), profile.tint, .35f);
        state.cloudLightColour *= glm::mix(glm::vec3(1.f), profile.tint, .20f);
        state.cloudShadowColour *= glm::mix(glm::vec3(1.f), profile.tint, .35f);
        return state;
    }

  private:
    static float ease(float value) noexcept { return value * value * (3.f - 2.f * value); }
    static double bounded(double value) noexcept
    {
        if (!std::isfinite(value)) return 0.;
        // Reserve one complete corner cell before converting to int; cover
        // extreme and negative coordinates without signed overflow.
        return std::clamp(value, double(std::numeric_limits<int>::min()) + Step,
                          double(std::numeric_limits<int>::max()) - 2 * Step);
    }
    static Profile blend(const Profile& a, const Profile& b, float amount) noexcept
    {
        return {glm::mix(a.tint, b.tint, amount), a.density + (b.density - a.density) * amount};
    }
    bool m_valid = false;
    std::int64_t m_cellX = 0, m_cellZ = 0;
    std::array<Profile, 4> m_corners{};
};

static_assert(sizeof(RegionalAtmosphere) <= 128, "Regional atmosphere cache budget");
#endif
