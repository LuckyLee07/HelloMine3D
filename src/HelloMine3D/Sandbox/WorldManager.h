#ifndef WORLDMANAGER_H_INCLUDED
#define WORLDMANAGER_H_INCLUDED

#include <memory>
#include <string>
#include <unordered_map>

#include "../Config.h"
#include "../Maths/glm.h"
#include "../Util/NonCopyable.h"

class Camera;
class Player;
class World;

class WorldManager : public NonCopyable {
  public:
    static constexpr int MainWorldId = 0;

    WorldManager(const Config &config, Camera &camera, Player &player,
                 bool startBackgroundLoader = true,
                 int initialPreloadRadius = 1,
                 std::string mainSaveDirectory = {});
    ~WorldManager();

    World &createWorld(int worldId = MainWorldId);
    bool loadWorld(int worldId = MainWorldId);
    bool saveWorld(int worldId = MainWorldId);
    void saveAllWorlds();
    bool closeAllWorlds();

    World *getWorld(int worldId);
    const World *getWorld(int worldId) const;
    World *getActiveWorld();
    const World *getActiveWorld() const;

    int getActiveWorldId() const;
    int getWorldTime() const;

    void tick();

    bool teleportPlayer(Player &player, const glm::vec3 &position);
    bool teleportPlayer(Player &player, int worldId,
                        const glm::vec3 &position);

  private:
    World &getOrCreateWorld(int worldId);
    std::string saveDirectoryForWorld(int worldId) const;

    const Config &m_config;
    Camera *m_camera = nullptr;
    Player *m_player = nullptr;
    std::unordered_map<int, std::unique_ptr<World>> m_worlds;
    int m_activeWorldId = MainWorldId;
    int m_worldTime = 0;
    bool m_startBackgroundLoader = true;
    int m_initialPreloadRadius = 1;
    std::string m_mainSaveDirectory;
};

#endif // WORLDMANAGER_H_INCLUDED
