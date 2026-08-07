#ifndef WORLDSAVE_H_INCLUDED
#define WORLDSAVE_H_INCLUDED

#include "../../Player/Player.h"

#include <glm/glm.hpp>
#include <string>

struct WorldSaveData {
    int version = 1;
    std::string worldId = "default";
    std::string worldName = "DefaultWorld";
    int seed = 0;
    glm::vec3 spawnPoint{0.f};
    float worldTime = 0.f;
    std::string activeGenerator = "ClassicOverWorld";
    bool hasPlayerState = false;
    PlayerSaveState playerState;
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
