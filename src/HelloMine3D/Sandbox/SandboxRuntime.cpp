#include "SandboxRuntime.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Diagnostics/RuntimeProfiler.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Event/PlayerDigEvent.h"
#include "../World/Interaction/BlockInteractionSystem.h"

SandboxRuntime::SandboxRuntime(const Config &config, Camera &camera,
                               bool startBackgroundLoader,
                               int initialPreloadRadius,
                               std::string mainSaveDirectory)
    : m_config(config)
    , m_camera(camera)
    , m_worldManager(m_config, camera, m_player, startBackgroundLoader,
                     initialPreloadRadius, std::move(mainSaveDirectory))
{
    BlockDatabase::get();
    m_camera.hookEntity(m_player);
    m_worldManager.loadWorld(WorldManager::MainWorldId);
    m_player.resetInterpolation();
    m_camera.update();
}

void SandboxRuntime::update(const SandboxInputState &input,
                            float deltaSeconds, bool acceptsPlayerInput)
{
    HELLOMINE3D_PROFILE_SCOPE("SandboxRuntime::update");
    m_player.applyInput(acceptsPlayerInput
                            ? input.player
                            : PlayerInputState());

    runFixedTicks(deltaSeconds);
    m_camera.update(m_player.getInterpolatedPosition(
                        m_tickScheduler.interpolationAlpha()),
                    m_player.rotation);

    World *world = m_worldManager.getActiveWorld();
    if (world == nullptr) {
        m_blockSelection.reset();
        m_actorSelection.reset();
        m_miningProgress.cancel();
        return;
    }

    PlayerTargetSelection target = PlayerTargetSelectionSystem::pick(
        *world, m_camera.position, m_player.rotation);
    m_blockSelection = std::move(target.block);
    m_actorSelection = std::move(target.actor);
    m_interactionCooldownSeconds = std::max(
        0.0f, m_interactionCooldownSeconds -
                  std::max(0.0f, deltaSeconds));
    if (acceptsPlayerInput) {
        handlePlayerInteraction(*world, input, deltaSeconds);
    }
    else {
        m_miningProgress.cancel();
    }
    if (input.resetMeshes) {
        world->resetChunkMeshes();
    }
    world->update(m_camera);
}

bool SandboxRuntime::closeWorld()
{
    if (!m_worldManager.closeAllWorlds()) {
        return false;
    }
    m_blockSelection.reset();
    m_actorSelection.reset();
    m_miningProgress.cancel();
    return true;
}

WorldDebugStats SandboxRuntime::collectDebugStats()
{
    World *world = m_worldManager.getActiveWorld();
    return world != nullptr ? world->collectDebugStats()
                            : WorldDebugStats();
}

Player &SandboxRuntime::getPlayer()
{
    return m_player;
}

const Player &SandboxRuntime::getPlayer() const
{
    return m_player;
}

WorldManager &SandboxRuntime::getWorldManager()
{
    return m_worldManager;
}

const WorldManager &SandboxRuntime::getWorldManager() const
{
    return m_worldManager;
}

const std::optional<BlockSelection> &SandboxRuntime::getBlockSelection() const
{
    return m_blockSelection;
}

const std::optional<ActorSelection> &SandboxRuntime::getActorSelection() const
{
    return m_actorSelection;
}

const MiningProgressSnapshot &SandboxRuntime::getMiningProgress() const noexcept
{
    return m_miningProgress.snapshot();
}

void SandboxRuntime::cancelMiningProgress() noexcept
{
    m_miningProgress.cancel();
}

void SandboxRuntime::handlePlayerInteraction(
    World &world, const SandboxInputState &input, float deltaSeconds)
{
    if (m_interactionCooldownSeconds > 0.0f) {
        if (!input.breakBlock) {
            m_miningProgress.cancel();
        }
        return;
    }

    if (input.breakBlock) {
        if (m_actorSelection.has_value()) {
            m_miningProgress.cancel();
            m_interactionCooldownSeconds = 0.2f;
            world.attackActor(m_actorSelection->actorId);
            return;
        }
        if (!m_blockSelection.has_value()) {
            m_miningProgress.cancel();
            return;
        }
        const BlockSelection &selection = *m_blockSelection;
        const BlockId blockId = static_cast<BlockId>(
            world.getBlock(selection.blockPosition.x,
                           selection.blockPosition.y,
                           selection.blockPosition.z).id);
        if (blockId == BlockId::Air || blockId == BlockId::Water) {
            m_miningProgress.cancel();
            return;
        }
        const ItemStack &held = m_player.getHeldItems();
        const BlockMiningEvaluation evaluation =
            BlockInteractionSystem::evaluateMining(blockId, held);
        if (m_miningProgress.advance(
                selection.blockPosition, blockId,
                held.getMaterial().id, evaluation.requiredSeconds,
                deltaSeconds)) {
            m_interactionCooldownSeconds = 0.2f;
            world.addEvent<PlayerDigEvent>(
                PlayerDigAction::Break,
                glm::vec3(selection.blockPosition), m_player);
        }
    }
    else if (input.placeBlock) {
        m_miningProgress.cancel();
        if (!m_blockSelection.has_value()) {
            return;
        }
        const BlockSelection &selection = *m_blockSelection;
        m_interactionCooldownSeconds = 0.2f;
        world.addEvent<PlayerDigEvent>(PlayerDigAction::Use,
                                       glm::vec3(selection.blockPosition),
                                       m_player);
        world.addEvent<PlayerDigEvent>(PlayerDigAction::Place,
                                       glm::vec3(selection.placementPosition),
                                       m_player);
    }
    else {
        m_miningProgress.cancel();
    }
}

void SandboxRuntime::runFixedTicks(float deltaSeconds)
{
    HELLOMINE3D_PROFILE_SCOPE("SandboxRuntime::runFixedTicks");
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::duration<double>(std::max(0.0f, deltaSeconds)));
    const std::size_t ticksThisFrame = m_tickScheduler.advance(elapsed);
    HELLOMINE3D_PROFILE_PLOT(
        "Simulation Ticks Per Frame",
        static_cast<double>(ticksThisFrame));
    for (std::size_t tick = 0; tick < ticksThisFrame; ++tick) {
        World *world = m_worldManager.getActiveWorld();
        if (world != nullptr) {
            m_player.update(1.0f / 20.0f, *world);
        }
        m_worldManager.tick();
    }

    RuntimePerformanceCapture::recordSimulationTicks(ticksThisFrame);
}
