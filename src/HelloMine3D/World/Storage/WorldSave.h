#ifndef WORLDSAVE_H_INCLUDED
#define WORLDSAVE_H_INCLUDED

#include "../../Actor/Actor.h"
#include "../../Player/Player.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

inline constexpr int WorldSaveFormatVersion = 2;

struct WorldSaveData {
    int version = WorldSaveFormatVersion;
    std::string worldId = "default";
    std::string worldName = "DefaultWorld";
    int seed = 0;
    glm::vec3 spawnPoint{0.f};
    float worldTime = 0.f;
    std::string activeGenerator = "ClassicOverWorld";
    bool hasPlayerState = false;
    PlayerSaveState playerState;
    std::vector<ActorSaveState> actors;
};

class WorldSave {
  public:
    WorldSave();
    explicit WorldSave(std::string rootDirectory);

    bool load(WorldSaveData &data) const;
    bool save(const WorldSaveData &data) const;

    std::string metadataPath() const;

  private:
    bool ensureRootDirectory() const;

    std::string m_rootDirectory;
};

#endif // WORLDSAVE_H_INCLUDED
