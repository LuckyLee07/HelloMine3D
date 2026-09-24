#ifndef WILDLIFEACTOR_H_INCLUDED
#define WILDLIFEACTOR_H_INCLUDED

#include "LivingActor.h"
#include "../World/Generation/Terrain/TerrainGenerator.h"

enum class WildlifeActivity {
    Rest = 0,
    Forage,
    Wander,
    Flee
};

namespace WildlifeSpecies {
    inline constexpr const char *Sheep = "hellomine:meadow_sheep";
    inline constexpr const char *Rabbit = "hellomine:forest_rabbit";
    inline constexpr const char *MarshBird = "hellomine:marsh_bird";

    inline bool isWildlife(const std::string &type) noexcept
    {
        return type == Sheep || type == Rabbit || type == MarshBird;
    }

    inline const char *forBiome(TerrainBiome biome) noexcept
    {
        switch (biome) {
            case TerrainBiome::Grassland: return Sheep;
            case TerrainBiome::LightForest:
            case TerrainBiome::TemperateForest: return Rabbit;
            case TerrainBiome::Wetland:
            case TerrainBiome::River:
            case TerrainBiome::Lake: return MarshBird;
            default: return nullptr;
        }
    }
}

// Natural wildlife is a bounded, transient population. It is regenerated from
// loaded habitat rather than written to world.meta; player edits and rewards
// remain authoritative in their existing storage paths.
class WildlifeActor final : public LivingActor {
  public:
    WildlifeActor(ActorId id, const std::string &type,
                  const glm::vec3 &position);

    void tick(World &world, float dt) override;
    ActorSnapshot getSnapshot() const override;

    WildlifeActivity activity() const noexcept { return m_activity; }

  private:
    glm::vec3 m_home{0.f};
    WildlifeActivity m_activity = WildlifeActivity::Rest;
    float m_ageSeconds = 0.f;
    float m_decisionClock = 0.f;
    float m_motionElapsed = 0.f;
    float m_fallSpeed = 0.f;
    float m_alarmSeconds = 0.f;
    float m_headingRadians = 0.f;
};

#endif // WILDLIFEACTOR_H_INCLUDED
