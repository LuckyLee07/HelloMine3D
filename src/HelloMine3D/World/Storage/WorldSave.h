#ifndef WORLDSAVE_H_INCLUDED
#define WORLDSAVE_H_INCLUDED

#include "../../Actor/Actor.h"
#include "../../Gameplay/ObjectiveState.h"
#include "../../Gameplay/DifficultyProfile.h"
#include "../../Gameplay/WorldOutcomeState.h"
#include "../../Player/Player.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "StorageTransaction.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr int WorldSaveFormatVersion = 10;

inline glm::vec3 initialWorldSpawnPlaceholder() noexcept
{
    return {0.5f, 96.0f, 0.5f};
}

struct WorldSaveData {
    int version = WorldSaveFormatVersion;
    std::string worldId = "default";
    std::string worldName = "Default World";
    int seed = 0;
    std::int64_t createdUtc = 0;
    std::int64_t lastPlayedUtc = 0;
    std::string lastBuildIdentity = "development";
    glm::vec3 spawnPoint{0.f};
    float worldTime = 0.f;
    std::string activeGenerator = "ClassicOverWorld";
    int terrainGenerationVersion = CurrentTerrainGenerationVersion;
    int difficultyProfileVersion = CurrentDifficultyProfileVersion;
    WorldDifficulty difficulty = WorldDifficulty::Normal;
    std::uint32_t alphaJourneyFlags = 0;
    ObjectiveSaveState objectiveState;
    WorldOutcomeState worldOutcome;
    bool hasPlayerState = false;
    PlayerSaveState playerState;
    std::vector<ActorSaveState> actors;
};

class WorldSave {
  public:
    WorldSave();
    explicit WorldSave(std::string rootDirectory);

    bool load(WorldSaveData &data) const;
    static bool loadFromPath(const std::string &path, WorldSaveData &data,
                             std::string *errorMessage = nullptr);
    bool save(const WorldSaveData &data) const;
    bool save(const WorldSaveData &data,
              const StorageTransactionOptions &options,
              StorageTransactionMetrics *metrics) const;

    std::string metadataPath() const;

  private:
    bool ensureRootDirectory() const;

    std::string m_rootDirectory;
};

#endif // WORLDSAVE_H_INCLUDED
