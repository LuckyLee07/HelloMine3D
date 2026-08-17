#ifndef SANDBOXRUNTIME_H_INCLUDED
#define SANDBOXRUNTIME_H_INCLUDED

#include <optional>
#include <string>

#include "../Config.h"
#include "../Core/Camera.h"
#include "../Player/Player.h"
#include "../Util/NonCopyable.h"
#include "../World/World.h"
#include "../World/Interaction/BlockSelection.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "FixedTickScheduler.h"
#include "WorldManager.h"

struct SandboxInputState {
    PlayerInputState player;
    bool breakBlock = false;
    bool placeBlock = false;
    bool resetMeshes = false;
};

class SandboxRuntime : public NonCopyable {
  public:
    SandboxRuntime(const Config &config, Camera &camera,
                   bool startBackgroundLoader = true,
                   int initialPreloadRadius = 1,
                   std::string mainSaveDirectory = {});

    void update(const SandboxInputState &input, float deltaSeconds,
                bool acceptsPlayerInput = true);
    void applyUserSettings(const UserSettings &settings) noexcept;
    bool closeWorld();
    WorldDebugStats collectDebugStats();

    Player &getPlayer();
    const Player &getPlayer() const;
    WorldManager &getWorldManager();
    const WorldManager &getWorldManager() const;
    const std::optional<BlockSelection> &getBlockSelection() const;
    const std::optional<ActorSelection> &getActorSelection() const;
    const MiningProgressSnapshot &getMiningProgress() const noexcept;
    void cancelMiningProgress() noexcept;

  private:
    void handlePlayerInteraction(World &world,
                                 const SandboxInputState &input,
                                 float deltaSeconds);
    void runFixedTicks(float deltaSeconds);

    Config m_config;
    Camera &m_camera;
    Player m_player;
    WorldManager m_worldManager;
    FixedTickScheduler m_tickScheduler;
    std::optional<BlockSelection> m_blockSelection;
    std::optional<ActorSelection> m_actorSelection;
    BlockMiningProgress m_miningProgress;
    float m_interactionCooldownSeconds = 0.0f;
};

#endif // SANDBOXRUNTIME_H_INCLUDED
