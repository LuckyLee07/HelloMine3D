#include "WorldManager.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "../Core/Camera.h"
#include "../Player/Player.h"
#include "Events/PlayerEvents.h"
#include "../Util/ResourcePaths.h"
#include "../World/World.h"

WorldManager::WorldManager(const Config &config, Camera &camera,
                           Player &player, bool startBackgroundLoader,
                           int initialPreloadRadius,
                           std::string mainSaveDirectory)
    : m_config(config)
    , m_camera(&camera)
    , m_player(&player)
    , m_startBackgroundLoader(startBackgroundLoader)
    , m_initialPreloadRadius(initialPreloadRadius)
    , m_mainSaveDirectory(std::move(mainSaveDirectory))
{
}

WorldManager::~WorldManager()
{
    saveAllWorlds();
}

World &WorldManager::createWorld(int worldId)
{
    auto existing = m_worlds.find(worldId);
    if (existing != m_worlds.end()) {
        m_activeWorldId = worldId;
        return *existing->second;
    }

    auto world = std::make_unique<World>(
        *m_camera, m_config, *m_player, saveDirectoryForWorld(worldId),
        m_startBackgroundLoader, m_initialPreloadRadius);
    World &createdWorld = *world;
    m_worlds.emplace(worldId, std::move(world));

    m_activeWorldId = worldId;
    m_worldTime = static_cast<int>(createdWorld.getWorldTime());
    createdWorld.getEventBus().publish(
        PlayerSpawnEvent(DefaultPlayerActorId, worldId, m_player->position));
    return createdWorld;
}

bool WorldManager::loadWorld(int worldId)
{
    getOrCreateWorld(worldId);
    return true;
}

bool WorldManager::saveWorld(int worldId)
{
    World *world = getWorld(worldId);
    if (world == nullptr) {
        return false;
    }

    return world->save();
}

void WorldManager::saveAllWorlds()
{
    for (auto &entry : m_worlds) {
        if (entry.second) {
            entry.second->save();
        }
    }
}

bool WorldManager::closeAllWorlds()
{
    for (auto &entry : m_worlds) {
        if (entry.second && !entry.second->save()) {
            return false;
        }
    }
    m_worlds.clear();
    return true;
}

World *WorldManager::getWorld(int worldId)
{
    auto iter = m_worlds.find(worldId);
    if (iter == m_worlds.end()) {
        return nullptr;
    }

    return iter->second.get();
}

const World *WorldManager::getWorld(int worldId) const
{
    auto iter = m_worlds.find(worldId);
    if (iter == m_worlds.end()) {
        return nullptr;
    }

    return iter->second.get();
}

World *WorldManager::getActiveWorld()
{
    return getWorld(m_activeWorldId);
}

const World *WorldManager::getActiveWorld() const
{
    return getWorld(m_activeWorldId);
}

int WorldManager::getActiveWorldId() const
{
    return m_activeWorldId;
}

int WorldManager::getWorldTime() const
{
    return m_worldTime;
}

void WorldManager::tick()
{
    ++m_worldTime;

    World *world = getActiveWorld();
    if (world != nullptr) {
        world->tick(m_worldTime);
    }
}

bool WorldManager::teleportPlayer(Player &player, const glm::vec3 &position)
{
    return teleportPlayer(player, m_activeWorldId, position);
}

bool WorldManager::teleportPlayer(Player &player, int worldId,
                                  const glm::vec3 &position)
{
    if (worldId != m_activeWorldId) {
        return false;
    }

    World *world = getWorld(worldId);
    if (world == nullptr) {
        return false;
    }

    const int fromWorldId = m_activeWorldId;
    const glm::vec3 fromPosition = player.position;
    player.position = position;
    player.velocity = glm::vec3(0.f);
    player.box.update(player.position);
    player.resetInterpolation();
    world->preloadAround(position);
    world->getEventBus().publish(PlayerTeleportEvent(
        DefaultPlayerActorId, fromWorldId, worldId, fromPosition, position));
    return true;
}

World &WorldManager::getOrCreateWorld(int worldId)
{
    World *world = getWorld(worldId);
    if (world != nullptr) {
        m_activeWorldId = worldId;
        return *world;
    }

    return createWorld(worldId);
}

std::string WorldManager::saveDirectoryForWorld(int worldId) const
{
    if (!m_mainSaveDirectory.empty()) {
        if (worldId == MainWorldId) {
            return m_mainSaveDirectory;
        }
        return ResourcePaths::join(
            m_mainSaveDirectory, "world_" + std::to_string(worldId));
    }

    const char *overrideRoot = std::getenv("HELLOMINE3D_SAVE_DIR");
    if (overrideRoot != nullptr && overrideRoot[0] != '\0') {
        if (worldId == MainWorldId) {
            return overrideRoot;
        }

        return ResourcePaths::join(overrideRoot,
                                   "world_" + std::to_string(worldId));
    }

    if (worldId == MainWorldId) {
        return ResourcePaths::bin("saves/default");
    }

    return ResourcePaths::bin("saves/world_" + std::to_string(worldId));
}
