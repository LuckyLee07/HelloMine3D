#pragma once

#include "AdventureAudioPresentation.h"
#include "../World/Block/BlockId.h"
#include "../World/Generation/Terrain/TerrainGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

// Converts copied, read-only world observations into the small vocabulary used
// by AdventureAudioPresentation. It never requests chunks, reveals map cells,
// or changes gameplay state.
namespace AdventureAudioWorldAdapter
{
namespace Audio = AdventureAudioPresentation;

inline Audio::SurfaceKind surfaceKind(BlockId block) noexcept
{
    switch (block)
    {
    case BlockId::Grass:
    case BlockId::Dirt:
    case BlockId::ForestFloor:
    case BlockId::Silt:
    case BlockId::Clay:
    case BlockId::Snow:
    case BlockId::OakLeaf:
        return Audio::SurfaceKind::GrassDirt;
    case BlockId::Stone:
    case BlockId::Cobblestone:
    case BlockId::MossStone:
    case BlockId::CoalOre:
    case BlockId::IronOre:
    case BlockId::Glass:
    case BlockId::GlassBorderless:
    case BlockId::Furnace:
    case BlockId::Crusher:
    case BlockId::WaystoneCore:
        return Audio::SurfaceKind::Stone;
    case BlockId::OakBark:
    case BlockId::OakPlank:
    case BlockId::Chest:
    case BlockId::Workbench:
    case BlockId::OakDoorClosed:
    case BlockId::OakDoorOpen:
        return Audio::SurfaceKind::Wood;
    case BlockId::Sand:
    case BlockId::Gravel:
        return Audio::SurfaceKind::Sand;
    default:
        return Audio::SurfaceKind::Unknown;
    }
}

inline Audio::AnimalSpecies animalSpecies(const std::string &type) noexcept
{
    if (type == "hellomine:meadow_sheep")
        return Audio::AnimalSpecies::Sheep;
    if (type == "hellomine:forest_rabbit")
        return Audio::AnimalSpecies::Rabbit;
    if (type == "hellomine:marsh_bird")
        return Audio::AnimalSpecies::WetlandBird;
    return Audio::AnimalSpecies::Unknown;
}

inline Audio::AnimalActivity animalActivity(int value) noexcept
{
    switch (value)
    {
    case 0:
        return Audio::AnimalActivity::Rest;
    case 1:
        return Audio::AnimalActivity::Forage;
    case 2:
        return Audio::AnimalActivity::Wander;
    case 3:
        return Audio::AnimalActivity::Flee;
    default:
        return Audio::AnimalActivity::Rest;
    }
}

struct EnvironmentProbe
{
    TerrainBiome biome = TerrainBiome::Grassland;
    bool surfaceKnown = false;
    int surfaceHeight = 0;
    BlockId surfaceMaterial = BlockId::Air;
    Audio::Vec3 position{};
    float weight = 1.f;
};

class EnvironmentAccumulator
{
  public:
    void add(const EnvironmentProbe &probe) noexcept
    {
        const float weight = Audio::finite(probe.weight)
                                 ? std::max(0.f, probe.weight)
                                 : 0.f;
        if (weight <= 0.f || !Audio::finite(probe.position))
            return;

        m_total += weight;
        const bool ocean = probe.biome == TerrainBiome::Ocean;
        const bool inlandBiome = probe.biome == TerrainBiome::River ||
                                 probe.biome == TerrainBiome::Lake;
        const bool residentWater =
            probe.surfaceKnown && probe.surfaceMaterial == BlockId::Water;
        const bool forest = probe.biome == TerrainBiome::LightForest ||
                            probe.biome == TerrainBiome::TemperateForest;
        const bool waterEvidence = ocean || inlandBiome || residentWater;

        if (!waterEvidence)
        {
            if (forest)
                m_forest += weight;
            else
                m_open += weight;
        }

        if (ocean)
        {
            m_coast += weight;
            addPosition(m_coastPosition, m_coastPositionWeight,
                        probe.position, weight);
        }
        else if (inlandBiome || residentWater)
        {
            m_inland += weight;
            addPosition(m_inlandPosition, m_inlandPositionWeight,
                        probe.position, weight);
        }
    }

    std::array<Audio::EnvironmentTarget, Audio::AmbientKindCount>
    targets() const noexcept
    {
        std::array<Audio::EnvironmentTarget, Audio::AmbientKindCount> result{};
        if (m_total <= 0.f)
            return result;

        const float land = m_open + m_forest;
        if (land > 0.f)
        {
            result[index(Audio::AmbientKind::OpenLand)].weight =
                m_open / land;
            result[index(Audio::AmbientKind::Forest)].weight =
                m_forest / land;
        }

        // Water is a local landmark rather than a full-screen biome tint. The
        // multiplier lets a nearby river or shoreline become audible before
        // it occupies most of the bounded probe grid.
        result[index(Audio::AmbientKind::InlandWater)] = waterTarget(
            m_inland, m_inlandPosition, m_inlandPositionWeight);
        // Ocean alone is open water, not a shoreline. Only expose the coast
        // layer when this bounded probe grid also contains land, so travelling
        // far offshore does not keep playing surf as if the shore were near.
        if (land > 0.f)
        {
            result[index(Audio::AmbientKind::Coast)] = waterTarget(
                m_coast, m_coastPosition, m_coastPositionWeight);
        }
        return Audio::sanitizeEnvironmentTargets(result);
    }

  private:
    static constexpr std::size_t index(Audio::AmbientKind kind) noexcept
    {
        return static_cast<std::size_t>(kind);
    }

    static void addPosition(Audio::Vec3 &sum, float &sumWeight,
                            const Audio::Vec3 &position,
                            float weight) noexcept
    {
        sum.x += position.x * weight;
        sum.y += position.y * weight;
        sum.z += position.z * weight;
        sumWeight += weight;
    }

    Audio::EnvironmentTarget waterTarget(
        float evidence, const Audio::Vec3 &positionSum,
        float positionWeight) const noexcept
    {
        Audio::EnvironmentTarget target;
        target.weight = Audio::clamp01(evidence / m_total * 1.8f);
        if (positionWeight > 0.f)
        {
            target.hasPosition = true;
            target.position = {positionSum.x / positionWeight,
                               positionSum.y / positionWeight,
                               positionSum.z / positionWeight};
        }
        return target;
    }

    float m_total = 0.f;
    float m_open = 0.f;
    float m_forest = 0.f;
    float m_inland = 0.f;
    float m_coast = 0.f;
    Audio::Vec3 m_inlandPosition{};
    Audio::Vec3 m_coastPosition{};
    float m_inlandPositionWeight = 0.f;
    float m_coastPositionWeight = 0.f;
};
} // namespace AdventureAudioWorldAdapter
