#include "SandboxRuntime.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Diagnostics/RuntimeProfiler.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Command/PlayerBlockInteractionCommand.h"
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
    m_feedbackWorld = m_worldManager.getActiveWorld();
    if (m_feedbackWorld != nullptr) {
        m_actionFeedback.attach(m_feedbackWorld->getEventBus());
    }
    m_actionFeedback.setIntensity(m_config.feedbackIntensity);
    m_player.resetInterpolation();
    m_camera.update();
}

void SandboxRuntime::update(const SandboxInputState &input,
                            float deltaSeconds, bool acceptsPlayerInput)
{
    HELLOMINE3D_PROFILE_SCOPE("SandboxRuntime::update");
    m_foodUseResult.reset();
    m_actionFeedback.update(deltaSeconds);
    const bool breakAttackPressed =
        acceptsPlayerInput && input.breakAttack && !m_breakAttackWasDown;
    m_breakAttackWasDown = input.breakAttack;
    m_player.applyInput(acceptsPlayerInput
                            ? input.player
                            : PlayerInputState());

    World *worldBeforeTick = m_worldManager.getActiveWorld();
    if (worldBeforeTick != m_feedbackWorld) {
        m_actionFeedback.detach();
        m_feedbackWorld = worldBeforeTick;
        if (m_feedbackWorld != nullptr) {
            m_actionFeedback.attach(m_feedbackWorld->getEventBus());
        }
    }
    if (worldBeforeTick != nullptr) {
        const GameplayWorldAction action = acceptsPlayerInput
            ? resolveWorldAction(*worldBeforeTick, input)
            : GameplayWorldAction::None;
        worldBeforeTick->setPlayerGuarding(
            action == GameplayWorldAction::Guard);
    }
    runFixedTicks(deltaSeconds);
    m_camera.update(m_player.getInterpolatedPosition(
                        m_tickScheduler.interpolationAlpha()),
                    m_player.rotation);

    World *world = m_worldManager.getActiveWorld();
    if (world != m_feedbackWorld) {
        m_actionFeedback.detach();
        m_feedbackWorld = world;
        if (m_feedbackWorld != nullptr) {
            m_actionFeedback.attach(m_feedbackWorld->getEventBus());
        }
    }
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
    const GameplayWorldAction worldAction = acceptsPlayerInput
        ? resolveWorldAction(*world, input)
        : GameplayWorldAction::None;
    world->setPlayerGuarding(worldAction == GameplayWorldAction::Guard);
    m_interactionCooldownSeconds = std::max(
        0.0f, m_interactionCooldownSeconds -
                  std::max(0.0f, deltaSeconds));
    if (acceptsPlayerInput) {
        handlePlayerInteraction(*world, input, worldAction, deltaSeconds,
                                breakAttackPressed);
    }
    else {
        m_miningProgress.cancel();
    }
    if (input.resetMeshes) {
        world->resetChunkMeshes();
    }
    world->update(m_camera);
}

void SandboxRuntime::applyUserSettings(
    const UserSettings &settings) noexcept
{
    userSettings(m_config) = settings;
    m_actionFeedback.setIntensity(settings.feedbackIntensity);
    m_camera.setFov(settings.fov);
    World *world = m_worldManager.getActiveWorld();
    if (world != nullptr) {
        world->setRenderDistance(settings.renderDistance);
    }
}

bool SandboxRuntime::closeWorld()
{
    World *previousWorld = m_worldManager.getActiveWorld();
    m_actionFeedback.detach();
    m_feedbackWorld = nullptr;
    if (!m_worldManager.closeAllWorlds()) {
        m_feedbackWorld = previousWorld;
        if (m_feedbackWorld != nullptr) {
            m_actionFeedback.attach(m_feedbackWorld->getEventBus());
        }
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

ActionFeedbackSnapshot SandboxRuntime::getActionFeedback() const
{
    return m_actionFeedback.snapshot();
}

const std::optional<FoodUseResult> &
SandboxRuntime::getFoodUseResult() const noexcept
{
    return m_foodUseResult;
}

void SandboxRuntime::cancelMiningProgress() noexcept
{
    m_miningProgress.cancel();
}

GameplayWorldAction SandboxRuntime::resolveWorldAction(
    World &world, const SandboxInputState &input)
{
    GameplayWorldActionIntent intent;
    intent.breakAttack = input.breakAttack;
    intent.use = input.useBlock;
    intent.place = input.placeBlock;
    intent.guard = input.guardCombat;
    GameplayWorldActionContext context;
    context.actorTarget = m_actorSelection.has_value();
    context.guardAvailable = world.canPlayerGuard();
    if (input.useBlock && m_blockSelection.has_value()) {
        const glm::ivec3 &position = m_blockSelection->blockPosition;
        const BlockId id = static_cast<BlockId>(
            world.getBlock(position.x, position.y, position.z).id);
        if (id != BlockId::Air && id != BlockId::Water &&
            BlockDatabase::get().getDefinition(id).behavior->supportsUse()) {
            context.usableBlockTarget = true;
        }
    }
    if (input.placeBlock && m_blockSelection.has_value()) {
        const ItemStack &held = m_player.getHeldItems();
        if (!held.isEmpty() && held.getMaterial().isBlock) {
            context.placeableHeldItem = true;
        }
    }
    return resolveGameplayWorldAction(intent, context);
}

void SandboxRuntime::handlePlayerInteraction(
    World &world, const SandboxInputState &input,
    GameplayWorldAction action, float deltaSeconds,
    bool breakAttackPressed)
{
    if (input.useHeldFood) {
        m_miningProgress.cancel();
        m_foodUseResult = world.useHeldFood(true);
        return;
    }
    if (m_interactionCooldownSeconds > 0.0f) {
        if (action != GameplayWorldAction::BreakAttack) {
            m_miningProgress.cancel();
        }
        return;
    }

    if (action == GameplayWorldAction::BreakAttack) {
        if (m_actorSelection.has_value()) {
            m_miningProgress.cancel();
            const CombatAttackResult result =
                world.tryAttackActor(m_actorSelection->actorId, true);
            if (breakAttackPressed && result != CombatAttackResult::Hit &&
                result != CombatAttackResult::CoolingDown) {
                m_actionFeedback.submitAttackMiss();
            }
            return;
        }
        if (!m_blockSelection.has_value()) {
            m_miningProgress.cancel();
            if (breakAttackPressed) {
                m_actionFeedback.submitAttackMiss();
            }
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
            world.addCommand<PlayerBlockInteractionCommand>(
                PlayerBlockInteractionAction::Break,
                glm::vec3(selection.blockPosition), m_player);
        }
    }
    else if (action == GameplayWorldAction::Use) {
        m_miningProgress.cancel();
        if (!m_blockSelection.has_value()) {
            return;
        }
        const BlockSelection &selection = *m_blockSelection;
        m_interactionCooldownSeconds = 0.2f;
        world.addCommand<PlayerBlockInteractionCommand>(
            PlayerBlockInteractionAction::Use,
            glm::vec3(selection.blockPosition), m_player);
    }
    else if (action == GameplayWorldAction::Place) {
        m_miningProgress.cancel();
        if (!m_blockSelection.has_value()) {
            return;
        }
        const BlockSelection &selection = *m_blockSelection;
        m_interactionCooldownSeconds = 0.2f;
        world.addCommand<PlayerBlockInteractionCommand>(
            PlayerBlockInteractionAction::Place,
            glm::vec3(selection.placementPosition), m_player);
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
