// Headless runtime validation for the sandbox foundation milestones.
//
// This runner drives the real World / ChunkManager / WorldManager / actor code
// paths without a graphics context, so the S0-S7 milestones can be validated
// without a human driving the client window.
//
// Each check prints one line:
//   [VALIDATION] PASS <id> :: <detail>
//   [VALIDATION] FAIL <id> :: <detail>
// and the process exits non-zero when any check fails.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <FreeImage.h>

#include "../Actor/EnemyRegistry.h"
#include "../Actor/EnemyPresentation.h"
#include "../Actor/ItemEntity.h"
#include "../Actor/LivingActor.h"
#include "../Actor/MobActor.h"
#include "../Audio/AudioDefinitionRegistry.h"
#include "../Audio/AudioRuntime.h"
#include "../Audio/MusicDefinitionRegistry.h"
#include "../Audio/MusicRuntime.h"
#include "../Config.h"
#include "../Core/Camera.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Diagnostics/RuntimeDebugOptions.h"
#include "../Diagnostics/TerrainBufferMetrics.h"
#include "../Gameplay/AlphaJourney.h"
#include "../Gameplay/ObjectiveRegistry.h"
#include "../Gameplay/VictoryFlow.h"
#include "../Gameplay/WaystoneEncounter.h"
#include "../Feedback/ActionFeedback.h"
#include "../Item/Material.h"
#include "../Item/CraftingSession.h"
#include "../Item/ContainerInventory.h"
#include "../Item/FoodRegistry.h"
#include "../Item/RecipeRegistry.h"
#include "../Item/SmeltingRegistry.h"
#include "../Item/ToolRegistry.h"
#include "../Player/Player.h"
#include "../Presentation/LocalizedTextRegistry.h"
#include "../Presentation/LocalizedPresentation.h"
#include "../Presentation/PresentationCaption.h"
#include "../Presentation/PresentationLayout.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/ChunkEvents.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/FoodEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SmeltingEvents.h"
#include "../Sandbox/Events/WaystoneEvents.h"
#include "../Sandbox/FixedTickScheduler.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Sandbox/WorldManager.h"
#include "../Util/ResourcePaths.h"
#include "../World/Block/BlockBehavior.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockTextureCoordinates.h"
#include "../World/Block/TerrainAppearance.h"
#include "../World/Interaction/BlockSelection.h"
#include "../World/Interaction/BlockInteractionSystem.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "../World/Chunk/ChunkMeshBuilder.h"
#include "../World/Chunk/SectionMeshInput.h"
#include "../World/Environment/WorldEnvironment.h"
#include "../World/Light/VertexLighting.h"
#include "../World/Generation/Biome/TemperateForestBiome.h"
#include "../World/Generation/Structures/StructurePlanning.h"
#include "../World/Generation/Terrain/ClassicOverWorldGenerator.h"
#include "../World/Storage/ChunkStorage.h"
#include "../World/Storage/WorldCatalogue.h"
#include "../World/Storage/WorldManagementService.h"
#include "../World/Storage/WorldSave.h"
#include "../World/World.h"

namespace {

constexpr int kValidationSeed = 20260807;

int g_checkCount = 0;
int g_failureCount = 0;

void check(const std::string &id, bool passed, const std::string &detail = "")
{
    ++g_checkCount;
    if (!passed) {
        ++g_failureCount;
    }

    std::cout << "[VALIDATION] " << (passed ? "PASS " : "FAIL ") << id;
    if (!detail.empty()) {
        std::cout << " :: " << detail;
    }
    std::cout << '\n';
    std::cout.flush();
}

struct IndexedQuad
{
    std::array<std::uint32_t, 4> vertices{};
    std::size_t count = 0;
};

IndexedQuad indexedQuad(const Mesh &mesh, std::size_t face)
{
    IndexedQuad result;
    const std::size_t firstIndex = face * 6;
    if (firstIndex + 6 > mesh.indices.size()) {
        return result;
    }
    for (std::size_t offset = 0; offset < 6; ++offset) {
        const std::uint32_t candidate = mesh.indices[firstIndex + offset];
        if (std::find(result.vertices.begin(),
                      result.vertices.begin() + result.count,
                      candidate) != result.vertices.begin() + result.count) {
            continue;
        }
        if (result.count == result.vertices.size()) {
            result.count = 0;
            return result;
        }
        result.vertices[result.count++] = candidate;
    }
    if (result.count != result.vertices.size()) {
        result.count = 0;
    }
    return result;
}

void setEnv(const char *name, const std::string &value)
{
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) {
        unsetenv(name);
    }
    else {
        setenv(name, value.c_str(), 1);
    }
#endif
}

void clearDeterministicEnv()
{
    setEnv("HELLOMINE3D_PLAYER_POSITION", "");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "");
    setEnv("HELLOMINE3D_WORLD_TIME", "");
}

/// Fresh, isolated save directory so a validation run never touches bin/saves.
std::string freshSaveDirectory(const std::string &name)
{
    std::filesystem::path root =
        std::filesystem::path(ResourcePaths::bin("validation_runs")) / name;

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    return root.string();
}

bool initializeTerrainIdentity(const std::string &directory,
                               const std::string &worldId,
                               int terrainGenerationVersion)
{
    WorldSaveData data;
    data.worldId = worldId;
    data.worldName = worldId;
    data.seed = kValidationSeed;
    data.createdUtc = LegacyWorldTimestampUtc;
    data.lastPlayedUtc = LegacyWorldTimestampUtc;
    data.lastBuildIdentity = "terrain-test";
    data.terrainGenerationVersion = terrainGenerationVersion;
    data.hasPlayerState = true;
    return WorldSave(directory).save(data);
}

std::string vecToString(const glm::vec3 &value)
{
    std::ostringstream out;
    out << value.x << ' ' << value.y << ' ' << value.z;
    return out.str();
}

Config makeConfig()
{
    Config config;
    config.renderDistance = 2;
    return config;
}

/// Counts every sandbox event type published during a scenario.
class EventRecorder {
  public:
    explicit EventRecorder(SandboxEventBus &bus)
        : m_bus(&bus)
    {
        for (auto type : allTypes()) {
            m_ids.push_back(m_bus->subscribe(
                type, [this, type](const SandboxEvent &) { ++m_counts[static_cast<int>(type)]; }));
        }
    }

    ~EventRecorder()
    {
        for (auto id : m_ids) {
            m_bus->unsubscribe(id);
        }
    }

    EventRecorder(const EventRecorder &) = delete;
    EventRecorder &operator=(const EventRecorder &) = delete;

    int count(SandboxEventType type) const
    {
        return m_counts[static_cast<int>(type)];
    }

    void reset()
    {
        for (auto &value : m_counts) {
            value = 0;
        }
    }

  private:
    static const std::vector<SandboxEventType> &allTypes()
    {
        static const std::vector<SandboxEventType> types = {
            SandboxEventType::BlockBreak,
            SandboxEventType::BlockPlace,
            SandboxEventType::BlockUse,
            SandboxEventType::BlockChanged,
            SandboxEventType::ChunkGenerated,
            SandboxEventType::ChunkLoaded,
            SandboxEventType::ChunkUnloaded,
            SandboxEventType::ChunkSaved,
            SandboxEventType::EntitySpawn,
            SandboxEventType::EntityDamage,
            SandboxEventType::EntityDeath,
            SandboxEventType::ItemPickup,
            SandboxEventType::PlayerSpawn,
            SandboxEventType::PlayerTeleport,
            SandboxEventType::PlayerInventoryChanged,
            SandboxEventType::CraftCompleted,
            SandboxEventType::SmeltCompleted,
            SandboxEventType::FoodConsumed,
            SandboxEventType::CombatWindup,
            SandboxEventType::CombatGuard,
            SandboxEventType::WaystoneActivated,
            SandboxEventType::VictoryRewardClaimed,
        };
        return types;
    }

    SandboxEventBus *m_bus;
    std::vector<SandboxEventBus::SubscriptionId> m_ids;
    std::array<int, 32> m_counts{};
};

// ---------------------------------------------------------------------------
// V3 - the debug panel can be enabled before the first captured frame
// ---------------------------------------------------------------------------
void caseDebugPanelStartupOption()
{
    check("V3/debug-option-false-values",
          !RuntimeDebugOptions::isEnabledValue(nullptr) &&
              !RuntimeDebugOptions::isEnabledValue("") &&
              !RuntimeDebugOptions::isEnabledValue("0") &&
              !RuntimeDebugOptions::isEnabledValue("false") &&
              !RuntimeDebugOptions::isEnabledValue("OFF") &&
              !RuntimeDebugOptions::isEnabledValue("No"));
    check("V3/debug-option-true-values",
          RuntimeDebugOptions::isEnabledValue("1") &&
              RuntimeDebugOptions::isEnabledValue("true") &&
              RuntimeDebugOptions::isEnabledValue("on"));

    setEnv("HELLOMINE3D_SHOW_DEBUG_INFO", "1");
    check("V3/debug-option-env-on",
          RuntimeDebugOptions::showDebugInfoAtStartup());
    setEnv("HELLOMINE3D_SHOW_DEBUG_INFO", "0");
    check("V3/debug-option-env-off",
          !RuntimeDebugOptions::showDebugInfoAtStartup());
    setEnv("HELLOMINE3D_SHOW_DEBUG_INFO", "");
}

// ---------------------------------------------------------------------------
// V1 - the runtime fixed-step scheduler produces 20 ticks per second
// ---------------------------------------------------------------------------
void caseFixedTickScheduler()
{
    FixedTickScheduler scheduler;
    std::size_t ticks = 0;
    for (int millisecond = 0; millisecond < 10000; ++millisecond) {
        ticks += scheduler.advance(std::chrono::milliseconds(1));
    }
    check("V1/fixed-tick-scheduler-20hz", ticks == 200,
          "ticks=" + std::to_string(ticks) + " over 10 seconds");

    FixedTickScheduler cappedScheduler;
    const auto cappedTicks = cappedScheduler.advance(std::chrono::seconds(1));
    const auto nextTicks =
        cappedScheduler.advance(std::chrono::milliseconds(50));
    check("V1/fixed-tick-catchup-bounded",
          cappedTicks == 5 && nextTicks == 1,
          "capped=" + std::to_string(cappedTicks) +
              " next=" + std::to_string(nextTicks));

    FixedTickScheduler interpolationScheduler;
    const std::size_t halfStep = interpolationScheduler.advance(
        std::chrono::milliseconds(25));
    const float halfAlpha = interpolationScheduler.interpolationAlpha();
    const std::size_t fullStep = interpolationScheduler.advance(
        std::chrono::milliseconds(25));
    check("V1/fixed-tick-interpolation-alpha",
          halfStep == 0 && std::abs(halfAlpha - 0.5f) < 0.001f &&
              fullStep == 1 &&
              interpolationScheduler.interpolationAlpha() < 0.001f);
}

// ---------------------------------------------------------------------------
// AL-A3 - World fixed-tick orchestration has named, observable raw phases
// ---------------------------------------------------------------------------
void caseWorldSimulationRuntime()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player,
                freshSaveDirectory("al_a3_world_simulation"), false, 1);

    const WorldSimulationSnapshot initial =
        world.collectDebugStats().simulation;
    check("AL-A3/initial-snapshot-has-no-completed-tick",
          initial.completedTicks == 0);

    const std::array<const char *, WorldSimulationPhaseCount> expectedNames = {
        "Tick Preparation", "Actor Simulation", "Combat", "Encounter",
        "Block Random Tick", "Population", "Block Entity Simulation",
        "Gameplay Runtime"};
    bool identityOrdered = true;
    for (std::size_t index = 0; index < expectedNames.size(); ++index) {
        identityOrdered = identityOrdered &&
            initial.phases[index].phase ==
                static_cast<WorldSimulationPhase>(index) &&
            std::string(worldSimulationPhaseName(
                initial.phases[index].phase)) == expectedNames[index];
    }
    check("AL-A3/phase-identity-and-order-are-frozen", identityOrdered,
          "phases=" + std::to_string(expectedNames.size()));

    world.tick(4242);
    const WorldSimulationSnapshot first =
        world.collectDebugStats().simulation;
    check("AL-A3/tick-context-is-propagated",
          first.completedTicks == 1 && first.lastTick == 4242 &&
              std::abs(first.deltaSeconds -
                       WorldSimulation::FixedDeltaSeconds) < 0.000001f &&
              world.getWorldTime() == 4242.f);

    bool timingsValid = std::isfinite(first.tickElapsedMilliseconds) &&
                        first.tickElapsedMilliseconds >= 0.0;
    double phaseElapsed = 0.0;
    for (std::size_t index = 0; index < first.phases.size(); ++index) {
        const WorldSimulationPhaseTiming &phase = first.phases[index];
        timingsValid = timingsValid &&
            phase.phase == static_cast<WorldSimulationPhase>(index) &&
            std::isfinite(phase.elapsedMilliseconds) &&
            phase.elapsedMilliseconds >= 0.0;
        phaseElapsed += phase.elapsedMilliseconds;
    }
    check("AL-A3/raw-last-tick-timings-are-observable",
          timingsValid &&
              first.tickElapsedMilliseconds + 0.001 >= phaseElapsed,
          "total_ms=" + std::to_string(first.tickElapsedMilliseconds) +
              " phase_ms=" + std::to_string(phaseElapsed));

    GameApplicationFlow flow;
    const bool playing = flow.showWorldList() &&
                         flow.beginLoading("al-a3-pause") &&
                         flow.completeLoading(true);
    if (playing && flow.acceptsWorldSimulation()) {
        world.tick(4243);
    }
    const std::uint64_t beforePause =
        world.collectDebugStats().simulation.completedTicks;
    const bool paused = flow.pause();
    for (int frame = 0; frame < 10; ++frame) {
        if (flow.acceptsWorldSimulation()) {
            world.tick(5000 + frame);
        }
    }
    const std::uint64_t afterPause =
        world.collectDebugStats().simulation.completedTicks;
    check("AL-A3/caller-owned-pause-gate-freezes-simulation",
          playing && paused && beforePause == 2 &&
              afterPause == beforePause);

    const bool resumed = flow.resume();
    if (resumed && flow.acceptsWorldSimulation()) {
        world.tick(4244);
    }
    const WorldSimulationSnapshot resumedSnapshot =
        world.collectDebugStats().simulation;
    check("AL-A3/resume-advances-exactly-one-tick",
          resumed && resumedSnapshot.completedTicks == 3 &&
              resumedSnapshot.lastTick == 4244);
}

// ---------------------------------------------------------------------------
// AL-A5 - selected real phases expose last-tick work and budget vocabulary
// ---------------------------------------------------------------------------
void caseSimulationPhaseMetrics()
{
    const std::array<WorldSimulationPhase, SimulationMetricPhaseCount>
        expectedPhases = {
            WorldSimulationPhase::ActorSimulation,
            WorldSimulationPhase::Combat,
            WorldSimulationPhase::BlockRandomTick,
            WorldSimulationPhase::Population};

    SimulationPhaseMetrics vocabulary;
    const bool unbudgeted =
        vocabulary.budgetStatus() ==
            SimulationPhaseBudgetStatus::Unbudgeted &&
        std::string(simulationPhaseBudgetStatusName(
            vocabulary.budgetStatus())) == "unbudgeted";
    vocabulary.budgetScope = SimulationPhaseBudgetScope::PerTick;
    vocabulary.budget = 4;
    vocabulary.processed = 2;
    const bool within = vocabulary.budgetStatus() ==
                        SimulationPhaseBudgetStatus::WithinBudget;
    vocabulary.processed = 4;
    const bool atBudget = vocabulary.budgetStatus() ==
                          SimulationPhaseBudgetStatus::AtBudget;
    vocabulary.deferred = 1;
    const bool deferred = vocabulary.budgetStatus() ==
                          SimulationPhaseBudgetStatus::WorkDeferred;
    check("AL-A5/budget-status-vocabulary-is-frozen",
          unbudgeted && within && atBudget && deferred &&
              std::string(simulationPhaseBudgetScopeName(
                  SimulationPhaseBudgetScope::PerPopulationCycle)) ==
                  "per-cycle");

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player,
                freshSaveDirectory("al_a5_simulation_metrics"), false, 1);

    const WorldSimulationSnapshot initial =
        world.collectDebugStats().simulation;
    bool identityOrdered = initial.metrics.size() == expectedPhases.size();
    for (std::size_t index = 0; index < expectedPhases.size(); ++index) {
        identityOrdered = identityOrdered &&
            initial.metrics[index].phase == expectedPhases[index];
    }
    check("AL-A5/metric-identities-exclude-empty-system-slots",
          identityOrdered && initial.metrics.size() == 4);

    world.tick(4240);
    const WorldSimulationSnapshot cycle =
        world.collectDebugStats().simulation;
    bool elapsedMatches = true;
    for (const SimulationPhaseMetrics &metrics : cycle.metrics) {
        const std::size_t phase = static_cast<std::size_t>(metrics.phase);
        elapsedMatches = elapsedMatches &&
            phase < cycle.phases.size() &&
            metrics.elapsedMilliseconds ==
                cycle.phases[phase].elapsedMilliseconds &&
            std::isfinite(metrics.elapsedMilliseconds) &&
            metrics.elapsedMilliseconds >= 0.0;
    }
    check("AL-A5/metric-elapsed-matches-a3-phase-timing",
          elapsedMatches);

    const SimulationPhaseMetrics *actor = findSimulationPhaseMetrics(
        cycle, WorldSimulationPhase::ActorSimulation);
    check("AL-A5/actor-work-is-unbudgeted-and-counted",
          actor != nullptr && actor->processed >= 1 &&
              actor->deferred == 0 && actor->budget == 0 &&
              actor->budgetStatus() ==
                  SimulationPhaseBudgetStatus::Unbudgeted);

    const SimulationPhaseMetrics *combat = findSimulationPhaseMetrics(
        cycle, WorldSimulationPhase::Combat);
    check("AL-A5/combat-uses-existing-per-tick-budget",
          combat != nullptr &&
              combat->budgetScope ==
                  SimulationPhaseBudgetScope::PerTick &&
              combat->budget ==
                  World::CombatProjectileStepBudgetPerTick &&
              combat->processed <= combat->budget &&
              (combat->deferred == 0 ||
               combat->budgetStatus() ==
                   SimulationPhaseBudgetStatus::WorkDeferred));

    const SimulationPhaseMetrics *randomTick = findSimulationPhaseMetrics(
        cycle, WorldSimulationPhase::BlockRandomTick);
    check("AL-A5/random-tick-uses-existing-per-tick-budget",
          randomTick != nullptr &&
              randomTick->budgetScope ==
                  SimulationPhaseBudgetScope::PerTick &&
              randomTick->budget ==
                  World::RandomTickSectionBudgetPerTick &&
              randomTick->processed <= randomTick->budget &&
              (randomTick->deferred == 0 ||
               randomTick->budgetStatus() ==
                   SimulationPhaseBudgetStatus::WorkDeferred));

    const SimulationPhaseMetrics *population = findSimulationPhaseMetrics(
        cycle, WorldSimulationPhase::Population);
    const std::size_t cycleAttempts =
        population != nullptr ? population->processed : 0;
    const std::size_t cycleBudget =
        population != nullptr ? population->budget : 0;
    world.tick(4241);
    const WorldSimulationSnapshot nextCycle =
        world.collectDebugStats().simulation;
    const SimulationPhaseMetrics *nextPopulation =
        findSimulationPhaseMetrics(
            nextCycle,
            WorldSimulationPhase::Population);
    check("AL-A5/population-is-per-cycle-and-resets-next-tick",
          population != nullptr && cycleAttempts > 0 &&
              cycleAttempts <= cycleBudget &&
              population->deferred == 0 &&
              population->budgetScope ==
                  SimulationPhaseBudgetScope::PerPopulationCycle &&
              nextPopulation != nullptr &&
              nextPopulation->processed == 0 &&
              nextPopulation->deferred == 0 &&
              nextPopulation->budget == cycleBudget,
          "cycle_attempts=" + std::to_string(cycleAttempts) +
              " budget=" + std::to_string(cycleBudget));
}

// ---------------------------------------------------------------------------
// E0 - block data and mesh UV generation do not require a graphics context
// ---------------------------------------------------------------------------
void caseBlockTextureCoordinates()
{
    const TerrainMaterialParameters &defaults =
        runtimeTerrainMaterialProfile().parameters();
    const auto first = BlockTextureCoordinates::get(0, 0);
    const auto last = BlockTextureCoordinates::get(15, 15);
    constexpr float epsilon = 0.000001f;

    check("E0/texture-coordinates-first-tile",
          std::abs(first[0] - 0.060546875f) < epsilon &&
              std::abs(first[2] - 0.001953125f) < epsilon &&
              std::abs(first[5] - 0.001953125f) < epsilon);
    check("E0/texture-coordinates-last-tile",
          std::abs(last[0] - 0.998046875f) < epsilon &&
              std::abs(last[2] - 0.939453125f) < epsilon &&
              std::abs(last[5] - 0.939453125f) < epsilon);
    check("V10B1/default-terrain-material-profile-is-compatible",
          TerrainMaterialParameters::ContractVersion == 1 &&
              defaults.atlasPixels == 256 &&
              defaults.tilePixels == 16 &&
              defaults.tilesPerRow == 16 &&
              defaults.containsTile(15, 15) &&
              !defaults.containsTile(16, 0));

    TerrainMaterialParameters scaled = defaults;
    scaled.atlasPixels = 512;
    scaled.tilePixels = 32;
    const auto scaledFirst = BlockTextureCoordinates::get(0, 0, scaled);
    check("V10B1/scaled-atlas-pixel-centres-are-parameterized",
          std::abs(scaledFirst[0] - 0.0615234375f) < epsilon &&
              std::abs(scaledFirst[2] - 0.0009765625f) < epsilon);
}

// ---------------------------------------------------------------------------
// A3 - runtime config is generated locally and remains user-owned
// ---------------------------------------------------------------------------
void caseRuntimeConfigOwnership()
{
    const std::filesystem::path directory =
        freshSaveDirectory("runtime_config");
    const std::filesystem::path configPath = directory / "config.txt";

    const Config generated = loadRuntimeConfig(configPath.string());
    check("A3/missing-config-regenerated",
          std::filesystem::is_regular_file(configPath),
          configPath.string());
    check("A3/generated-config-uses-documented-defaults",
          generated.renderDistance == 8 && !generated.isFullscreen &&
              generated.windowX == 1280 && generated.windowY == 720 &&
              generated.fov == 90 &&
              generated.directionalShadowQuality ==
                  DirectionalShadowQuality::Off &&
              generated.postProcessingQuality ==
                  PostProcessingQuality::Off &&
              std::abs(generated.mouseSensitivity - 0.05f) < 0.0001f &&
              !generated.invertMouseY &&
              std::abs(generated.musicVolume - 0.65f) < 0.0001f &&
              std::abs(generated.uiScale - 1.f) < 0.0001f &&
              generated.locale == "en-US" &&
              generated.audioCaptions && generated.showActionHints &&
              generated.inputBindings.get(GameplayAction::MoveForward) ==
                  GameplayKey::W &&
              generated.inputBindings.get(GameplayAction::ConsumeFood) ==
                  GameplayKey::R &&
              generated.mouseBindings.get(
                  GameplayWorldAction::BreakAttack) ==
                  GameplayMouseButton::Primary &&
              generated.mouseBindings.get(GameplayWorldAction::Use) ==
                  GameplayMouseButton::Secondary &&
              generated.sprintMode == GameplayHoldMode::Hold &&
              generated.sneakMode == GameplayHoldMode::Hold &&
              generated.feedbackIntensity ==
                  GameplayFeedbackIntensity::Full &&
              !generated.worldSeed.has_value(),
          std::to_string(generated.renderDistance) + " " +
              std::to_string(generated.isFullscreen) + " " +
              std::to_string(generated.windowX) + "x" +
              std::to_string(generated.windowY) + " " +
              std::to_string(generated.fov));
    {
        std::ifstream input(configPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("V10E/settings-file-is-versioned-with-post-processing",
               text.find("settings_version 8\n") != std::string::npos &&
                   text.find("directionalshadowquality off\n") !=
                       std::string::npos &&
                   text.find("postprocessingquality off\n") !=
                       std::string::npos &&
                   text.find("mastervolume 1") != std::string::npos &&
                   text.find("ambientvolume 1") != std::string::npos &&
                   text.find("musicvolume 0.649") != std::string::npos &&
                   text.find("uiscale 1") != std::string::npos &&
                   text.find("locale en-US") != std::string::npos &&
                   text.find("key_consume_food r") != std::string::npos &&
                   text.find("sprintmode hold\n") != std::string::npos &&
                   text.find("mouse_break_attack primary\n") !=
                       std::string::npos &&
                   text.find("mouse_guard secondary\n") !=
                       std::string::npos &&
                   text.find("feedbackintensity full\n") !=
                       std::string::npos,
              text);
    }

    {
        std::ofstream output(configPath,
                             std::ios::binary | std::ios::trunc);
        output << "renderdistance 3\n"
               << "fullscreen 1\n"
               << "windowsize 1024 768\n"
               << "fov 100\n"
               << "mousesensitivity 0.12\n"
               << "invertmousey 1\n";
    }
    const Config customised = loadRuntimeConfig(configPath.string());
    check("A3/user-config-overrides-defaults",
          customised.renderDistance == 3 && customised.isFullscreen &&
              customised.windowX == 1024 && customised.windowY == 768 &&
              customised.fov == 100 &&
              std::abs(customised.mouseSensitivity - 0.12f) < 0.0001f &&
              customised.invertMouseY &&
              std::abs(customised.masterVolume - 1.0f) < 0.0001f);
    {
        std::ifstream input(configPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("V10E/legacy-settings-migrated-atomically",
               customised.directionalShadowQuality ==
                       DirectionalShadowQuality::Off &&
                   customised.postProcessingQuality ==
                       PostProcessingQuality::Off &&
                   text.find("settings_version 8\n") == 0 &&
                   text.find("directionalshadowquality off\n") !=
                       std::string::npos &&
                   text.find("postprocessingquality off\n") !=
                       std::string::npos &&
                   text.find("uivolume 1") != std::string::npos &&
                   text.find("musicvolume 0.649") != std::string::npos &&
                   text.find("locale en-US") != std::string::npos &&
                   text.find("audiocaptions 1") != std::string::npos,
               text);
    }

    const std::filesystem::path versionOnePath =
        directory / "version-one-config.txt";
    {
        std::ofstream output(versionOnePath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 1\n"
               << "renderdistance 6\n"
               << "fov 95\n";
    }
    const Config versionOne = loadRuntimeConfig(versionOnePath.string());
    {
        std::ifstream input(versionOnePath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("N12A/version-one-settings-migrate-with-presentation-defaults",
              versionOne.renderDistance == 6 && versionOne.fov == 95 &&
                  versionOne.locale == "en-US" &&
                  versionOne.audioCaptions && versionOne.showActionHints &&
                  versionOne.inputBindings.get(
                      GameplayAction::OpenCrafting) == GameplayKey::E &&
                  std::abs(versionOne.musicVolume - 0.65f) < 0.0001f &&
                  versionOne.directionalShadowQuality ==
                      DirectionalShadowQuality::Off &&
                  versionOne.postProcessingQuality ==
                      PostProcessingQuality::Off &&
                  text.find("settings_version 8\n") == 0,
              text);
    }

    const std::filesystem::path versionTwoPath =
        directory / "version-two-config.txt";
    {
        std::ofstream output(versionTwoPath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 2\n"
               << "uiscale 1.25\n"
               << "audiocaptions 0\n";
    }
    const Config versionTwo = loadRuntimeConfig(versionTwoPath.string());
    {
        std::ifstream input(versionTwoPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("N12A/version-two-settings-migrate-with-locale-default",
              std::abs(versionTwo.uiScale - 1.25f) < 0.0001f &&
                  !versionTwo.audioCaptions &&
                  versionTwo.locale == "en-US" &&
                  std::abs(versionTwo.musicVolume - 0.65f) < 0.0001f &&
                  versionTwo.directionalShadowQuality ==
                      DirectionalShadowQuality::Off &&
                  versionTwo.postProcessingQuality ==
                      PostProcessingQuality::Off &&
                  text.find("settings_version 8\n") == 0 &&
                  text.find("locale en-US\n") != std::string::npos,
              text);
    }

    const std::filesystem::path versionThreePath =
        directory / "version-three-config.txt";
    {
        std::ofstream output(versionThreePath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 3\n"
               << "locale zh-CN\n"
               << "ambientvolume 0.4\n";
    }
    const Config versionThree = loadRuntimeConfig(versionThreePath.string());
    {
        std::ifstream input(versionThreePath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("N12C/version-three-settings-migrate-with-music-default",
              versionThree.locale == "zh-CN" &&
                  std::abs(versionThree.ambientVolume - 0.4f) < 0.0001f &&
                  std::abs(versionThree.musicVolume - 0.65f) < 0.0001f &&
                  versionThree.directionalShadowQuality ==
                      DirectionalShadowQuality::Off &&
                  versionThree.postProcessingQuality ==
                      PostProcessingQuality::Off &&
                  text.find("settings_version 8\n") == 0 &&
                  text.find("musicvolume 0.649") != std::string::npos,
              text);
    }

    const std::filesystem::path versionFourPath =
        directory / "version-four-config.txt";
    {
        std::ofstream output(versionFourPath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 4\n"
               << "musicvolume 0.4\n";
    }
    const Config versionFour = loadRuntimeConfig(versionFourPath.string());
    {
        std::ifstream input(versionFourPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("V10D/version-four-settings-migrate-shadows-off",
              std::abs(versionFour.musicVolume - 0.4f) < 0.0001f &&
                  versionFour.directionalShadowQuality ==
                      DirectionalShadowQuality::Off &&
                  versionFour.postProcessingQuality ==
                      PostProcessingQuality::Off &&
                  text.find("settings_version 8\n") == 0 &&
                   text.find("directionalshadowquality off\n") !=
                       std::string::npos,
              text);
    }

    const std::filesystem::path versionFivePath =
        directory / "version-five-config.txt";
    {
        std::ofstream output(versionFivePath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 5\n"
               << "directionalshadowquality high\n";
    }
    const Config versionFive = loadRuntimeConfig(versionFivePath.string());
    {
        std::ifstream input(versionFivePath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("V10E/version-five-settings-migrate-post-processing-off",
              versionFive.directionalShadowQuality ==
                      DirectionalShadowQuality::High &&
                  versionFive.postProcessingQuality ==
                      PostProcessingQuality::Off &&
                  text.find("settings_version 8\n") == 0 &&
                  text.find("postprocessingquality off\n") !=
                      std::string::npos,
              text);
    }

    const std::filesystem::path versionSixPath =
        directory / "version-six-config.txt";
    {
        std::ofstream output(versionSixPath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 6\n"
               << "directionalshadowquality medium\n"
               << "postprocessingquality on\n"
               << "key_consume_food q\n";
    }
    const Config versionSix = loadRuntimeConfig(versionSixPath.string());
    {
        std::ifstream input(versionSixPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("P11A/version-six-settings-migrate-input-defaults",
              versionSix.directionalShadowQuality ==
                      DirectionalShadowQuality::Medium &&
                  versionSix.postProcessingQuality ==
                      PostProcessingQuality::On &&
                  versionSix.inputBindings.get(
                      GameplayAction::ConsumeFood) == GameplayKey::Q &&
                  versionSix.sprintMode == GameplayHoldMode::Hold &&
                  versionSix.sneakMode == GameplayHoldMode::Hold &&
                  versionSix.mouseBindings.get(
                      GameplayWorldAction::BreakAttack) ==
                      GameplayMouseButton::Primary &&
                  versionSix.mouseBindings.get(
                      GameplayWorldAction::Guard) ==
                      GameplayMouseButton::Secondary &&
                  text.find("settings_version 8\n") == 0 &&
                  versionSix.feedbackIntensity ==
                      GameplayFeedbackIntensity::Full,
              text);
    }

    const std::filesystem::path versionSevenPath =
        directory / "version-seven-config.txt";
    {
        std::ofstream output(versionSevenPath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 7\n"
               << "directionalshadowquality off\n"
               << "postprocessingquality off\n"
               << "sprintmode toggle\n"
               << "sneakmode hold\n"
               << "mouse_break_attack primary\n"
               << "mouse_use secondary\n"
               << "mouse_place secondary\n"
               << "mouse_guard secondary\n";
    }
    const Config versionSeven = loadRuntimeConfig(versionSevenPath.string());
    {
        std::ifstream input(versionSevenPath, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        check("P11B/version-seven-settings-migrate-feedback-full",
              versionSeven.sprintMode == GameplayHoldMode::Toggle &&
                  versionSeven.feedbackIntensity ==
                      GameplayFeedbackIntensity::Full &&
                  text.find("settings_version 8\n") == 0 &&
                  text.find("feedbackintensity full\n") !=
                      std::string::npos,
              text);
    }

    RuntimeSettingsSession session;
    session.begin(userSettings(customised));
    session.draft().fov = 115;
    session.draft().renderDistance = 5;
    session.draft().directionalShadowQuality =
        DirectionalShadowQuality::Medium;
    session.draft().postProcessingQuality =
        PostProcessingQuality::On;
    RuntimeSettingsApplyPlan plan;
    std::string settingsError;
    check("G4/settings-draft-prepares-valid-apply",
          session.prepareApply(plan, settingsError) &&
              plan.settings.fov == 115 &&
              plan.renderDistanceChanged &&
              plan.directionalShadowQualityChanged &&
              plan.postProcessingQualityChanged &&
              !plan.restartRequired,
          settingsError);
    session.cancel();
    check("G4/settings-cancel-restores-snapshot",
          !session.isOpen() && session.draft().fov == customised.fov &&
              session.draft().renderDistance == customised.renderDistance);

    session.begin(userSettings(customised));
    session.draft().windowX = 1600;
    check("G4/display-change-requires-restart",
          session.prepareApply(plan, settingsError) &&
              plan.restartRequired);
    session.restoreDefaults();
    check("G4/settings-defaults-are-bounded",
          session.prepareApply(plan, settingsError) &&
              plan.settings.windowX == 1280 &&
              plan.settings.renderDistance == 8 &&
              plan.settings.directionalShadowQuality ==
                  DirectionalShadowQuality::Off &&
              plan.settings.postProcessingQuality ==
                  PostProcessingQuality::Off &&
              std::abs(plan.settings.masterVolume - 1.0f) < 0.0001f &&
              std::abs(plan.settings.musicVolume - 0.65f) < 0.0001f);
    session.draft().fov = 121;
    check("G4/settings-reject-out-of-range-draft",
          !session.prepareApply(plan, settingsError) &&
              settingsError.find("FOV") != std::string::npos,
          settingsError);

    Config persisted = customised;
    persisted.worldSeed = 77123;
    userSettings(persisted) = plan.settings;
    persisted.fov = 96;
    persisted.uiScale = 1.25f;
    persisted.locale = "zh-CN";
    persisted.musicVolume = 0.4f;
    persisted.directionalShadowQuality =
        DirectionalShadowQuality::High;
    persisted.postProcessingQuality = PostProcessingQuality::On;
    persisted.audioCaptions = false;
    persisted.showActionHints = false;
    persisted.inputBindings.set(GameplayAction::ConsumeFood,
                                 GameplayKey::Q);
    persisted.mouseBindings.set(GameplayWorldAction::Guard,
                                 GameplayMouseButton::Side1);
    persisted.sprintMode = GameplayHoldMode::Toggle;
    persisted.sneakMode = GameplayHoldMode::Toggle;
    persisted.feedbackIntensity = GameplayFeedbackIntensity::Reduced;
    check("G4/settings-save-publishes-valid-candidate",
          saveRuntimeConfig(configPath.string(), persisted,
                            &settingsError),
          settingsError);
    const Config reloaded = loadRuntimeConfig(configPath.string());
    check("G4/settings-save-preserves-world-creation-seed",
          reloaded.fov == 96 && reloaded.worldSeed.has_value() &&
              *reloaded.worldSeed == 77123 &&
              std::abs(reloaded.uiScale - 1.25f) < 0.0001f &&
              reloaded.locale == "zh-CN" &&
              std::abs(reloaded.musicVolume - 0.4f) < 0.0001f &&
              reloaded.directionalShadowQuality ==
                  DirectionalShadowQuality::High &&
              reloaded.postProcessingQuality ==
                  PostProcessingQuality::On &&
              !reloaded.audioCaptions && !reloaded.showActionHints &&
              reloaded.inputBindings.get(GameplayAction::ConsumeFood) ==
                  GameplayKey::Q &&
              reloaded.mouseBindings.get(GameplayWorldAction::Guard) ==
                  GameplayMouseButton::Side1 &&
              reloaded.sprintMode == GameplayHoldMode::Toggle &&
              reloaded.sneakMode == GameplayHoldMode::Toggle &&
              reloaded.feedbackIntensity ==
                  GameplayFeedbackIntensity::Reduced);

    Config rejected = reloaded;
    rejected.fov = 101;
    StorageTransactionOptions fault;
    fault.faultPoint = StorageFaultPoint::BeforeReplace;
    check("G4/settings-atomic-failure-is-reported",
          !saveRuntimeConfig(configPath.string(), rejected,
                             &settingsError, fault) &&
              settingsError.find("before-replace") !=
                  std::string::npos,
          settingsError);
    const Config afterFailure = loadRuntimeConfig(configPath.string());
    check("G4/settings-atomic-failure-keeps-previous-file",
          afterFailure.fov == 96 && afterFailure.worldSeed.has_value() &&
              *afterFailure.worldSeed == 77123);

    Camera settingsCamera(afterFailure);
    const float previousProjection = settingsCamera.getProjMatrix()[1][1];
    settingsCamera.setFov(110);
    check("G4/live-fov-refreshes-logic-camera",
          std::abs(settingsCamera.getProjMatrix()[1][1] -
                   previousProjection) > 0.0001f);

    const std::filesystem::path futurePath =
        directory / "future-config.txt";
    {
        std::ofstream output(futurePath,
                             std::ios::binary | std::ios::trunc);
        output << "settings_version 999\n";
    }
    bool futureRejected = false;
    try {
        (void)loadRuntimeConfig(futurePath.string());
    }
    catch (const std::exception &) {
        futureRejected = true;
    }
    check("G4/unknown-settings-version-rejected", futureRejected);

    const auto invalidSettingsRejected =
        [&directory](const std::string &name, const std::string &content) {
            const std::filesystem::path path = directory / name;
            {
                std::ofstream output(path,
                                     std::ios::binary | std::ios::trunc);
                output << content;
            }
            try {
                (void)loadRuntimeConfig(path.string());
            }
            catch (const std::exception &) {
                return true;
            }
            return false;
        };
    check("N6/invalid-accessibility-and-bindings-are-rejected",
          invalidSettingsRejected("bad-scale.txt",
                                  "settings_version 2\nuiscale 2\n") &&
              invalidSettingsRejected(
                  "duplicate-binding.txt",
                  "settings_version 2\nkey_consume_food e\n") &&
              invalidSettingsRejected(
                  "unknown-binding.txt",
                  "settings_version 2\nkey_consume_food mouse9\n") &&
              invalidSettingsRejected(
                  "old-version-locale.txt",
                  "settings_version 2\nlocale zh-CN\n") &&
              invalidSettingsRejected(
                  "legacy-locale.txt", "locale zh-CN\n") &&
              invalidSettingsRejected(
                  "unknown-locale.txt",
                  "settings_version 3\nlocale fr-FR\n") &&
              invalidSettingsRejected(
                  "old-version-music.txt",
                  "settings_version 3\nmusicvolume 0.5\n") &&
              invalidSettingsRejected(
                  "invalid-music-volume.txt",
                  "settings_version 4\nmusicvolume 1.1\n") &&
              invalidSettingsRejected(
                  "old-version-shadow.txt",
                  "settings_version 4\ndirectionalshadowquality medium\n") &&
              invalidSettingsRejected(
                  "missing-v5-shadow.txt",
                  "settings_version 5\nrenderdistance 8\n") &&
              invalidSettingsRejected(
                  "invalid-v5-shadow.txt",
                  "settings_version 5\ndirectionalshadowquality ultra\n") &&
              invalidSettingsRejected(
                  "old-version-post.txt",
                  "settings_version 5\ndirectionalshadowquality off\npostprocessingquality on\n") &&
              invalidSettingsRejected(
                  "missing-v6-post.txt",
                  "settings_version 6\ndirectionalshadowquality off\n") &&
              invalidSettingsRejected(
                  "missing-v6-shadow.txt",
                  "settings_version 6\npostprocessingquality off\n") &&
              invalidSettingsRejected(
                  "invalid-v6-post.txt",
                  "settings_version 6\ndirectionalshadowquality off\npostprocessingquality ultra\n"));

    const std::string v7Prefix =
        "settings_version 7\n"
        "directionalshadowquality off\n"
        "postprocessingquality off\n";
    const std::string v7MouseBindings =
        "mouse_break_attack primary\n"
        "mouse_use secondary\n"
        "mouse_place secondary\n"
        "mouse_guard secondary\n";
    check("P11A/invalid-v7-input-settings-are-rejected",
          invalidSettingsRejected(
              "old-version-input-mode.txt",
              "settings_version 6\ndirectionalshadowquality off\n"
              "postprocessingquality off\nsprintmode hold\n") &&
              invalidSettingsRejected(
                  "missing-v7-mouse.txt",
                  v7Prefix + "sprintmode hold\nsneakmode hold\n"
                  "mouse_break_attack primary\n"
                  "mouse_use secondary\nmouse_place secondary\n") &&
              invalidSettingsRejected(
                  "invalid-v7-mode.txt",
                  v7Prefix + "sprintmode sticky\nsneakmode hold\n" +
                      v7MouseBindings) &&
              invalidSettingsRejected(
                  "invalid-v7-mouse.txt",
                  v7Prefix + "sprintmode hold\nsneakmode hold\n"
                  "mouse_break_attack mouse9\n"
                  "mouse_use secondary\nmouse_place secondary\n"
                  "mouse_guard secondary\n") &&
              invalidSettingsRejected(
                  "conflicting-v7-mouse.txt",
                  v7Prefix + "sprintmode hold\nsneakmode hold\n"
                  "mouse_break_attack primary\nmouse_use primary\n"
                  "mouse_place secondary\nmouse_guard secondary\n"));

    const std::string v8Required =
        "settings_version 8\n"
        "directionalshadowquality off\n"
        "postprocessingquality off\n"
        "sprintmode hold\n"
        "sneakmode hold\n" + v7MouseBindings;
    check("P11B/invalid-v8-feedback-settings-are-rejected",
          invalidSettingsRejected(
              "old-version-feedback.txt",
              v7Prefix + "sprintmode hold\nsneakmode hold\n" +
                  v7MouseBindings + "feedbackintensity reduced\n") &&
              invalidSettingsRejected(
                  "missing-v8-feedback.txt", v8Required) &&
              invalidSettingsRejected(
                  "invalid-v8-feedback.txt",
                  v8Required + "feedbackintensity strong\n"));
}

std::string readTextFile(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void caseWorldOutcomeAndLocalizedText()
{
    VictoryFlow flow;
    check("N7A/outcome-starts-unstarted",
          flow.snapshot().phase == WorldOutcomePhase::Unstarted &&
              !flow.snapshot().victory);
    check("N7A/outcome-rejects-skipped-transition",
          flow.beginEncounter() == VictoryTransitionResult::Rejected &&
              flow.resolveVictory(1) == VictoryTransitionResult::Rejected);
    check("N7A/activation-and-encounter-are-idempotent",
          flow.activate() == VictoryTransitionResult::Applied &&
              flow.activate() == VictoryTransitionResult::AlreadyApplied &&
              flow.beginEncounter() == VictoryTransitionResult::Applied &&
              flow.beginEncounter() ==
                  VictoryTransitionResult::AlreadyApplied);
    check("N7A/abandoned-encounter-can-resume",
          flow.abandonEncounter() == VictoryTransitionResult::Applied &&
              flow.abandonEncounter() ==
                  VictoryTransitionResult::AlreadyApplied &&
              flow.beginEncounter() == VictoryTransitionResult::Applied);
    check("N7A/victory-requires-nonzero-reward-epoch",
          flow.resolveVictory(0) == VictoryTransitionResult::Rejected &&
              flow.resolveVictory(41) == VictoryTransitionResult::Applied &&
              flow.resolveVictory(41) ==
                  VictoryTransitionResult::AlreadyApplied &&
              flow.resolveVictory(42) == VictoryTransitionResult::Rejected);
    const WorldOutcomeSnapshot victorious = flow.snapshot();
    check("N7A/victory-snapshot-is-authoritative",
          victorious.victory && victorious.rewardAvailable &&
              !victorious.rewardClaimed && victorious.rewardEpoch == 41);
    check("N7A/reward-claim-is-epoch-bound-and-idempotent",
          flow.claimReward(42) == VictoryTransitionResult::Rejected &&
              flow.claimReward(41) == VictoryTransitionResult::Applied &&
              flow.claimReward(41) ==
                  VictoryTransitionResult::AlreadyApplied &&
              flow.snapshot().rewardClaimed &&
              !flow.snapshot().rewardAvailable);

    bool invalidRestoreRejected = false;
    try {
        VictoryFlow invalid({WorldOutcomePhase::RewardClaimed, 4, 3});
        (void)invalid;
    }
    catch (const std::invalid_argument &) {
        invalidRestoreRejected = true;
    }
    check("N7A/invalid-restored-outcome-is-rejected",
          invalidRestoreRejected);

    const std::string english =
        readTextFile(ResourcePaths::media("text/en-US.text"));
    const std::string chinese =
        readTextFile(ResourcePaths::media("text/zh-CN.text"));
    LocalizedTextRegistry registry;
    registry.freeze({{"en-US.text", english}, {"zh-CN.text", chinese}});
    check("N7A/locales-freeze-with-identical-semantic-keys",
          registry.isFrozen() && registry.hasLocale("en-US") &&
              registry.hasLocale("zh-CN") &&
              registry.keys("en-US") == registry.keys("zh-CN") &&
              registry.keys("en-US").size() == 411 &&
              registry.lookup("en-US", "material.torch.name") ==
                  "Torch" &&
              registry.lookup("zh-CN", "material.torch.name") ==
                  "火把" &&
              registry.lookup("en-US", "material.oak_door.name") ==
                  "Oak Door" &&
              registry.lookup("zh-CN", "material.wooden_shovel.name") ==
                  "木铲" &&
              registry.lookup("en-US", "settings.music_volume") ==
                  "Music" &&
              registry.lookup("zh-CN", "settings.music_volume") !=
                  registry.lookup("en-US", "settings.music_volume"));
    check("N7A/localized-victory-text-resolves",
          registry.lookup("en-US", "victory.overlay.title") ==
                  "Waystone Restored" &&
              registry.lookup("zh-CN", "victory.overlay.title") !=
                  registry.lookup("en-US", "victory.overlay.title"));

    ensureRuntimeLocalizedTextRegistry();
    check("N12A/semantic-ids-localize-without-changing-identity",
          LocalizedPresentation::materialName(
              "zh-CN", Material::ID::IronIngot) ==
                  runtimeLocalizedTextRegistry().lookup(
                      "zh-CN", "material.iron_ingot.name") &&
              LocalizedPresentation::objectiveText(
                  "zh-CN", "alpha.gather_wood", "title") ==
                  runtimeLocalizedTextRegistry().lookup(
                      "zh-CN", "objective.alpha.gather_wood.title") &&
              LocalizedPresentation::audioCaption(
                  "zh-CN", "block.break") ==
                  runtimeLocalizedTextRegistry().lookup(
                      "zh-CN", "audio.block.break.caption") &&
              LocalizedPresentation::materialName(
                  "zh-CN", Material::ID::IronIngot) !=
                  LocalizedPresentation::materialName(
                      "en-US", Material::ID::IronIngot));

    ensureRuntimeObjectiveRegistry();
    bool semanticCoverageComplete = true;
    for (int value = 0; value < static_cast<int>(Material::ID::Count); ++value)
    {
        std::string id = Material::toStringId(
            static_cast<Material::ID>(value));
        const std::size_t separator = id.find(':');
        if (separator != std::string::npos)
        {
            id.erase(0, separator + 1);
        }
        const std::string key = "material." + id + ".name";
        semanticCoverageComplete = semanticCoverageComplete &&
            registry.hasKey("en-US", key) && registry.hasKey("zh-CN", key);
    }
    for (const ObjectiveDefinition& definition :
         runtimeObjectiveRegistry().definitions())
    {
        const std::string prefix = "objective." + definition.id + ".";
        semanticCoverageComplete = semanticCoverageComplete &&
            registry.hasKey("en-US", prefix + "title") &&
            registry.hasKey("zh-CN", prefix + "title") &&
            registry.hasKey("en-US", prefix + "instruction") &&
            registry.hasKey("zh-CN", prefix + "instruction") &&
            registry.hasKey("en-US", prefix + "feedback") &&
            registry.hasKey("zh-CN", prefix + "feedback");
    }
    const std::array<const char*, 9> captionCueIds = {{
        "ui.click", "block.break", "block.place", "item.pickup",
        "craft.success", "combat.hit", "combat.windup", "combat.guard",
        "ambient.wind"}};
    for (const char* cueId : captionCueIds)
    {
        const std::string key = "audio." + std::string(cueId) + ".caption";
        semanticCoverageComplete = semanticCoverageComplete &&
            registry.hasKey("en-US", key) && registry.hasKey("zh-CN", key);
    }
    check("N12A/all-material-objective-and-caption-ids-have-text",
          semanticCoverageComplete);

    bool parityRejected = false;
    try {
        LocalizedTextRegistry mismatched;
        mismatched.freeze({
            {"mismatch-en", "# HelloMine3D localized text v1\n"
                            "locale en-US\n"
                            "text app.title \"HelloMine3D\"\n"},
            {"mismatch-zh", "# HelloMine3D localized text v1\n"
                            "locale zh-CN\n"
                            "text common.close \"CloseZh\"\n"}});
    }
    catch (const std::runtime_error &) {
        parityRejected = true;
    }
    check("N12A/production-catalogues-require-key-parity",
          parityRejected);

    LocalizedTextRegistry fallback;
    fallback.freeze({
        {"fallback-en", "# HelloMine3D localized text v1\n"
                        "locale en-US\n"
                        "text victory.overlay.title \"Victory\"\n"},
        {"fallback-zh", "# HelloMine3D localized text v1\n"
                        "locale zh-CN\n"
                        "text world.list.completed \"Complete\"\n"}},
        false);
    const bool translationFallback =
        fallback.lookup("zh-CN", "victory.overlay.title") == "Victory";
    const bool localeFallback =
        fallback.lookup("fr-FR", "victory.overlay.title") == "Victory";
    const bool missingFallback =
        fallback.lookup("zh-CN", "missing.semantic.key") ==
        "[missing.semantic.key]";
    fallback.lookup("zh-CN", "missing.semantic.key");
    check("N7A/text-fallbacks-are-bounded-and-diagnosed",
          translationFallback && localeFallback && missingFallback &&
              fallback.diagnostics().size() == 3);

    LocalizedTextRegistry invalidText;
    bool invalidTextRejected = false;
    try {
        invalidText.freeze({
            {"invalid", "# HelloMine3D localized text v1\n"
                        "locale en-US\n"
                        "text Invalid.Key \"bad\"\n"}});
    }
    catch (const std::runtime_error &) {
        invalidTextRejected = true;
    }
    check("N7A/invalid-text-source-does-not-partially-freeze",
          invalidTextRejected && !invalidText.isFrozen());

    auto rejectedLocalizedValue = [](const std::string& value)
    {
        LocalizedTextRegistry candidate;
        try
        {
            candidate.freeze({
                {"invalid-value", "# HelloMine3D localized text v1\n"
                                  "locale en-US\n"
                                  "text test.value \"" + value + "\"\n"}});
        }
        catch (const std::runtime_error&)
        {
            return !candidate.isFrozen();
        }
        return false;
    };
    const unsigned char invalidUtf8Bytes[] = {0xf0u, 0x28u, 0x8cu, 0x28u};
    const std::string invalidUtf8(
        reinterpret_cast<const char*>(invalidUtf8Bytes),
        sizeof(invalidUtf8Bytes));
    check("N12A/invalid-utf8-and-oversized-text-are-rejected",
          rejectedLocalizedValue(invalidUtf8) &&
              rejectedLocalizedValue(std::string(
                  LocalizedTextRegistry::MaxTextBytes + 1, 'x')));

    const PresentationFontProbe bundledFont = probePresentationFont(
        ResourcePaths::media("fonts/NotoSansSC-VF.ttf"));
    const PresentationFontProbe missingFont = probePresentationFont(
        ResourcePaths::media("fonts/missing-presentation-font.ttf"));
    check("N12A/font-probe-accepts-bundle-and-bounds-missing-fallback",
          bundledFont.usable && bundledFont.bytes > 1024u * 1024u &&
              !missingFont.usable && !missingFont.diagnostic.empty());
    const std::string fontLicense = readTextFile(
        ResourcePaths::media("fonts/NotoSansSC-OFL.txt"));
    const std::string audioLicense = readTextFile(
        ResourcePaths::media(
            "audio/samples/LICENSE-HelloMine3D-Audio.txt"));
    const std::string musicLicense = readTextFile(
        ResourcePaths::media(
            "music/tracks/LICENSE-HelloMine3D-Music.txt"));
    check("N12A/credits-and-font-license-assets-are-present",
          registry.lookup("en-US", "credits.title") ==
                  "Credits and Licenses" &&
              registry.lookup("zh-CN", "credits.title") !=
                  registry.lookup("en-US", "credits.title") &&
              registry.lookup("en-US", "credits.font_path").find(
                  "media/fonts/NotoSansSC-OFL.txt") != std::string::npos &&
              fontLicense.find("SIL OPEN FONT LICENSE Version 1.1") !=
                  std::string::npos &&
              registry.lookup("en-US", "credits.audio_path").find(
                  "media/audio/samples/LICENSE-HelloMine3D-Audio.txt") !=
                  std::string::npos &&
              registry.lookup("zh-CN", "credits.audio_name") !=
                  registry.lookup("en-US", "credits.audio_name") &&
              audioLicense.find("MIT License") != std::string::npos &&
              registry.lookup("en-US", "credits.music_path").find(
                  "media/music/tracks/LICENSE-HelloMine3D-Music.txt") !=
                  std::string::npos &&
              registry.lookup("zh-CN", "credits.music_name") !=
                  registry.lookup("en-US", "credits.music_name") &&
              musicLicense.find("MIT License") != std::string::npos);

    const PresentationWindowLayout minimumLayout =
        fitPresentationWindow(640.0f, 480.0f, 820.0f, 660.0f, 1.75f);
    const PresentationWindowLayout maximumLayout =
        fitPresentationWindow(7680.0f, 4320.0f, 820.0f, 660.0f, 0.75f);
    const PresentationWindowLayout invalidLayout =
        fitPresentationWindow(NAN, NAN, 820.0f, 660.0f, NAN);
    const std::size_t longTextLines =
        estimateWrappedPresentationLines(
            std::string(LocalizedTextRegistry::MaxTextBytes, 'x'), 32);
    check("N12A/extreme-ui-scale-layout-remains-on-screen",
          minimumLayout.width == 610.0f &&
              minimumLayout.height == 450.0f &&
              minimumLayout.scrollRequired &&
              maximumLayout.width == 820.0f &&
              maximumLayout.height == 660.0f &&
              !maximumLayout.scrollRequired &&
              invalidLayout.width == 610.0f &&
              invalidLayout.height == 450.0f && longTextLines == 32);

    PresentationCaptionTimeline captionTimeline;
    captionTimeline.submit("ambient.wind", "Wind");
    captionTimeline.update(2.4f);
    const PresentationCaptionSnapshot nearlyExpired =
        captionTimeline.snapshot();
    captionTimeline.submit("ambient.wind", "Wind");
    const PresentationCaptionSnapshot refreshed = captionTimeline.snapshot();
    captionTimeline.submit("combat.windup", "Enemy attack warning");
    captionTimeline.submit("ambient.wind", "Wind");
    const PresentationCaptionSnapshot protectedCombat =
        captionTimeline.snapshot();
    captionTimeline.update(PresentationCaptionTimeline::DurationSeconds);
    check("N12A/caption-duration-refresh-priority-and-expiry-are-bounded",
          nearlyExpired.visible() && nearlyExpired.remainingSeconds < 0.11f &&
              refreshed.remainingSeconds ==
                  PresentationCaptionTimeline::DurationSeconds &&
              protectedCombat.cueId == "combat.windup" &&
              !captionTimeline.snapshot().visible());

    const auto saveDirectory = freshSaveDirectory("n7a_outcome_current");
    WorldSaveData valid;
    valid.worldId = "n7a-outcome-current";
    valid.worldName = "N7A Outcome Current";
    valid.seed = kValidationSeed;
    valid.createdUtc = LegacyWorldTimestampUtc;
    valid.lastPlayedUtc = LegacyWorldTimestampUtc;
    valid.lastBuildIdentity = "validation";
    valid.worldOutcome = {WorldOutcomePhase::Victorious, 41, 0};
    WorldSave save(saveDirectory);
    WorldSaveData roundTrip;
    const bool saved = save.save(valid) && save.load(roundTrip);
    check("N7A/current-outcome-round-trips",
          saved && roundTrip.version == WorldSaveFormatVersion &&
              roundTrip.worldOutcome.phase ==
                  WorldOutcomePhase::Victorious &&
              roundTrip.worldOutcome.rewardEpoch == 41 &&
              roundTrip.worldOutcome.claimedRewardEpoch == 0);
    WorldSaveData invalidOutcome = valid;
    invalidOutcome.worldOutcome =
        {WorldOutcomePhase::RewardClaimed, 41, 40};
    WorldSaveData preserved;
    check("N7A/invalid-outcome-preserves-last-good-save",
          !save.save(invalidOutcome) && save.load(preserved) &&
              preserved.worldOutcome.phase ==
                  WorldOutcomePhase::Victorious &&
              preserved.worldOutcome.rewardEpoch == 41);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("n7a_v8_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n7a-v8-outcome";
    std::filesystem::create_directories(migratedWorld);
    std::filesystem::copy_file(
        ResourcePaths::join(
            ResourcePaths::projectRoot(),
            "tools/fixtures/victory/world-v8-pre-outcome.meta"),
        migratedWorld / "world.meta",
        std::filesystem::copy_options::overwrite_existing);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("n7a-v8-outcome");
    WorldSaveData migratedData;
    const bool migratedLoaded =
        migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    check("N7A/v8-migrates-to-unstarted-current-outcome",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.worldOutcome.phase ==
                  WorldOutcomePhase::Unstarted &&
              migratedData.worldOutcome.rewardEpoch == 0 &&
              migratedData.worldOutcome.claimedRewardEpoch == 0);
}

void caseWaystoneVictoryLoop()
{
    WaystoneEncounterState payloadState{
        1, WaystoneEncounter::FirstWaveGuardians, 0};
    WaystoneEncounterState decoded;
    const std::string payload =
        WaystoneEncounter::serialize(payloadState);
    std::string payloadError;
    check("N7B/waystone-payload-is-strict-and-versioned",
          WaystoneEncounter::deserialize(payload, decoded,
                                          &payloadError) &&
              decoded.wave == 1 &&
              decoded.remainingGuardians == 2 &&
              !WaystoneEncounter::deserialize(
                  payload + "wave 2\n", decoded, &payloadError) &&
              !WaystoneEncounter::validState({2, 2, 0}),
          payloadError);

    Inventory atomicInventory(2);
    atomicInventory.addItem(Material::IRON_INGOT, 2);
    const std::vector<InventorySlotState> ritual = {{
        Material::ID::IronIngot,
        WaystoneEncounter::ActivationIronIngots, 0}};
    const std::uint64_t atomicRevision = atomicInventory.revision();
    check("N7B/ritual-consumption-is-revision-guarded-and-atomic",
          atomicInventory.canConsume(ritual) &&
              !atomicInventory.consume(ritual, atomicRevision + 1) &&
              atomicInventory.count(Material::ID::IronIngot) == 2 &&
              atomicInventory.consume(ritual, atomicRevision) &&
              atomicInventory.count(Material::ID::IronIngot) == 0);

    ObjectiveRegistry finaleRegistry;
    finaleRegistry.freeze({{
        "Base.objective",
        readTextFile(ResourcePaths::media("objectives/Base.objective"))}});
    const ObjectiveDefinition *stalkerObjective =
        finaleRegistry.find("finale.defeat_stalkers");
    const ObjectiveDefinition *claimObjective =
        finaleRegistry.find("finale.claim_reward");
    check("N7B/five-ordered-finale-objectives-are-data-driven",
          finaleRegistry.definitionVersion() == 3 &&
              finaleRegistry.definitions().size() == 34 &&
              finaleRegistry.find("finale.prepare_ritual") != nullptr &&
              finaleRegistry.find("finale.activate_waystone") != nullptr &&
              finaleRegistry.find("finale.activate_waystone")->type ==
                  ObjectiveType::ActivateWaystone &&
              stalkerObjective != nullptr &&
              stalkerObjective->targetActorType ==
                  WaystoneEncounter::StalkerType &&
              claimObjective != nullptr &&
              claimObjective->type ==
                  ObjectiveType::ClaimVictoryReward);
    ObjectiveSaveState finaleProgress;
    for (const ObjectiveDefinition &definition :
         finaleRegistry.definitions()) {
        if (definition.id == "finale.activate_waystone") {
            break;
        }
        finaleProgress.completedIds.push_back(definition.id);
    }
    Player objectivePlayer;
    SandboxEventBus objectiveBus;
    ObjectiveSystem finaleObjectives(
        finaleRegistry, objectivePlayer, objectiveBus, finaleProgress,
        ObjectiveState::LegacyAlphaKnownFlags, false);
    objectiveBus.publish(WaystoneActivatedEvent(
        DefaultPlayerActorId, {0, 0, 0}));
    objectiveBus.publish(EntityDeathEvent(
        2, DefaultPlayerActorId, {}, World::StalkerMobType));
    const bool wrongActorIgnored =
        finaleObjectives.progress("finale.defeat_stalkers") == 0;
    objectiveBus.publish(EntityDeathEvent(
        3, DefaultPlayerActorId, {}, WaystoneEncounter::StalkerType));
    objectiveBus.publish(EntityDeathEvent(
        4, DefaultPlayerActorId, {}, WaystoneEncounter::StalkerType));
    objectiveBus.publish(EntityDeathEvent(
        5, DefaultPlayerActorId, {}, WaystoneEncounter::BruteType));
    objectiveBus.publish(VictoryRewardClaimedEvent(
        DefaultPlayerActorId, WaystoneEncounter::RewardEpoch));
    const ObjectiveSnapshot finaleComplete = finaleObjectives.snapshot();
    check("N7B/finale-objectives-filter-actors-and-remain-read-only-guidance",
          wrongActorIgnored && finaleComplete.sessionComplete &&
              finaleComplete.completedObjectives == 33);

    setEnv("HELLOMINE3D_SEED", "20260825");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    setEnv("HELLOMINE3D_WORLD_TIME", "1");
    const std::string saveDirectory =
        freshSaveDirectory("n7b_waystone_victory");
    const glm::ivec3 core{9, 100, 8};
    const Config config = makeConfig();

    const auto prepareArena = [&core](World &world) {
        for (int x = 1; x <= 15; ++x) {
            for (int z = 1; z <= 15; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
            }
        }
        world.setBlock(core.x, core.y, core.z,
                       BlockId::WaystoneCore);
        return world.initializeWaystone(core);
    };
    const auto actorIds = [](const World &world,
                             const std::string &type) {
        std::vector<ActorId> ids;
        for (const ActorSnapshot &actor :
             world.getActorManager().collectSnapshots()) {
            if (actor.type == type) {
                ids.push_back(actor.id);
            }
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    };

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, saveDirectory, false, 1);
        const bool arenaReady = prepareArena(world);
        const WaystoneActionResult paused =
            world.useWaystone(core, player, false);
        const WaystoneActionResult missing =
            world.useWaystone(core, player, true);
        const bool supplied =
            player.addItem(Material::IRON_INGOT,
                           WaystoneEncounter::ActivationIronIngots) ==
            WaystoneEncounter::ActivationIronIngots;
        const WaystoneActionResult activated =
            world.useWaystone(core, player, true);
        WaystoneEncounterState stored;
        check("N7B/normal-use-activation-conserves-materials",
              arenaReady && supplied &&
                  paused == WaystoneActionResult::SimulationPaused &&
                  missing == WaystoneActionResult::MissingMaterials &&
                  activated == WaystoneActionResult::Activated &&
                  player.getInventoryCount(Material::ID::IronIngot) == 0 &&
                  world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Activated &&
                  world.getWaystoneEncounterSnapshot().loadedGuardians == 0 &&
                  world.getBlockEntity(core).has_value() &&
                  WaystoneEncounter::deserialize(
                      world.getBlockEntity(core)->payload, stored) &&
                  stored.wave == 0);
        check("N7B/activated-stage-saves-before-encounter",
              world.save());
    }

    ActorId killedBeforeSave = InvalidActorId;
    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, saveDirectory, false, 1);
        const WaystoneActionResult started =
            world.useWaystone(core, player, true);
        const auto firstWave =
            actorIds(world, WaystoneEncounter::StalkerType);
        check("N7B/activated-stage-reopens-and-starts-bounded-first-wave",
              world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Encounter &&
                  started == WaystoneActionResult::EncounterStarted &&
                  firstWave.size() ==
                      WaystoneEncounter::FirstWaveGuardians &&
                  world.getWaystoneEncounterSnapshot().loadedGuardians <=
                      WaystoneEncounter::MaximumLoadedGuardians);

        const bool deathAccepted = world.damagePlayer(100.f);
        check("N7B/player-death-abandons-encounter-without-losing-activation",
              deathAccepted &&
                  world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Activated &&
                  world.getWaystoneEncounterSnapshot().loadedGuardians == 0);
        world.tick(2);
        const WaystoneActionResult restarted =
            world.useWaystone(core, player, true);
        const auto restartedWave =
            actorIds(world, WaystoneEncounter::StalkerType);
        check("N7B/death-reset-can-restart-the-same-finite-encounter",
              restarted == WaystoneActionResult::EncounterStarted &&
                  restartedWave.size() == 2);
        if (!restartedWave.empty()) {
            killedBeforeSave = restartedWave.front();
            world.attackActor(killedBeforeSave, 100.f);
        }
        const WaystoneEncounterSnapshot partial =
            world.getWaystoneEncounterSnapshot();
        check("N7B/half-wave-death-updates-persisted-remaining-count",
              partial.wave == 1 && partial.remainingGuardians == 1 &&
                  partial.loadedGuardians == 1 && world.save());
    }

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, saveDirectory, false, 1);
        const auto restoredStalkers =
            actorIds(world, WaystoneEncounter::StalkerType);
        check("N7B/half-wave-reopen-reconciles-without-duplicate-spawn",
              world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Encounter &&
                  restoredStalkers.size() == 1 &&
                  world.getWaystoneEncounterSnapshot().remainingGuardians ==
                      1);
        if (!restoredStalkers.empty()) {
            world.attackActor(restoredStalkers.front(), 100.f);
        }
        const auto bruteWave =
            actorIds(world, WaystoneEncounter::BruteType);
        const WaystoneEncounterSnapshot beforeDuplicate =
            world.getWaystoneEncounterSnapshot();
        world.getEventBus().publish(EntityDeathEvent(
            killedBeforeSave, DefaultPlayerActorId, {},
            WaystoneEncounter::StalkerType));
        const WaystoneEncounterSnapshot afterDuplicate =
            world.getWaystoneEncounterSnapshot();
        check("N7B/duplicate-death-event-cannot-skip-the-final-wave",
              bruteWave.size() == 1 && beforeDuplicate.wave == 2 &&
                  beforeDuplicate.remainingGuardians == 1 &&
                  afterDuplicate.wave == beforeDuplicate.wave &&
                  afterDuplicate.remainingGuardians ==
                      beforeDuplicate.remainingGuardians &&
                  !world.getWorldOutcomeSnapshot().victory);

        const VectorXZ coreChunk =
            World::getChunkXZ(core.x, core.z);
        world.getChunkManager().unloadChunk(coreChunk.x, coreChunk.z);
        const bool frozen =
            world.getWorldOutcomeSnapshot().phase ==
                WorldOutcomePhase::Encounter &&
            world.getWaystoneEncounterSnapshot().loadedGuardians == 0;
        world.getChunkManager().loadChunk(coreChunk.x, coreChunk.z);
        world.tick(3);
        const auto restoredBrute =
            actorIds(world, WaystoneEncounter::BruteType);
        check("N7B/core-chunk-unload-freezes-and-reloads-the-pending-wave",
              frozen && restoredBrute.size() == 1 &&
                  world.getWaystoneEncounterSnapshot().remainingGuardians ==
                      1);
        if (!restoredBrute.empty()) {
            world.attackActor(restoredBrute.front(), 100.f);
        }
        check("N7B/final-guardian-normal-death-resolves-authoritative-victory",
              world.getWorldOutcomeSnapshot().victory &&
                  world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Victorious &&
                  world.getWorldOutcomeSnapshot().rewardAvailable &&
                  world.getWaystoneEncounterSnapshot().loadedGuardians == 0);

        const std::array<const Material *, 5> fillers = {{
            &Material::DIRT_BLOCK, &Material::STONE_BLOCK,
            &Material::OAK_BARK_BLOCK, &Material::WHEAT,
            &Material::COAL_ORE_BLOCK}};
        for (const Material *material : fillers) {
            player.addItem(*material, material->maxStackSize);
        }
        const WaystoneActionResult full =
            world.claimWaystoneReward(true);
        check("N7B/full-inventory-keeps-victory-reward-pending",
              full == WaystoneActionResult::InventoryFull &&
                  world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Victorious &&
                  world.getWorldOutcomeSnapshot().rewardAvailable);
        int freedSlot = -1;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            if (player.getInventorySlot(slot).getMaterial().id ==
                Material::ID::Dirt) {
                freedSlot = slot;
                break;
            }
        }
        if (freedSlot >= 0) {
            player.removeInventoryItem(
                freedSlot,
                player.getInventorySlot(freedSlot).getNumInStack());
        }
        const WaystoneActionResult claimed =
            world.claimWaystoneReward(true);
        const int rewardCount =
            player.getInventoryCount(Material::ID::IronIngot);
        const WaystoneActionResult repeated =
            world.claimWaystoneReward(true);
        check("N7B/reward-claim-is-capacity-checked-and-one-time",
              claimed == WaystoneActionResult::RewardClaimed &&
                  rewardCount == WaystoneEncounter::RewardIronIngots &&
                  repeated ==
                      WaystoneActionResult::RewardAlreadyClaimed &&
                  player.getInventoryCount(Material::ID::IronIngot) ==
                      rewardCount &&
                  world.getWorldOutcomeSnapshot().rewardClaimed);

        world.setBlock(12, 100, 12, BlockId::Dirt);
        check("N7B/victory-keeps-the-world-playing-as-a-sandbox",
              static_cast<BlockId>(world.getBlock(12, 100, 12).id) ==
                      BlockId::Dirt &&
                  world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::RewardClaimed &&
                  world.save());
    }

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, saveDirectory, false, 1);
        check("N7B/reward-claimed-stage-reopens-without-guardians",
              world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::RewardClaimed &&
                  world.getWorldOutcomeSnapshot().rewardClaimed &&
                  world.getWaystoneEncounterSnapshot().loadedGuardians == 0 &&
                  player.getInventoryCount(Material::ID::IronIngot) ==
                      WaystoneEncounter::RewardIronIngots);
    }

    WorldBackup backup(saveDirectory);
    std::vector<WorldBackupInfo> backups;
    std::string backupError;
    WorldBackupMetrics restoreMetrics;
    const bool restoredMidEncounter =
        backup.listBackups(backups, &backupError) &&
        backups.size() == 3 &&
        backup.restoreBackup(backups[1].id, {}, &restoreMetrics);
    check("N7B/mid-encounter-backup-is-valid-and-restorable",
          restoredMidEncounter,
          backupError.empty() ? restoreMetrics.error : backupError);
    if (restoredMidEncounter) {
        Camera camera(config);
        Player player;
        World world(camera, config, player, saveDirectory, false, 1);
        const WaystoneEncounterSnapshot restored =
            world.getWaystoneEncounterSnapshot();
        check("N7B/backup-restore-reconciles-persisted-wave-and-actors",
              world.getWorldOutcomeSnapshot().phase ==
                      WorldOutcomePhase::Encounter &&
                  restored.wave == 1 &&
                  restored.remainingGuardians == 1 &&
                  restored.loadedGuardians == 1 &&
                  actorIds(world, WaystoneEncounter::StalkerType).size() == 1);
    }
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}

void caseP11ACoreInput()
{
    GameplayMouseBindings bindings;
    std::string bindingError;
    check("P11A/default-mouse-context-sharing-is-valid",
          validateGameplayMouseBindings(bindings, bindingError) &&
              describeGameplayMouseBindingSharing(bindings).find(
                  "Use, Place, Guard") != std::string::npos,
          bindingError);
    bindings.set(GameplayWorldAction::Use,
                 GameplayMouseButton::Primary);
    check("P11A/break-attack-mouse-conflict-is-rejected",
          !validateGameplayMouseBindings(bindings, bindingError) &&
              bindingError.find("Mouse primary") != std::string::npos,
          bindingError);

    GameplayWorldActionIntent intent;
    intent.breakAttack = true;
    intent.use = true;
    intent.place = true;
    intent.guard = true;
    GameplayWorldActionContext context;
    context.actorTarget = true;
    context.guardAvailable = true;
    context.usableBlockTarget = true;
    context.placeableHeldItem = true;
    check("P11A/world-action-arbiter-selects-one-owner",
          resolveGameplayWorldAction(intent, context) ==
              GameplayWorldAction::BreakAttack);
    intent.breakAttack = false;
    check("P11A/context-prefers-guard-over-use-and-place",
          resolveGameplayWorldAction(intent, context) ==
              GameplayWorldAction::Guard);
    context.guardAvailable = false;
    check("P11A/context-prefers-usable-block-over-place",
          resolveGameplayWorldAction(intent, context) ==
              GameplayWorldAction::Use);
    context.usableBlockTarget = false;
    check("P11A/context-falls-back-to-place",
          resolveGameplayWorldAction(intent, context) ==
              GameplayWorldAction::Place);

    const GameplayLookDelta oneFrame = calculateGameplayLookDelta(
        120.f, -48.f, 0.05f, false);
    const GameplayLookDelta splitA = calculateGameplayLookDelta(
        45.f, -20.f, 0.05f, false);
    const GameplayLookDelta splitB = calculateGameplayLookDelta(
        75.f, -28.f, 0.05f, false);
    const GameplayLookDelta inverted = calculateGameplayLookDelta(
        120.f, -48.f, 0.05f, true);
    check("P11A/look-curve-is-frame-and-window-independent",
          std::abs(oneFrame.yaw - (splitA.yaw + splitB.yaw)) <
                  0.0001f &&
              std::abs(oneFrame.pitch -
                       (splitA.pitch + splitB.pitch)) < 0.0001f &&
              std::abs(oneFrame.yaw - 6.f) < 0.0001f &&
              std::abs(oneFrame.pitch + 2.4f) < 0.0001f);
    check("P11A/invert-only-flips-pitch-sign",
          std::abs(inverted.yaw - oneFrame.yaw) < 0.0001f &&
              std::abs(inverted.pitch + oneFrame.pitch) < 0.0001f);

    GameplayMovementModeTracker movement;
    GameplayMovementModeState modes = movement.update(
        true, true, true, GameplayHoldMode::Hold,
        GameplayHoldMode::Hold);
    check("P11A/hold-modes-follow-current-key-state",
          modes.sprint && modes.sneak);
    movement.reset();
    modes = movement.update(true, true, false,
                            GameplayHoldMode::Toggle,
                            GameplayHoldMode::Toggle);
    modes = movement.update(true, true, false,
                            GameplayHoldMode::Toggle,
                            GameplayHoldMode::Toggle);
    check("P11A/toggle-mode-uses-press-edge-once",
          modes.sprint && !modes.sneak);
    movement.update(false, true, false, GameplayHoldMode::Toggle,
                    GameplayHoldMode::Toggle);
    modes = movement.update(true, true, false,
                            GameplayHoldMode::Toggle,
                            GameplayHoldMode::Toggle);
    check("P11A/inactive-held-key-does-not-retoggle-on-resume",
          !modes.sprint);
    movement.update(true, false, false, GameplayHoldMode::Toggle,
                    GameplayHoldMode::Toggle);
    modes = movement.update(true, true, false,
                            GameplayHoldMode::Toggle,
                            GameplayHoldMode::Toggle);
    check("P11A/toggle-rearms-after-release", modes.sprint);

    GameplayFocusGate focus;
    check("P11A/initial-focus-accepts-world-input",
          focus.isFocused() && focus.allowsWorldButtons(false) &&
              focus.acceptsLookSample());
    focus.setFocused(false);
    const bool backgroundButtons = focus.allowsWorldButtons(true);
    const bool backgroundLook = focus.acceptsLookSample();
    focus.setFocused(true);
    const bool heldFocusClick = focus.allowsWorldButtons(true);
    const bool firstLook = focus.acceptsLookSample();
    const bool released = focus.allowsWorldButtons(false);
    const bool nextLook = focus.acceptsLookSample();
    check("P11A/focus-gate-blocks-background-and-held-focus-click",
          !backgroundButtons && !backgroundLook && !heldFocusClick);
    check("P11A/focus-gate-discards-one-look-and-rearms-on-release",
          !firstLook && released && nextLook);

    check("P11A/only-interactive-block-behaviors-support-use",
          BlockDatabase::get()
                  .getDefinition(BlockId::Chest)
                  .behavior->supportsUse() &&
              BlockDatabase::get()
                  .getDefinition(BlockId::Workbench)
                  .behavior->supportsUse() &&
              !BlockDatabase::get()
                   .getDefinition(BlockId::Stone)
                   .behavior->supportsUse());
}

void caseP11BActionFeedback()
{
    MiningProgressSnapshot mining;
    check("P11B/inactive-mining-has-no-crack-stage",
          mining.crackStage() == -1);
    mining.active = true;
    mining.requiredSeconds = 1.f;
    mining.elapsedSeconds = 0.f;
    const int stageZero = mining.crackStage();
    mining.elapsedSeconds = 0.1f;
    const int stageOne = mining.crackStage();
    mining.elapsedSeconds = 0.99f;
    const int stageNine = mining.crackStage();
    mining.elapsedSeconds = 100.f;
    check("P11B/mining-crack-stages-are-bounded-and-monotonic",
          stageZero == 0 && stageOne == 1 && stageNine == 9 &&
              mining.crackStage() == 9);

    SandboxEventBus eventBus;
    ActionFeedbackTimeline feedback;
    feedback.attach(eventBus);
    eventBus.publish(BlockBreakEvent({1, 2, 3}, BlockId::Stone));
    ActionFeedbackSnapshot snapshot = feedback.snapshot();
    check("P11B/block-break-feedback-matches-material-and-bounds-particles",
          snapshot.kind == ActionFeedbackKind::BlockBreak &&
              snapshot.particles.size() ==
                  ActionFeedbackTimeline::FullBlockParticleCount &&
              std::all_of(snapshot.particles.begin(),
                          snapshot.particles.end(),
                          [](const ActionFeedbackParticle &particle)
                          {
                              return particle.materialId ==
                                  Material::ID::Stone;
                          }));

    for (int eventIndex = 0; eventIndex < 12; ++eventIndex)
    {
        eventBus.publish(BlockBreakEvent(
            {eventIndex, 2, 3}, BlockId::Dirt));
    }
    snapshot = feedback.snapshot();
    check("P11B/feedback-particle-cap-is-deterministic",
          snapshot.particles.size() ==
              ActionFeedbackTimeline::MaxParticles);
    feedback.update(0.25f);
    feedback.update(0.25f);
    feedback.update(0.25f);
    check("P11B/feedback-particles-expire-within-contract",
          feedback.snapshot().particles.empty());

    feedback.setIntensity(GameplayFeedbackIntensity::Reduced);
    eventBus.publish(BlockBreakEvent({1, 2, 3}, BlockId::Stone));
    snapshot = feedback.snapshot();
    check("P11B/reduced-feedback-halves-decorative-density",
          snapshot.particles.size() == 4 &&
              snapshot.recoil > 0.f && snapshot.recoil < 0.7f);

    feedback.setIntensity(GameplayFeedbackIntensity::Off);
    feedback.submitAttackMiss();
    snapshot = feedback.snapshot();
    check("P11B/off-keeps-essential-state-without-discomfort-effects",
          snapshot.kind == ActionFeedbackKind::AttackMiss &&
              snapshot.particles.empty() && snapshot.recoil == 0.f &&
              snapshot.hitStopSeconds == 0.f);

    feedback.setIntensity(GameplayFeedbackIntensity::Full);
    feedback.submitAttackMiss();
    const ActionFeedbackKind missKind = feedback.snapshot().kind;
    eventBus.publish(EntityDamageEvent(
        42, DefaultPlayerActorId, 3.f, 7.f, {}));
    const ActionFeedbackSnapshot hit = feedback.snapshot();
    eventBus.publish(CombatGuardEvent(
        DefaultPlayerActorId, 42, {}, CombatDirection::Front));
    const ActionFeedbackSnapshot guard = feedback.snapshot();
    eventBus.publish(EntityDamageEvent(
        DefaultPlayerActorId, 42, 2.f, 8.f, {}));
    const ActionFeedbackSnapshot hurt = feedback.snapshot();
    check("P11B/miss-hit-guard-and-hurt-have-distinct-feedback-states",
          missKind == ActionFeedbackKind::AttackMiss &&
              hit.kind == ActionFeedbackKind::AttackHit &&
              guard.kind == ActionFeedbackKind::Guard &&
              hurt.kind == ActionFeedbackKind::PlayerHurt &&
              hit.hitStopSeconds > 0.f &&
              hit.hitStopSeconds <=
                  ActionFeedbackTimeline::MaxHitStopSeconds);

    eventBus.publish(ItemPickupEvent(
        DefaultPlayerActorId, 91, Material::ID::IronIngot, 1, {}));
    snapshot = feedback.snapshot();
    check("P11B/item-pickup-feedback-is-bounded-and-material-matched",
          snapshot.kind == ActionFeedbackKind::ItemPickup &&
              snapshot.particles.size() == 3 &&
              snapshot.particles.front().materialId ==
                  Material::ID::IronIngot);

    const float gainA = ActionFeedbackTimeline::audioGainVariant(77);
    const float gainB = ActionFeedbackTimeline::audioGainVariant(77);
    check("P11B/audio-microvariation-is-deterministic-and-bounded",
          gainA == gainB && gainA >= 0.94f && gainA <= 1.06f);
}

void caseEventCommandQueryBoundary()
{
    const BlockBreakEvent domainEvent({1, 2, 3}, BlockId::Stone);
    check("AL-A4/current-events-are-immutable-domain-facts",
          domainEvent.category == SandboxEventCategory::Domain &&
              domainEvent.type == SandboxEventType::BlockBreak);

    SandboxEventBus observerBus;
    int observerCalls = 0;
    observerBus.subscribe(
        SandboxEventType::BlockBreak,
        [&observerCalls](const SandboxEvent &) { ++observerCalls; },
        SandboxEventSubscriptionOptions::observer("AL-A4.Observer"));
    const SandboxEventDispatchResult observerResult =
        observerBus.publish(domainEvent);
    check("AL-A4/observer-delivery-is-synchronous-and-declared",
          observerCalls == 1 && observerResult.publicationAccepted() &&
              observerResult.deliveredHandlers == 1 &&
              observerResult.rejectedHandlers == 0);

    SandboxEventBus forbiddenBus;
    int forbiddenNestedDeliveries = 0;
    SandboxEventDispatchResult forbiddenNestedResult;
    forbiddenBus.subscribe(
        SandboxEventType::BlockPlace,
        [&forbiddenNestedDeliveries](const SandboxEvent &) {
            ++forbiddenNestedDeliveries;
        });
    forbiddenBus.subscribe(
        SandboxEventType::BlockBreak,
        [&forbiddenBus, &forbiddenNestedResult](const SandboxEvent &) {
            forbiddenNestedResult = forbiddenBus.publish(
                BlockPlaceEvent({2, 2, 3}, BlockId::Dirt));
        },
        SandboxEventSubscriptionOptions::observer("AL-A4.NoRepublish"));
    forbiddenBus.publish(domainEvent);
    check("AL-A4/observer-cannot-hide-nested-publication",
          forbiddenNestedDeliveries == 0 &&
              forbiddenNestedResult.rejection ==
                  SandboxEventDispatchRejection::
                      HandlerRepublishForbidden);

    SandboxEventBus boundedBus;
    int boundedNestedDeliveries = 0;
    SandboxEventDispatchResult boundedNestedResult;
    boundedBus.subscribe(
        SandboxEventType::BlockPlace,
        [&boundedNestedDeliveries](const SandboxEvent &) {
            ++boundedNestedDeliveries;
        });
    boundedBus.subscribe(
        SandboxEventType::BlockBreak,
        [&boundedBus, &boundedNestedResult](const SandboxEvent &) {
            boundedNestedResult = boundedBus.publish(
                BlockPlaceEvent({2, 2, 3}, BlockId::Dirt));
        },
        SandboxEventSubscriptionOptions::domainMutation(
            "AL-A4.BoundedReaction",
            SandboxEventRepublishPolicy::Bounded));
    boundedBus.publish(domainEvent);
    check("AL-A4/declared-domain-reaction-may-publish-bounded-fact",
          boundedNestedDeliveries == 1 &&
              boundedNestedResult.publicationAccepted() &&
              boundedNestedResult.deliveredHandlers == 1);

    SandboxEventBus depthBus;
    int recursiveDeliveries = 0;
    SandboxEventDispatchResult depthLimitResult;
    depthBus.subscribe(
        SandboxEventType::BlockChanged,
        [&depthBus, &recursiveDeliveries,
         &depthLimitResult](const SandboxEvent &) {
            ++recursiveDeliveries;
            const SandboxEventDispatchResult nested = depthBus.publish(
                SandboxEvent(SandboxEventType::BlockChanged));
            if (!nested.publicationAccepted()) {
                depthLimitResult = nested;
            }
        },
        SandboxEventSubscriptionOptions::domainMutation(
            "AL-A4.DepthProbe", SandboxEventRepublishPolicy::Bounded));
    depthBus.publish(SandboxEvent(SandboxEventType::BlockChanged));
    const SandboxEventBusDebugSnapshot depthSnapshot =
        depthBus.debugSnapshot();
    check("AL-A4/recursive-publication-has-hard-depth-limit",
          recursiveDeliveries ==
                  static_cast<int>(SandboxEventBus::MaxDispatchDepth) &&
              depthLimitResult.rejection ==
                  SandboxEventDispatchRejection::DepthLimit &&
              depthSnapshot.maxObservedDispatchDepth ==
                  SandboxEventBus::MaxDispatchDepth &&
              depthSnapshot.currentDispatchDepth == 0 &&
              depthSnapshot.rejectedPublications == 1);

    SandboxEventBus diagnosticBus;
    int diagnosticObservers = 0;
    int diagnosticMutations = 0;
    diagnosticBus.subscribe(
        SandboxEventType::BlockBreak,
        [&diagnosticObservers](const SandboxEvent &) {
            ++diagnosticObservers;
        },
        SandboxEventSubscriptionOptions::observer(
            "AL-A4.DiagnosticObserver"));
    diagnosticBus.subscribe(
        SandboxEventType::BlockBreak,
        [&diagnosticMutations](const SandboxEvent &) {
            ++diagnosticMutations;
        },
        SandboxEventSubscriptionOptions::domainMutation(
            "AL-A4.DiagnosticMutation"));
    const SandboxEvent diagnosticEvent(
        SandboxEventType::BlockBreak, SandboxEventCategory::Diagnostic);
    const SandboxEventDispatchResult diagnosticResult =
        diagnosticBus.publish(diagnosticEvent);
    check("AL-A4/diagnostic-event-cannot-drive-domain-mutation",
          diagnosticObservers == 1 && diagnosticMutations == 0 &&
              diagnosticResult.deliveredHandlers == 1 &&
              diagnosticResult.rejectedHandlers == 1 &&
              diagnosticBus.debugSnapshot()
                      .rejectedDiagnosticHandlers == 1);

    SandboxEventBus membershipBus;
    int firstMembershipCalls = 0;
    int secondMembershipCalls = 0;
    SandboxEventBus::SubscriptionId secondMembershipId = 0;
    membershipBus.subscribe(
        SandboxEventType::BlockUse,
        [&membershipBus, &secondMembershipId,
         &firstMembershipCalls](const SandboxEvent &) {
            ++firstMembershipCalls;
            membershipBus.unsubscribe(secondMembershipId);
        });
    secondMembershipId = membershipBus.subscribe(
        SandboxEventType::BlockUse,
        [&secondMembershipCalls](const SandboxEvent &) {
            ++secondMembershipCalls;
        });
    const SandboxEventDispatchResult firstMembershipResult =
        membershipBus.publish(SandboxEvent(SandboxEventType::BlockUse));
    const SandboxEventDispatchResult secondMembershipResult =
        membershipBus.publish(SandboxEvent(SandboxEventType::BlockUse));
    check("AL-A4/subscription-membership-is-snapshotted-per-publication",
          firstMembershipCalls == 2 && secondMembershipCalls == 1 &&
              firstMembershipResult.deliveredHandlers == 2 &&
              secondMembershipResult.deliveredHandlers == 1);

    SandboxEventBus exceptionBus;
    exceptionBus.subscribe(
        SandboxEventType::ChunkSaved,
        [](const SandboxEvent &) { throw std::runtime_error("A4 probe"); });
    bool exceptionPropagated = false;
    try {
        exceptionBus.publish(SandboxEvent(SandboxEventType::ChunkSaved));
    }
    catch (const std::runtime_error &) {
        exceptionPropagated = true;
    }
    check("AL-A4/handler-exception-restores-dispatch-boundary",
          exceptionPropagated &&
              exceptionBus.debugSnapshot().currentDispatchDepth == 0);
}

void casePausedApplicationFlow()
{
    GameApplicationFlow flow;
    check("G4/main-menu-does-not-advance-simulation",
          !flow.acceptsWorldSimulation());
    check("G4/application-enters-playing",
          flow.showWorldList() && flow.beginLoading("pause-smoke") &&
              flow.completeLoading(true) &&
              flow.acceptsWorldSimulation());

    int simulatedTicks = 0;
    const auto pump = [&]() {
        if (flow.acceptsWorldSimulation()) {
            ++simulatedTicks;
        }
    };
    pump();
    check("G4/pause-transition-is-stateful",
          flow.pause() && flow.state() == GameApplicationState::Paused &&
              !flow.acceptsWorldSimulation());
    for (int frame = 0; frame < 10; ++frame) {
        pump();
    }
    check("G4/paused-frames-freeze-simulation", simulatedTicks == 1,
          std::to_string(simulatedTicks));
    check("G4/invalid-double-pause-rejected", !flow.pause());
    check("G4/resume-restores-simulation",
          flow.resume() && flow.acceptsWorldSimulation());
    pump();
    check("G4/resumed-frame-advances-simulation", simulatedTicks == 2,
          std::to_string(simulatedTicks));
}

// ---------------------------------------------------------------------------
// A4 - an integer seed in config reproduces a new world
// ---------------------------------------------------------------------------
void caseConfiguredWorldSeed()
{
    constexpr int configuredSeed = 20260811;
    const std::filesystem::path configDirectory =
        freshSaveDirectory("configured_seed_config");
    const std::filesystem::path configPath =
        configDirectory / "config.txt";
    {
        std::ofstream output(configPath,
                             std::ios::binary | std::ios::trunc);
        output << "seed " << configuredSeed << '\n';
    }

    Config config = loadRuntimeConfig(configPath.string());
    config.renderDistance = 1;
    check("A4/config-seed-loaded",
          config.worldSeed.has_value() &&
              *config.worldSeed == configuredSeed,
          config.worldSeed.has_value()
              ? std::to_string(*config.worldSeed)
              : "random");

    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    const std::string firstDirectory =
        freshSaveDirectory("configured_seed_first");
    const std::string secondDirectory =
        freshSaveDirectory("configured_seed_second");

    Camera firstCamera(config);
    Player firstPlayer;
    World first(firstCamera, config, firstPlayer, firstDirectory, false, 1);
    Camera secondCamera(config);
    Player secondPlayer;
    World second(secondCamera, config, secondPlayer, secondDirectory, false,
                 1);

    first.setRenderDistance(3);
    check("G4/live-render-distance-updates-world",
          first.getRenderDistance() == 3);

    const int firstSeed = first.collectDebugStats().terrainSeed;
    const int secondSeed = second.collectDebugStats().terrainSeed;
    check("A4/new-world-uses-config-seed",
          firstSeed == configuredSeed && secondSeed == configuredSeed,
          std::to_string(firstSeed) + "/" +
              std::to_string(secondSeed));

    std::size_t mismatches = 0;
    std::size_t samples = 0;
    for (int x = -8; x <= 23; x += 3) {
        for (int z = -8; z <= 23; z += 3) {
            for (int y = 0; y <= 127; y += 7) {
                ++samples;
                if (first.getBlock(x, y, z).id !=
                    second.getBlock(x, y, z).id) {
                    ++mismatches;
                }
            }
        }
    }
    check("A4/config-seed-reproduces-terrain", mismatches == 0,
          "mismatches=" + std::to_string(mismatches) + " over " +
              std::to_string(samples) + " samples");
    clearDeterministicEnv();
}

// ---------------------------------------------------------------------------
// A2 - malformed block definitions identify the source file and exact key
// ---------------------------------------------------------------------------
void caseBlockDataDiagnostics()
{
    const std::string directory =
        freshSaveDirectory("block_data_diagnostics");

    const auto makeBlockText = [](int id, const std::string &atlas,
                                  int meshType, int shaderType,
                                  bool includeShader) {
        std::ostringstream out;
        out << "Name\nFixture Block\n\n"
            << "Id\n" << id << "\n\n"
            << "TexAll\n" << atlas << "\n\n"
            << "Opaque\n1\n\n"
            << "MeshType\n" << meshType << "\n\n";
        if (includeShader) {
            out << "ShaderType\n" << shaderType << "\n\n";
        }
        out << "Light\n0\n\n"
            << "Collidable\n1\n";
        return out.str();
    };

    const auto fixturePath = [&](const std::string &name) {
        return std::filesystem::path(directory) / (name + ".block");
    };
    const auto writeFixture = [&](const std::string &name,
                                  const std::string &contents) {
        const std::filesystem::path path = fixturePath(name);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << contents;
        return path.string();
    };

    const auto parseError = [&](const std::string &name,
                                const std::string &contents) {
        writeFixture(name, contents);
        try {
            BlockData data(name, directory);
            (void)data;
        }
        catch (const std::runtime_error &error) {
            return std::string(error.what());
        }
        return std::string();
    };

    const std::string missingPath = fixturePath("MissingShader").string();
    const std::string missingError = parseError(
        "MissingShader", makeBlockText(3, "3 0", 0, 0, false));
    check("A2/missing-key-identifies-file-and-key",
          missingError.find(missingPath) != std::string::npos &&
              missingError.find("ShaderType") != std::string::npos &&
              missingError.find("missing") != std::string::npos,
          missingError);

    const std::string enumPath = fixturePath("BadEnum").string();
    const std::string enumError = parseError(
        "BadEnum", makeBlockText(3, "3 0", 9, 0, true));
    check("A2/bad-enum-identifies-file-and-key",
          enumError.find(enumPath) != std::string::npos &&
              enumError.find("MeshType") != std::string::npos &&
              enumError.find("invalid enum") != std::string::npos,
          enumError);

    const std::string atlasPath = fixturePath("BadAtlas").string();
    const std::string atlasError = parseError(
        "BadAtlas", makeBlockText(3, "16 0", 0, 0, true));
    check("A2/bad-atlas-identifies-file-and-key",
          atlasError.find(atlasPath) != std::string::npos &&
              atlasError.find("TexAll") != std::string::npos &&
              atlasError.find("outside [0, 15]") != std::string::npos,
          atlasError);

    std::string badLightText = makeBlockText(3, "3 0", 0, 0, true);
    const std::size_t lightValue = badLightText.find("Light\n0");
    badLightText.replace(lightValue, std::string("Light\n0").size(),
                         "Light\n16");
    const std::string lightPath = fixturePath("BadLight").string();
    const std::string lightError = parseError("BadLight", badLightText);
    check("A2/bad-light-identifies-file-and-key",
          lightError.find(lightPath) != std::string::npos &&
              lightError.find("Light") != std::string::npos &&
              lightError.find("outside [0, 15]") != std::string::npos,
          lightError);

    writeFixture("DuplicateOne",
                 makeBlockText(3, "3 0", 0, 0, true));
    const std::string duplicatePath = writeFixture(
        "DuplicateTwo", makeBlockText(3, "3 0", 0, 0, true));
    BlockData duplicateOne("DuplicateOne", directory);
    BlockData duplicateTwo("DuplicateTwo", directory);
    BlockIdUniquenessValidator validator;
    std::string duplicateError;
    try {
        validator.add(duplicateOne.getBlockData().id,
                      duplicateOne.getSourcePath());
        validator.add(duplicateTwo.getBlockData().id,
                      duplicateTwo.getSourcePath());
    }
    catch (const std::runtime_error &error) {
        duplicateError = error.what();
    }
    check("A2/duplicate-id-identifies-file-and-key",
          duplicateError.find(duplicatePath) != std::string::npos &&
              duplicateError.find("Id") != std::string::npos &&
              duplicateError.find("duplicates value 3") !=
                  std::string::npos,
          duplicateError);
}

// ---------------------------------------------------------------------------
// P4 - ore definitions point at distinct, populated atlas tiles
// ---------------------------------------------------------------------------
void caseOreTextures()
{
    const auto &database = BlockDatabase::get();
    const auto &stone = database.getDefinition(BlockId::Stone);
    const auto &coal = database.getDefinition(BlockId::CoalOre);
    const auto &iron = database.getDefinition(BlockId::IronOre);

    check("P4/ore-texture-coordinates",
          coal.render.texTopCoord == glm::ivec2(13, 0) &&
              iron.render.texTopCoord == glm::ivec2(14, 0) &&
              coal.render.texTopCoord != stone.render.texTopCoord &&
              iron.render.texTopCoord != stone.render.texTopCoord);

    const std::string atlasPath =
        ResourcePaths::media("textures/DefaultPack.png");
    FREE_IMAGE_FORMAT format = FreeImage_GetFileType(atlasPath.c_str(), 0);
    if (format == FIF_UNKNOWN) {
        format = FreeImage_GetFIFFromFilename(atlasPath.c_str());
    }
    FIBITMAP *source = format != FIF_UNKNOWN
                           ? FreeImage_Load(format, atlasPath.c_str())
                           : nullptr;
    FIBITMAP *atlas =
        source != nullptr ? FreeImage_ConvertTo32Bits(source) : nullptr;
    const unsigned int atlasWidth =
        atlas != nullptr ? FreeImage_GetWidth(atlas) : 0;
    const unsigned int atlasHeight =
        atlas != nullptr ? FreeImage_GetHeight(atlas) : 0;
    const bool loaded = atlas != nullptr && atlasWidth == 256 &&
                        atlasHeight == 256;
    check("P4/atlas-loads", loaded,
          atlasPath + " format=" + std::to_string(static_cast<int>(format)) +
              " size=" + std::to_string(atlasWidth) + "x" +
              std::to_string(atlasHeight));
    if (!loaded) {
        if (atlas != nullptr) {
            FreeImage_Unload(atlas);
        }
        if (source != nullptr) {
            FreeImage_Unload(source);
        }
        return;
    }

    const auto hashTile = [&](int tileX, int tileY) {
        std::uint64_t hash = 1469598103934665603ull;
        for (int y = 0; y < 16; ++y) {
            const int sourceY = tileY * 16 + y;
            const BYTE *scanline = FreeImage_GetScanLine(
                atlas, static_cast<int>(atlasHeight) - sourceY - 1);
            for (int x = 0; x < 16; ++x) {
                const std::size_t offset =
                    static_cast<std::size_t>((tileX * 16 + x) * 4);
                for (int channel = 0; channel < 4; ++channel) {
                    hash ^= scanline[offset + channel];
                    hash *= 1099511628211ull;
                }
            }
        }
        return hash;
    };
    const auto visiblePixelCount = [&](int tileX, int tileY) {
        int visible = 0;
        for (int y = 0; y < 16; ++y) {
            const int sourceY = tileY * 16 + y;
            const BYTE *scanline = FreeImage_GetScanLine(
                atlas, static_cast<int>(atlasHeight) - sourceY - 1);
            for (int x = 0; x < 16; ++x) {
                const std::size_t offset =
                    static_cast<std::size_t>((tileX * 16 + x) * 4);
                if (scanline[offset + 3] != 0) {
                    ++visible;
                }
            }
        }
        return visible;
    };

    const auto stoneHash = hashTile(3, 0);
    const auto coalHash = hashTile(13, 0);
    const auto ironHash = hashTile(14, 0);
    check("P4/coal-texture-distinct", coalHash != stoneHash);
    check("P4/iron-texture-distinct",
          ironHash != stoneHash && ironHash != coalHash);

    bool iconCatalogueCovered =
        !Material::iconCoordinate(Material::ID::Nothing).available();
    for (int id = static_cast<int>(Material::ID::Grass);
         id < static_cast<int>(Material::ID::Count); ++id) {
        iconCatalogueCovered = iconCatalogueCovered &&
            Material::iconCoordinate(static_cast<Material::ID>(id))
                .available();
    }
    check("FS3/material-icon-map-covers-catalogue",
          iconCatalogueCovered);

    const auto &chest = database.getDefinition(BlockId::Chest);
    const auto &workbench = database.getDefinition(BlockId::Workbench);
    const auto &furnace = database.getDefinition(BlockId::Furnace);
    const auto &torch = database.getDefinition(BlockId::Torch);
    const bool interactiveCoordinates =
        chest.render.texTopCoord == glm::ivec2(0, 1) &&
        workbench.render.texTopCoord == glm::ivec2(1, 1) &&
        furnace.render.texTopCoord == glm::ivec2(2, 1) &&
        torch.render.texTopCoord == glm::ivec2(6, 1);
    const std::set<std::uint64_t> interactiveHashes = {
        hashTile(0, 1), hashTile(1, 1), hashTile(2, 1)};
    check("FS3/interactive-blocks-use-dedicated-tiles",
          interactiveCoordinates && interactiveHashes.size() == 3 &&
              visiblePixelCount(0, 1) > 24 &&
              visiblePixelCount(1, 1) > 24 &&
              visiblePixelCount(2, 1) > 24);
    check("P11-0/torch-tile-is-visible-cutout",
          visiblePixelCount(6, 1) > 8 &&
              visiblePixelCount(6, 1) < 128);
    check("P11-1/building-and-tool-tiles-are-visible-and-distinct",
          hashTile(7, 1) != stoneHash &&
              hashTile(8, 1) != hashTile(5, 1) &&
              visiblePixelCount(7, 1) == 256 &&
              visiblePixelCount(8, 1) == 256 &&
              visiblePixelCount(15, 2) > 12 &&
              visiblePixelCount(15, 3) > 12 &&
              hashTile(15, 2) != hashTile(15, 3));

    std::set<std::uint64_t> itemHashes;
    bool itemTilesPopulated = true;
    for (int tileX = 0; tileX < 15; ++tileX) {
        itemHashes.insert(hashTile(tileX, 2));
        itemTilesPopulated = itemTilesPopulated &&
                             visiblePixelCount(tileX, 2) > 12;
    }
    check("FS3/item-icons-are-populated-and-distinct",
          itemTilesPopulated && itemHashes.size() == 15,
          "visible=" + std::to_string(itemTilesPopulated ? 1 : 0) +
              " unique=" + std::to_string(itemHashes.size()));
    FreeImage_Unload(atlas);
    FreeImage_Unload(source);
}

// ---------------------------------------------------------------------------
// P3 - block picking provides one result for rendering and interaction
// ---------------------------------------------------------------------------
void caseBlockSelection()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");

    const auto directory = freshSaveDirectory("block_selection");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    world.setBlock(8, 200, 6, ChunkBlock(BlockId::Water));
    world.setBlock(8, 200, 5, ChunkBlock(BlockId::Stone));

    const auto selection = BlockSelectionSystem::pick(
        world, glm::vec3(8.5f, 200.5f, 8.5f), glm::vec3(0.f));
    check("P3/selection-hits-solid",
          selection.has_value() &&
              selection->blockPosition == glm::ivec3(8, 200, 5) &&
              selection->blockId == BlockId::Stone);
    check("P3/selection-placement-adjacent",
          selection.has_value() &&
              selection->placementPosition == glm::ivec3(8, 200, 6));
    check("P3/selection-skips-water",
          selection.has_value() && selection->hitPoint.z < 6.f);

    const auto shortRange = BlockSelectionSystem::pick(
        world, glm::vec3(8.5f, 200.5f, 8.5f), glm::vec3(0.f), 2.f);
    check("P3/selection-respects-range", !shortRange.has_value());

    world.setBlock(8, 200, 5, ChunkBlock(BlockId::Air));
    const auto waterOnly = BlockSelectionSystem::pick(
        world, glm::vec3(8.5f, 200.5f, 8.5f), glm::vec3(0.f));
    check("P3/selection-misses-with-only-water", !waterOnly.has_value());

    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}

// ---------------------------------------------------------------------------
// V2 - player controls accept deterministic synthetic input
// ---------------------------------------------------------------------------
void casePlayerControllerInput()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");

    const auto directory = freshSaveDirectory("player_controller");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    PlayerController controller;

    player.position = {8.f, 200.f, 8.f};
    player.rotation = {0.f, 0.f, 0.f};
    player.velocity = {0.f, 0.f, 0.f};

    PlayerInputState input;
    input.moveForward = true;
    input.jump = true;
    input.toggleFlying = true;
    input.hotbarSlot = 3;
    input.lookDelta = {1.f, -0.5f};
    controller.applyInput(player, input);

    check("V2/fly-toggle", player.isFlying());
    check("V2/hotbar-selection", player.getSaveState().heldItem == 3,
          "selected=" + std::to_string(player.getSaveState().heldItem));
    check("V2/look-input",
          std::abs(player.rotation.x + 0.5f) < 0.001f &&
              std::abs(player.rotation.y - 1.f) < 0.001f,
          vecToString(player.rotation));

    const auto beforeUpdate = player.position;
    player.update(0.05f, world);
    check("V2/movement-input", player.position.z < beforeUpdate.z,
          vecToString(beforeUpdate) + " -> " + vecToString(player.position));
    check("V2/jump-input", player.position.y > beforeUpdate.y,
          vecToString(beforeUpdate) + " -> " + vecToString(player.position));

    PlayerInputState toggleOff;
    toggleOff.toggleFlying = true;
    controller.applyInput(player, toggleOff);
    check("V2/fly-toggle-off", !player.isFlying());

    PlayerInputState sneak;
    sneak.descend = true;
    player.applyInput(sneak);
    check("V2/sneak-is-held", player.isSneaking());
    player.applyInput(PlayerInputState());
    check("V2/sneak-releases", !player.isSneaking());

    Player sampledOnce;
    Player sampledRepeatedly;
    sampledOnce.position = {20.f, 220.f, 20.f};
    sampledRepeatedly.position = sampledOnce.position;
    PlayerInputState enableFlight;
    enableFlight.toggleFlying = true;
    sampledOnce.applyInput(enableFlight);
    sampledRepeatedly.applyInput(enableFlight);
    PlayerInputState heldForward;
    heldForward.moveForward = true;
    sampledOnce.applyInput(heldForward);
    for (int frame = 0; frame < 8; ++frame) {
        sampledRepeatedly.applyInput(heldForward);
    }
    sampledOnce.update(0.05f, world);
    sampledRepeatedly.update(0.05f, world);
    check("V2/repeated-frame-sampling-is-idempotent",
          glm::length(sampledOnce.position -
                      sampledRepeatedly.position) < 0.0001f,
          vecToString(sampledOnce.position) + " / " +
              vecToString(sampledRepeatedly.position));

    const glm::vec3 interpolationMidpoint =
        sampledOnce.getInterpolatedPosition(0.5f);
    check("V2/player-position-interpolates-between-ticks",
          glm::length(interpolationMidpoint -
                      (glm::vec3(20.f, 220.f, 20.f) +
                       sampledOnce.position) * 0.5f) < 0.0001f,
          vecToString(interpolationMidpoint));

    Player diagonal;
    diagonal.position = {24.f, 220.f, 24.f};
    diagonal.applyInput(enableFlight);
    PlayerInputState heldDiagonal;
    heldDiagonal.moveForward = true;
    heldDiagonal.moveRight = true;
    diagonal.applyInput(heldDiagonal);
    diagonal.update(0.05f, world);
    const float straightDistance = glm::length(
        glm::vec2(sampledOnce.position.x - 20.f,
                  sampledOnce.position.z - 20.f));
    const float diagonalDistance = glm::length(
        glm::vec2(diagonal.position.x - 24.f,
                  diagonal.position.z - 24.f));
    check("V2/diagonal-speed-is-normalized",
          std::abs(straightDistance - diagonalDistance) < 0.0001f,
          std::to_string(straightDistance) + " / " +
              std::to_string(diagonalDistance));

    Player walkRight;
    Player sprintRight;
    walkRight.position = {28.f, 220.f, 28.f};
    sprintRight.position = {32.f, 220.f, 32.f};
    walkRight.applyInput(enableFlight);
    sprintRight.applyInput(enableFlight);
    PlayerInputState strafe;
    strafe.moveRight = true;
    walkRight.applyInput(strafe);
    strafe.sprint = true;
    sprintRight.applyInput(strafe);
    for (int tick = 0; tick < 8; ++tick) {
        walkRight.update(0.05f, world);
        sprintRight.update(0.05f, world);
    }
    check("V2/sprint-applies-to-strafe",
          sprintRight.position.x - 32.f >
              walkRight.position.x - 28.f + 0.3f);

    Player focusRecovery;
    focusRecovery.position = {36.f, 220.f, 36.f};
    focusRecovery.applyInput(enableFlight);
    focusRecovery.applyInput(heldForward);
    focusRecovery.update(0.05f, world);
    const bool movedBeforeNeutral =
        glm::length(glm::vec2(focusRecovery.velocity.x,
                              focusRecovery.velocity.z)) > 0.001f;
    focusRecovery.applyInput(PlayerInputState());
    for (int tick = 0; tick < 20; ++tick) {
        focusRecovery.update(0.05f, world);
    }
    const glm::vec3 stoppedPosition = focusRecovery.position;
    focusRecovery.update(0.05f, world);
    check("R3A/focus-neutral-input-stops-held-movement",
          movedBeforeNeutral &&
              glm::length(glm::vec2(focusRecovery.velocity.x,
                                    focusRecovery.velocity.z)) < 0.0001f &&
              glm::length(focusRecovery.position - stoppedPosition) <
                  0.0001f);

    Player opposedDirections;
    opposedDirections.position = {40.f, 220.f, 40.f};
    opposedDirections.applyInput(enableFlight);
    PlayerInputState opposedInput;
    opposedInput.moveForward = true;
    opposedInput.moveBackward = true;
    opposedInput.moveLeft = true;
    opposedInput.moveRight = true;
    opposedDirections.applyInput(opposedInput);
    opposedDirections.update(0.05f, world);
    check("R3A/opposed-direction-state-is-neutral",
          glm::length(opposedDirections.position -
                      glm::vec3(40.f, 220.f, 40.f)) < 0.0001f);

    Player frameLocalLook;
    PlayerInputState lookOnce;
    lookOnce.lookDelta = {7.f, -3.f};
    frameLocalLook.applyInput(lookOnce);
    const glm::vec3 rotationAfterLook = frameLocalLook.rotation;
    frameLocalLook.applyInput(PlayerInputState());
    check("R3A/mouse-look-delta-is-frame-local",
          glm::length(frameLocalLook.rotation - rotationAfterLook) <
              0.0001f);

    Player wheelSelection;
    PlayerInputState previousSlot;
    previousSlot.hotbarDelta = -1;
    wheelSelection.applyInput(previousSlot);
    const int lastSlot = wheelSelection.getInventorySlotCount() - 1;
    const bool wrappedBackward =
        wheelSelection.getSaveState().heldItem == lastSlot;
    PlayerInputState nextSlot;
    nextSlot.hotbarDelta = 1;
    wheelSelection.applyInput(nextSlot);
    check("R3A/hotbar-wheel-wraps-both-directions",
          wrappedBackward && wheelSelection.getSaveState().heldItem == 0);

    Player verticalFlight;
    verticalFlight.position = {44.f, 220.f, 44.f};
    verticalFlight.applyInput(enableFlight);
    PlayerInputState rise;
    rise.jump = true;
    verticalFlight.applyInput(rise);
    for (int tick = 0; tick < 4; ++tick) {
        verticalFlight.update(0.05f, world);
    }
    const float heightAfterRise = verticalFlight.position.y;
    PlayerInputState descend;
    descend.descend = true;
    verticalFlight.applyInput(descend);
    for (int tick = 0; tick < 8; ++tick) {
        verticalFlight.update(0.05f, world);
    }
    check("R3A/flight-rise-descend-uses-held-state",
          heightAfterRise > 220.f &&
              verticalFlight.position.y < heightAfterRise &&
              !verticalFlight.isSneaking());
}

// ---------------------------------------------------------------------------
// V2 - player collision sweeps across every crossed block cell
// ---------------------------------------------------------------------------
void casePlayerSweptCollision()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 120 8");

    const auto directory = freshSaveDirectory("player_swept_collision");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int floorY = 100;
    world.setBlock(8, floorY, 8, BlockId::Stone);
    player.position = {8.5f, 110.f, 8.5f};
    player.velocity = {0.f, -200.f, 0.f};
    player.box.update(player.position);

    player.update(0.05f, world);
    const float expectedRestingY =
        static_cast<float>(floorY + 1) + player.box.dimensions.y;
    check("V2/high-speed-fall-stops-on-floor",
          std::abs(player.position.y - expectedRestingY) < 0.001f &&
              std::abs(player.velocity.y) < 0.001f,
          vecToString(player.position));

    player.update(0.05f, world);
    check("V2/resting-contact-remains-grounded",
          std::abs(player.position.y - expectedRestingY) < 0.001f &&
              std::abs(player.velocity.y) < 0.001f,
          vecToString(player.position));

    PlayerInputState jump;
    jump.jump = true;
    player.applyInput(jump);
    player.update(0.05f, world);
    check("V2/jump-after-resting-contact",
          player.position.y > expectedRestingY,
          vecToString(player.position));

    clearDeterministicEnv();
}

int scanHighestOpaqueBlock(const Chunk &chunk, int x, int z)
{
    const int highestPossible =
        static_cast<int>(chunk.getSectionCount() * CHUNK_SIZE) - 1;
    for (int y = highestPossible; y >= 0; --y) {
        if (chunk.getBlock(x, y, z).getData().isOpaque) {
            return y;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// V4 - the cached height map follows edits to the highest opaque block
// ---------------------------------------------------------------------------
void caseHeightMapEdits()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");

    const auto directory = freshSaveDirectory("height_map_edits");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    if (chunk == nullptr) {
        check("V4/chunk-available", false);
        return;
    }

    constexpr int x = 8;
    constexpr int z = 8;
    const int initialHeight = chunk->getHeightAt(x, z);
    const int initialScan = scanHighestOpaqueBlock(*chunk, x, z);
    check("V4/generated-height-matches-scan", initialHeight == initialScan,
          "cached=" + std::to_string(initialHeight) +
              " scanned=" + std::to_string(initialScan));

    chunk->setBlock(x, initialHeight, z, BlockId::Air);
    const int heightAfterBreak = chunk->getHeightAt(x, z);
    const int scanAfterBreak = scanHighestOpaqueBlock(*chunk, x, z);
    check("V4/break-highest-updates-height",
          heightAfterBreak < initialHeight &&
              heightAfterBreak == scanAfterBreak,
          "cached=" + std::to_string(heightAfterBreak) +
              " scanned=" + std::to_string(scanAfterBreak));

    const int placedHeight = initialHeight + 5;
    chunk->setBlock(x, placedHeight, z, BlockId::Stone);
    const int heightAfterPlace = chunk->getHeightAt(x, z);
    const int scanAfterPlace = scanHighestOpaqueBlock(*chunk, x, z);
    check("V4/place-above-updates-height",
          heightAfterPlace == placedHeight &&
              heightAfterPlace == scanAfterPlace,
          "cached=" + std::to_string(heightAfterPlace) +
              " scanned=" + std::to_string(scanAfterPlace));
}

// ---------------------------------------------------------------------------
// V5 - background loading survives sustained load-center churn
// ---------------------------------------------------------------------------
void caseBackgroundLoaderStress()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");

    const auto directory = freshSaveDirectory("background_loader_stress");
    Config config = makeConfig();
    config.renderDistance = 1;
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, true, 0);

    const std::array<VectorXZ, 9> centers = {
        VectorXZ{0, 0},   VectorXZ{4, 0},   VectorXZ{4, 4},
        VectorXZ{0, 4},   VectorXZ{-4, 4},  VectorXZ{-4, 0},
        VectorXZ{-4, -4}, VectorXZ{0, -4},  VectorXZ{4, -4},
    };

    constexpr int iterations = 240;
    bool statsConsistent = true;
    bool blockReadsValid = true;
    std::size_t maximumExistingChunks = 0;

    for (int iteration = 0; iteration < iterations; ++iteration) {
        const VectorXZ center = centers[static_cast<std::size_t>(iteration) %
                                        centers.size()];
        camera.position = {
            static_cast<float>(center.x * CHUNK_SIZE + CHUNK_SIZE / 2),
            90.f,
            static_cast<float>(center.z * CHUNK_SIZE + CHUNK_SIZE / 2),
        };
        world.update(camera);

        const auto block = world.getBlock(
            World::toBlockCoord(camera.position.x), 64,
            World::toBlockCoord(camera.position.z));
        blockReadsValid =
            blockReadsValid &&
            block.id < static_cast<Block_t>(BlockId::NUM_TYPES);

        const WorldDebugStats stats = world.collectDebugStats();
        const std::size_t classifiedSections =
            stats.chunks.meshCleanSections +
            stats.chunks.meshDirtySections +
            stats.chunks.meshQueuedSections +
            stats.chunks.meshBuildingSections +
            stats.chunks.cpuReadySections;
        statsConsistent =
            statsConsistent &&
            stats.chunks.loadedChunks <= stats.chunks.existingChunks &&
            classifiedSections == stats.chunks.sections;
        maximumExistingChunks =
            std::max(maximumExistingChunks, stats.chunks.existingChunks);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Hold one final center long enough to prove that churn did not leave the
    // worker stuck rebuilding obsolete queues forever.
    const std::size_t rebuildsBeforeFinalCenter =
        world.collectDebugStats().chunks.meshRebuilds;
    camera.position = {
        static_cast<float>(8 * CHUNK_SIZE + CHUNK_SIZE / 2), 90.f,
        static_cast<float>(8 * CHUNK_SIZE + CHUNK_SIZE / 2)};
    // Generation competes with antivirus, symbol and build I/O on the Windows
    // gate. Keep the assertion about eventual progress, but do not turn a
    // transiently busy host into a five-second false negative.
    const auto progressDeadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(15);
    WorldDebugStats finalStats;
    do {
        world.update(camera);
        finalStats = world.collectDebugStats();
        if (finalStats.chunks.meshRebuilds > rebuildsBeforeFinalCenter) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < progressDeadline);

    check("V5/load-center-churn-completes", statsConsistent,
          "iterations=" + std::to_string(iterations));
    check("V5/concurrent-block-reads-valid", blockReadsValid);
    check("V5/background-loader-makes-progress",
          finalStats.chunks.meshRebuilds > rebuildsBeforeFinalCenter,
          "chunks=" + std::to_string(maximumExistingChunks) +
              " mesh_rebuilds=" +
              std::to_string(rebuildsBeforeFinalCenter) + " -> " +
              std::to_string(finalStats.chunks.meshRebuilds));
}

// ---------------------------------------------------------------------------
// S0.6 - spawn preload uses chunk coordinates
// ---------------------------------------------------------------------------
void caseSpawnPreload()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));

    const auto directory = freshSaveDirectory("spawn_preload");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    const auto spawnChunk =
        World::getChunkXZ(World::toBlockCoord(player.position.x),
                          World::toBlockCoord(player.position.z));

    auto &chunks = world.getChunkManager();
    int loaded = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            if (chunks.chunkLoadedAt(spawnChunk.x + dx, spawnChunk.z + dz)) {
                ++loaded;
            }
        }
    }

    check("S0.6/spawn-preload-3x3", loaded == 9,
          "loaded " + std::to_string(loaded) + "/9 around chunk (" +
              std::to_string(spawnChunk.x) + "," +
              std::to_string(spawnChunk.z) + ")");

    const int spawnHeight = World::toBlockCoord(player.position.y);
    check("S0.6/spawn-above-terrain", spawnHeight > 0,
          "spawn " + vecToString(player.position));

    // Player position is above the floor surface: ground is at Y-2 while
    // Y-1 and Y must remain clear so the player cannot start inside terrain.
    const int spawnX = World::toBlockCoord(player.position.x);
    const int spawnZ = World::toBlockCoord(player.position.z);
    const auto ground = world.getBlock(spawnX, spawnHeight - 2, spawnZ);
    check("S0.6/spawn-on-solid-ground", ground.id != 0,
          "block under spawn id=" + std::to_string(static_cast<int>(ground.id)));
    check("S0.6/spawn-has-two-block-clearance",
          world.getBlock(spawnX, spawnHeight - 1, spawnZ).id == 0 &&
              world.getBlock(spawnX, spawnHeight, spawnZ).id == 0);
}

// ---------------------------------------------------------------------------
// FS1 - managed worlds initialize a deterministic, playable first spawn
// ---------------------------------------------------------------------------
void caseManagedWorldFirstSpawn()
{
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");

    const int suggestedSeedA = WorldManagementService::suggestWorldSeed();
    const int suggestedSeedB = WorldManagementService::suggestWorldSeed();
    check("FS1/world-seed-suggestions-are-nonzero",
          suggestedSeedA > 0 && suggestedSeedB > 0,
          std::to_string(suggestedSeedA) + "," +
              std::to_string(suggestedSeedB));
    check("FS1/world-seed-suggestions-refresh",
          suggestedSeedA != suggestedSeedB,
          std::to_string(suggestedSeedA) + " -> " +
              std::to_string(suggestedSeedB));

    const auto oakCountInLoadedChunks = [](World &world) {
        int count = 0;
        for (const auto &entry : world.getChunkManager().getChunks()) {
            const Chunk &chunk = entry.second;
            if (!chunk.hasLoaded()) {
                continue;
            }
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const int highest = chunk.getHeightAt(x, z);
                    for (int y = highest; y >= std::max(0, highest - 12);
                         --y) {
                        if (chunk.getBlock(x, y, z) == BlockId::OakBark) {
                            ++count;
                        }
                    }
                }
            }
        }
        return count;
    };

    const auto validateManagedSeed = [&](const std::string &name, int seed) {
        const std::string catalogueRoot =
            freshSaveDirectory("fs1_" + name);
        const WorldManagementService management(catalogueRoot);
        const WorldManagementResult created =
            management.createWorld("FS1 " + name, seed);
        const WorldManagementResult opened =
            created.succeeded()
                ? management.prepareWorldForOpen(created.worldId)
                : WorldManagementResult{};

        WorldSaveData initial;
        const bool initialLoaded = opened.succeeded() &&
            WorldSave::loadFromPath(
                (std::filesystem::path(opened.directoryPath) /
                 "world.meta").string(),
                initial);
        const glm::vec3 placeholder = initialWorldSpawnPlaceholder();
        check("FS1/" + name + "/managed-world-starts-uninitialized",
              initialLoaded && !initial.hasPlayerState &&
                  glm::length(initial.spawnPoint - placeholder) < 0.01f);
        if (!initialLoaded) {
            return glm::vec3(0.f);
        }

        Config config = makeConfig();
        Camera camera(config);
        glm::vec3 firstSpawn{0.f};
        {
            Player player;
            World world(camera, config, player, opened.directoryPath,
                        false, 1);
            firstSpawn = world.getPlayerSpawnPoint();
            const int blockX = World::toBlockCoord(firstSpawn.x);
            const int blockY = World::toBlockCoord(firstSpawn.y);
            const int blockZ = World::toBlockCoord(firstSpawn.z);
            const ChunkBlock ground =
                world.getBlock(blockX, blockY - 2, blockZ);
            const ChunkBlock body =
                world.getBlock(blockX, blockY - 1, blockZ);
            const ChunkBlock head =
                world.getBlock(blockX, blockY, blockZ);
            const TerrainBiome biome =
                world.getChunkManager()
                    .getTerrainGenerator()
                    .getBiomeAtWorld(blockX, blockZ);

            check("FS1/" + name + "/spawn-replaces-placeholder",
                  glm::length(firstSpawn - placeholder) > 1.f,
                  vecToString(firstSpawn));
            check("FS1/" + name + "/player-starts-at-spawn",
                  glm::length(player.position - firstSpawn) < 0.01f,
                  vecToString(player.position));
            check("FS1/" + name + "/spawn-has-solid-floor",
                  ground.getData().isCollidable &&
                      static_cast<BlockId>(ground.id) != BlockId::Water,
                  "block=" +
                      std::to_string(static_cast<int>(ground.id)));
            check("FS1/" + name + "/spawn-has-two-block-clearance",
                  body == BlockId::Air && head == BlockId::Air);
            check("FS1/" + name + "/spawn-is-not-ocean",
                  biome != TerrainBiome::Ocean);
            check("FS1/" + name + "/spawn-neighborhood-has-oak",
                  oakCountInLoadedChunks(world) > 0,
                  "spawn=" + vecToString(firstSpawn));
            check("FS1/" + name + "/initialized-world-saves",
                  world.save());
        }

        WorldSaveData persisted;
        const bool persistedLoaded = WorldSave::loadFromPath(
            (std::filesystem::path(opened.directoryPath) / "world.meta")
                .string(),
            persisted);
        check("FS1/" + name + "/first-entry-is-persisted",
              persistedLoaded && persisted.hasPlayerState &&
                  glm::length(persisted.spawnPoint - firstSpawn) < 0.01f &&
                  glm::length(persisted.playerState.position - firstSpawn) <
                      0.01f);

        Player reloadedPlayer;
        World reloaded(camera, config, reloadedPlayer,
                       opened.directoryPath, false, 1);
        check("FS1/" + name + "/reopen-keeps-spawn",
              glm::length(reloaded.getPlayerSpawnPoint() - firstSpawn) <
                      0.01f &&
                  glm::length(reloadedPlayer.position - firstSpawn) < 0.01f,
              vecToString(reloadedPlayer.position));
        return firstSpawn;
    };

    const glm::vec3 seedZeroFirst = validateManagedSeed("seed_zero_a", 0);
    const glm::vec3 seedZeroSecond = validateManagedSeed("seed_zero_b", 0);
    check("FS1/seed-zero-spawn-is-deterministic",
          glm::length(seedZeroFirst - seedZeroSecond) < 0.01f,
          vecToString(seedZeroFirst) + " / " +
              vecToString(seedZeroSecond));
    validateManagedSeed("fixed_seed", kValidationSeed);

    const std::string rescueDirectory =
        freshSaveDirectory("fs1_placeholder_rescue");
    WorldSaveData rescue;
    rescue.worldId = "fs1-rescue";
    rescue.worldName = "FS1 Rescue";
    rescue.seed = 0;
    rescue.terrainGenerationVersion = LegacyTerrainGenerationVersion;
    rescue.createdUtc = 1787222508;
    rescue.lastPlayedUtc = rescue.createdUtc;
    rescue.spawnPoint = initialWorldSpawnPlaceholder();
    rescue.worldTime = 291.f;
    rescue.hasPlayerState = true;
    rescue.playerState.position = {0.5f, 66.f, 0.5f};
    rescue.playerState.inventory.resize(5);
    rescue.actors.push_back(
        {ActorSaveKind::Mob, 2, World::BruteMobType,
         {13.5f, 65.f, 1.f}});
    ActorSaveState droppedItem;
    droppedItem.kind = ActorSaveKind::Item;
    droppedItem.id = 3;
    droppedItem.type = "item";
    droppedItem.position = {1.5f, 68.f, 1.5f};
    droppedItem.materialId = static_cast<int>(Material::ID::Stone);
    droppedItem.amount = 2;
    rescue.actors.push_back(droppedItem);
    check("FS1/placeholder-rescue-fixture-saves",
          WorldSave(rescueDirectory).save(rescue));
    {
        Config config = makeConfig();
        Camera camera(config);
        Player player;
        World world(camera, config, player, rescueDirectory, false, 1);
        check("FS1/short-empty-placeholder-is-rescued",
              glm::length(world.getPlayerSpawnPoint() -
                          initialWorldSpawnPlaceholder()) > 1.f &&
                  glm::length(player.position -
                              world.getPlayerSpawnPoint()) < 0.01f,
              vecToString(player.position));
        WorldSaveData repaired;
        const bool repairedLoaded = WorldSave::loadFromPath(
            (std::filesystem::path(rescueDirectory) / "world.meta").string(),
            repaired);
        check("FS1/rescue-preserves-world-identity",
              repairedLoaded && repaired.seed == rescue.seed &&
                  repaired.terrainGenerationVersion ==
                      rescue.terrainGenerationVersion);
        check("FS1/rescue-removes-placeholder-natural-actors",
              repairedLoaded && repaired.actors.size() == 1u &&
                  repaired.actors.front().kind == ActorSaveKind::Item);
        check("FS1/rescue-preserves-non-natural-actors",
              repairedLoaded && repaired.actors.size() == 1u &&
                  repaired.actors.front().id == droppedItem.id &&
                  repaired.actors.front().materialId ==
                      droppedItem.materialId &&
                  repaired.actors.front().amount == droppedItem.amount);
    }

    const std::string protectedDirectory =
        freshSaveDirectory("fs1_progressed_placeholder");
    WorldSaveData protectedWorld = rescue;
    protectedWorld.worldId = "fs1-protected";
    protectedWorld.worldName = "FS1 Protected";
    protectedWorld.playerState.position = {1.5f, 66.f, 0.5f};
    protectedWorld.playerState.inventory[0] =
        {Material::ID::OakBark, 1, 0};
    check("FS1/progressed-placeholder-fixture-saves",
          WorldSave(protectedDirectory).save(protectedWorld));
    {
        Config config = makeConfig();
        Camera camera(config);
        Player player;
        World world(camera, config, player, protectedDirectory, false, 1);
        check("FS1/progressed-world-is-not-relocated",
              glm::length(world.getPlayerSpawnPoint() -
                          initialWorldSpawnPlaceholder()) < 0.01f &&
                  glm::length(player.position -
                              protectedWorld.playerState.position) < 0.01f,
              vecToString(player.position));
    }
}

// ---------------------------------------------------------------------------
// S0.1 - negative and cross-zero block/chunk mapping
// ---------------------------------------------------------------------------
void caseNegativeCoordinates()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "-40 90 -40");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("negative_coordinates");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();

    const int y = 100;
    world.setBlock(-40, y, -40, BlockId::Stone);

    const auto chunkPos = World::getChunkXZ(-40, -40);
    const auto localPos = World::getBlockXZ(-40, -40);
    check("S0.1/negative-chunk-mapping",
          chunkPos.x == -3 && chunkPos.z == -3 && localPos.x == 8 &&
              localPos.z == 8,
          "chunk (" + std::to_string(chunkPos.x) + "," +
              std::to_string(chunkPos.z) + ") local (" +
              std::to_string(localPos.x) + "," + std::to_string(localPos.z) +
              ")");

    Chunk *negativeChunk = chunks.findChunk(chunkPos.x, chunkPos.z);
    const bool storedInChunk =
        negativeChunk != nullptr &&
        negativeChunk->getBlock(localPos.x, y, localPos.z).id ==
            static_cast<Block_t>(BlockId::Stone);
    check("S0.1/negative-setblock-targets-chunk", storedInChunk);
    check("S0.1/negative-getblock-roundtrip",
          world.getBlock(-40, y, -40).id ==
              static_cast<Block_t>(BlockId::Stone));

    // Cross the zero boundary: block -1 and block 0 must land in different
    // chunks with the correct local coordinates.
    world.preloadAround({0.f, 90.f, 0.f});
    world.setBlock(0, y, 0, BlockId::Stone);
    world.setBlock(-1, y, -1, BlockId::Sand);

    Chunk *zeroChunk = chunks.findChunk(0, 0);
    Chunk *minusChunk = chunks.findChunk(-1, -1);
    const bool zeroOk =
        zeroChunk != nullptr &&
        zeroChunk->getBlock(0, y, 0).id == static_cast<Block_t>(BlockId::Stone);
    const bool minusOk =
        minusChunk != nullptr &&
        minusChunk->getBlock(15, y, 15).id ==
            static_cast<Block_t>(BlockId::Sand);
    check("S0.1/cross-zero-chunk-split", zeroOk && minusOk,
          "chunk(0,0)local(0,0) ok=" + std::to_string(zeroOk) +
              " chunk(-1,-1)local(15,15) ok=" + std::to_string(minusOk));
    check("S0.1/cross-zero-readback",
          world.getBlock(0, y, 0).id == static_cast<Block_t>(BlockId::Stone) &&
              world.getBlock(-1, y, -1).id ==
                  static_cast<Block_t>(BlockId::Sand));
}

// ---------------------------------------------------------------------------
// S0.2 - block queries must not create chunks
// ---------------------------------------------------------------------------
void caseNoImplicitChunkCreation()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("implicit_chunks");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    const auto before = world.collectDebugStats().chunks.existingChunks;

    const auto farBlock = world.getBlock(100000, 80, 100000);
    const auto afterRead = world.collectDebugStats().chunks.existingChunks;
    check("S0.2/getblock-does-not-create-chunk", afterRead == before,
          "existing chunks " + std::to_string(before) + " -> " +
              std::to_string(afterRead));
    check("S0.2/unloaded-getblock-returns-air", farBlock.id == 0);

    world.setBlock(100000, 80, 100000, BlockId::Stone);
    const auto afterWrite = world.collectDebugStats().chunks.existingChunks;
    check("S0.2/setblock-does-not-create-chunk", afterWrite == before,
          "existing chunks " + std::to_string(before) + " -> " +
              std::to_string(afterWrite));
}

// ---------------------------------------------------------------------------
// S0.4 / S0.5 - mesh dirty propagation across section and chunk boundaries
// ---------------------------------------------------------------------------
void caseMeshDirtyPropagation()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("mesh_dirty");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();

    const int y = 100;
    const int sectionY = y / CHUNK_SIZE;

    // Force the target sections to exist in both the edited chunk and the
    // western neighbour chunk.
    world.setBlock(8, y, 8, BlockId::Stone);
    world.setBlock(0, y, 8, BlockId::Stone);
    world.setBlock(-1, y, 8, BlockId::Stone);

    auto sectionAt = [&](int chunkX, int chunkZ, int index) -> ChunkSection * {
        Chunk *chunk = chunks.findChunk(chunkX, chunkZ);
        return chunk == nullptr ? nullptr : chunk->findSection(index);
    };

    ChunkSection *target = sectionAt(0, 0, sectionY);
    ChunkSection *west = sectionAt(-1, 0, sectionY);
    ChunkSection *below = sectionAt(0, 0, sectionY - 1);

    if (target == nullptr || west == nullptr || below == nullptr) {
        check("S0.5/sections-available", false,
              "missing sections for the boundary scenario");
        return;
    }
    check("S0.5/sections-available", true);

    auto clearDirty = [](ChunkSection *section) {
        if (section != nullptr) {
            section->makeMesh();
        }
    };

    // Interior edit: only the owning section becomes dirty.
    clearDirty(target);
    clearDirty(west);
    world.setBlock(8, y, 8, BlockId::Dirt);
    check("S0.5/interior-edit-marks-owner", target->isMeshDirty());
    check("S0.5/interior-edit-skips-neighbor", !west->isMeshDirty());

    // Chunk boundary edit: the western neighbour must become dirty too.
    clearDirty(target);
    clearDirty(west);
    world.setBlock(0, y, 8, BlockId::Dirt);
    check("S0.5/chunk-boundary-marks-owner", target->isMeshDirty());
    check("S0.5/chunk-boundary-marks-neighbor", west->isMeshDirty());

    // Section boundary edit: the section below must become dirty too.
    clearDirty(target);
    clearDirty(below);
    world.setBlock(8, sectionY * CHUNK_SIZE, 8, BlockId::Dirt);
    check("S0.5/section-boundary-marks-owner", target->isMeshDirty());
    check("S0.5/section-boundary-marks-below", below->isMeshDirty());

    // S0.4 - debug stats must expose the dirty state.
    const auto stats = world.collectDebugStats();
    check("S0.4/debug-stats-report-state",
          stats.chunks.existingChunks > 0 && stats.chunks.sections > 0 &&
              stats.chunks.meshDirtySections > 0,
          "chunks=" + std::to_string(stats.chunks.existingChunks) +
              " sections=" + std::to_string(stats.chunks.sections) +
              " dirty=" + std::to_string(stats.chunks.meshDirtySections));
    check("S0.4/debug-stats-report-seed", stats.terrainSeed == kValidationSeed,
          "seed=" + std::to_string(stats.terrainSeed));

    // M2 - rebuild only a small FIFO batch each frame, even when edits dirty
    // many independent sections before the next update.
    int drainGuard = 0;
    while (world.collectDebugStats().queuedChunkUpdates > 0 &&
           drainGuard++ < 16) {
        world.update(camera);
    }

    struct QueueTarget {
        int blockX;
        int blockZ;
        ChunkSection *section = nullptr;
    };
    std::array<QueueTarget, 5> queueTargets{{
        {-8, -8},
        {8, -8},
        {24, -8},
        {-8, 8},
        {8, 8},
    }};
    constexpr int queueBlockY = 100;
    const int queueSectionY = queueBlockY / CHUNK_SIZE;

    for (auto &entry : queueTargets) {
        const ChunkBlock current =
            world.getBlock(entry.blockX, queueBlockY, entry.blockZ);
        world.setBlock(entry.blockX, queueBlockY, entry.blockZ,
                       current.id == static_cast<Block_t>(BlockId::Stone)
                           ? ChunkBlock(BlockId::Dirt)
                           : ChunkBlock(BlockId::Stone));
        const auto chunkPosition =
            World::getChunkXZ(entry.blockX, entry.blockZ);
        entry.section =
            sectionAt(chunkPosition.x, chunkPosition.z, queueSectionY);
    }
    drainGuard = 0;
    while (world.collectDebugStats().queuedChunkUpdates > 0 &&
           drainGuard++ < 16) {
        world.update(camera);
    }

    for (const auto &entry : queueTargets) {
        const ChunkBlock current =
            world.getBlock(entry.blockX, queueBlockY, entry.blockZ);
        world.setBlock(entry.blockX, queueBlockY, entry.blockZ,
                       current.id == static_cast<Block_t>(BlockId::CoalOre)
                           ? ChunkBlock(BlockId::Dirt)
                           : ChunkBlock(BlockId::CoalOre));
    }
    const auto queuedBeforeDuplicate =
        world.collectDebugStats().queuedChunkUpdates;
    world.setBlock(queueTargets.front().blockX, queueBlockY,
                   queueTargets.front().blockZ, BlockId::IronOre);
    const WorldDebugStats queuedStats = world.collectDebugStats();
    check("M2/queue-deduplicates-sections",
          queuedBeforeDuplicate == queueTargets.size() &&
              queuedStats.queuedChunkUpdates == queueTargets.size(),
          "queued=" + std::to_string(queuedStats.queuedChunkUpdates));

    const std::size_t rebuildsBeforeBatch =
        queuedStats.chunks.meshRebuilds;
    world.update(camera);
    const WorldDebugStats firstBatchStats = world.collectDebugStats();
    const std::size_t firstBatchRebuilds =
        firstBatchStats.chunks.meshRebuilds - rebuildsBeforeBatch;
    check("M2/frame-rebuild-budget",
          firstBatchRebuilds == World::ChunkMeshRebuildBudgetPerUpdate &&
              firstBatchStats.queuedChunkUpdates ==
                  queueTargets.size() -
                      World::ChunkMeshRebuildBudgetPerUpdate,
          "rebuilt=" + std::to_string(firstBatchRebuilds) +
              " remaining=" +
              std::to_string(firstBatchStats.queuedChunkUpdates));

    bool fifoState = true;
    for (std::size_t index = 0; index < queueTargets.size(); ++index) {
        const bool shouldRemainPending =
            index >= World::ChunkMeshRebuildBudgetPerUpdate;
        const ChunkMeshState state = queueTargets[index].section != nullptr
            ? queueTargets[index].section->getMeshState()
            : ChunkMeshState::Clean;
        const bool remainsPending = state == ChunkMeshState::Dirty ||
                                    state == ChunkMeshState::Queued ||
                                    state == ChunkMeshState::Building;
        fifoState = fifoState && queueTargets[index].section != nullptr &&
                    remainsPending == shouldRemainPending;
    }
    check("M2/fifo-order-preserved", fifoState);

    std::size_t updateCount = 1;
    while (world.collectDebugStats().queuedChunkUpdates > 0 &&
           updateCount < queueTargets.size()) {
        world.update(camera);
        ++updateCount;
    }
    const WorldDebugStats drainedStats = world.collectDebugStats();
    bool allTargetsReady = true;
    for (const auto &entry : queueTargets) {
        allTargetsReady = allTargetsReady && entry.section != nullptr &&
                          (entry.section->getMeshState() ==
                               ChunkMeshState::CpuReady ||
                           entry.section->getMeshState() ==
                               ChunkMeshState::Clean);
    }
    check("M2/queue-drains-across-frames",
          drainedStats.queuedChunkUpdates == 0 && allTargetsReady &&
              drainedStats.chunks.meshRebuilds - rebuildsBeforeBatch ==
                  queueTargets.size(),
          "updates=" + std::to_string(updateCount) +
              " rebuilt=" +
              std::to_string(drainedStats.chunks.meshRebuilds -
                             rebuildsBeforeBatch));

    const std::size_t rebuildsAfterDrain = drainedStats.chunks.meshRebuilds;
    world.update(camera);
    check("M2/empty-queue-does-no-work",
          world.collectDebugStats().chunks.meshRebuilds ==
              rebuildsAfterDrain);
}

// ---------------------------------------------------------------------------
// S2.1 - S2.6 - chunk, world metadata and player persistence
// ---------------------------------------------------------------------------
void casePersistence()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("persistence");
    Config config = makeConfig();
    Camera camera(config);

    const int y = 100;
    int firstSeed = 0;
    glm::vec3 savedSpawn{0.f};
    ActorId savedMobId = InvalidActorId;
    ActorId savedItemId = InvalidActorId;
    std::string savedWorldId;
    ActorSaveState savedMobState;
    ActorSaveState savedItemState;

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        firstSeed = world.collectDebugStats().terrainSeed;

        world.setBlock(8, y, 8, BlockId::Stone);
        world.setBlock(9, y, 8, BlockId::CoalOre);
        world.setBlock(0, y, 0, BlockId::Sand);

        check("S2.4/chunk-marked-save-dirty",
              world.collectDebugStats().chunks.saveDirtyChunks > 0,
              "save dirty chunks=" +
                  std::to_string(
                      world.collectDebugStats().chunks.saveDirtyChunks));

        player.position = glm::vec3(12.5f, 101.f, 13.5f);
        player.rotation = glm::vec3(15.f, 45.f, 0.f);
        player.addItem(Material::toMaterial(Material::ID::Stone), 7);
        player.addItem(Material::WOODEN_PICKAXE, 1, 7);

        savedMobId = world.spawnMob(
            "validation_persistent_mob", glm::vec3(20.5f, 101.f, 20.5f));
        savedItemId = world.spawnItemEntity(
            Material::ID::IronOre, 4, glm::vec3(18.5f, 103.f, 18.5f),
            glm::vec3(0.25f, 0.5f, -0.25f));
        auto *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(savedMobId));
        auto *item = dynamic_cast<ItemEntity *>(
            world.getActorManager().findActor(savedItemId));
        check("P2/actors-created-before-save",
              mob != nullptr && item != nullptr);
        if (mob != nullptr && item != nullptr) {
            mob->setWanderSpeed(0.45f);
            mob->setDrop(Material::ID::CoalOre, 2);
            mob->velocity = glm::vec3(0.1f, 0.f, -0.2f);
            mob->damage(world, 3.f);
            item->setPickupDelay(6.5f);
            savedMobState = mob->getSaveState();
            savedItemState = item->getSaveState();
        }

        const ChunkDebugStats saveMetricsBefore =
            world.collectDebugStats().chunks;
        RuntimeOperationTimings &operationTimings = runtimeOperationTimings();
        operationTimings.reset(true);
        const bool worldSaveSucceeded = world.save();
        const ChunkDebugStats saveMetricsAfter =
            world.collectDebugStats().chunks;
        check("S2.1/world-save-succeeds", worldSaveSucceeded);
        std::vector<WorldBackupInfo> worldBackups;
        std::string worldBackupError;
        check("K3/world-save-publishes-validated-backup",
              worldSaveSucceeded &&
                  WorldBackup(directory).listBackups(worldBackups,
                                                     &worldBackupError) &&
                  worldBackups.size() == 1 &&
                  worldBackups.front().worldFormatVersion ==
                      WorldSaveFormatVersion &&
                  worldBackups.front().fileCount >= 2,
              worldBackupError);
        check("K2/world-stats-expose-save-total-and-maximum",
              worldSaveSucceeded &&
                  saveMetricsAfter.saveTransactions >
                      saveMetricsBefore.saveTransactions &&
                  saveMetricsAfter.saveTotalMs >
                      saveMetricsBefore.saveTotalMs &&
                  saveMetricsAfter.saveMaxMs > 0.0 &&
                  saveMetricsAfter.saveTotalMs >=
                      saveMetricsAfter.saveMaxMs,
              "transactions=" +
                  std::to_string(saveMetricsAfter.saveTransactions) +
                  " total_ms=" +
                  std::to_string(saveMetricsAfter.saveTotalMs) +
                  " max_ms=" +
                  std::to_string(saveMetricsAfter.saveMaxMs));

        RuntimeOperationRecord saveTiming;
        RuntimeOperationRecord backupTiming;
        const bool hasSaveTiming = operationTimings.latest(
            RuntimeOperationKind::Save, saveTiming);
        const bool hasBackupTiming = operationTimings.latest(
            RuntimeOperationKind::Backup, backupTiming);
        check(
            "Q2/real-world-save-emits-complete-operation-timings",
            hasSaveTiming && saveTiming.success && saveTiming.phaseCount == 5 &&
                saveTiming.filesWritten >= 2 &&
                saveTiming.chunksWritten >= 1 &&
                saveTiming.bytesWritten > 0 &&
                saveTiming.totalMilliseconds >=
                    saveTiming.cumulativeMilliseconds[4] &&
                saveTiming.totalMilliseconds >=
                    saveTiming.mainThreadMaxStallMilliseconds &&
                hasBackupTiming && backupTiming.success &&
                backupTiming.phaseCount == 4 &&
                backupTiming.bytesRead > 0 &&
                backupTiming.bytesWritten > 0,
            "save_files=" + std::to_string(saveTiming.filesWritten) +
                " save_chunks=" + std::to_string(saveTiming.chunksWritten) +
                " save_bytes=" + std::to_string(saveTiming.bytesWritten));

        std::ostringstream operationSummary;
        operationTimings.appendLatestSummary(operationSummary);
        const std::string operationSummaryText = operationSummary.str();
        const char *operationSummaryOutput =
            std::getenv("HELLO_OPERATION_SUMMARY_OUT");
        if (operationSummaryOutput != nullptr &&
            operationSummaryOutput[0] != '\0') {
            std::ofstream output(operationSummaryOutput,
                                 std::ios::trunc);
            output << operationSummaryText;
        }
        check(
            "Q2/real-world-save-summary-matches-q1-schema",
            operationSummaryText.find("save_prepare_complete_ms=") !=
                    std::string::npos &&
                operationSummaryText.find("save_replace_complete_ms=") !=
                    std::string::npos &&
                operationSummaryText.find("save_total_ms=") !=
                    std::string::npos &&
                operationSummaryText.find(
                    "save_main_thread_max_stall_ms=") != std::string::npos &&
                operationSummaryText.find("backup_total_ms=") !=
                    std::string::npos);

        operationTimings.reset(false);
        world.setBlock(10, y, 8, BlockId::Dirt);
        const bool disabledSaveSucceeded = world.save();
        WorldSaveData disabledSaveData;
        check("Q2/disabled-timing-preserves-save-and-retains-no-records",
              disabledSaveSucceeded &&
                  WorldSave(directory).load(disabledSaveData) &&
                  WorldCatalogue::isValidWorldId(disabledSaveData.worldId) &&
                  operationTimings.snapshot().empty());

        WorldSaveData meta;
        WorldSave saveFile(directory);
        check("S2.1/world-meta-readable", saveFile.load(meta));
        savedSpawn = meta.spawnPoint;
        check("S2.1/world-meta-has-seed", meta.seed == firstSeed,
              "meta seed=" + std::to_string(meta.seed));
        check("P2/world-meta-stores-actors",
              meta.version == WorldSaveFormatVersion &&
                  meta.actors.size() == 2,
              "version=" + std::to_string(meta.version) +
                  " actors=" + std::to_string(meta.actors.size()));
        savedWorldId = meta.worldId;
        check("K1/version-three-world-identity",
              WorldCatalogue::isValidWorldId(meta.worldId) &&
                  WorldCatalogue::isValidDisplayName(meta.worldName) &&
                  WorldCatalogue::isValidBuildIdentity(
                      meta.lastBuildIdentity) &&
                  meta.createdUtc >= LegacyWorldTimestampUtc &&
                  meta.lastPlayedUtc >= meta.createdUtc,
              "id=" + meta.worldId + " name=" + meta.worldName +
                  " build=" + meta.lastBuildIdentity);
        meta.worldName = "Persistence Fixture";
        WorldSaveData namedMeta;
        WorldSaveData invalidMeta = meta;
        invalidMeta.worldId = "../escape";
        WorldSaveData preservedMeta;
        check("K1/quoted-display-name-roundtrip",
              saveFile.save(meta) && saveFile.load(namedMeta) &&
                  namedMeta.worldName == meta.worldName &&
                  !saveFile.save(invalidMeta) &&
                  saveFile.load(preservedMeta) &&
                  preservedMeta.worldId == savedWorldId &&
                  preservedMeta.worldName == meta.worldName,
              "name=" + namedMeta.worldName);
    }

    // Relaunch without forced position/rotation so the save state is the only
    // source of player and spawn data.
    clearDeterministicEnv();
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.preloadAround({8.f, 90.f, 8.f});

        check("S2.5/chunk-edit-survives-relaunch",
              world.getBlock(8, y, 8).id ==
                      static_cast<Block_t>(BlockId::Stone) &&
                  world.getBlock(9, y, 8).id ==
                      static_cast<Block_t>(BlockId::CoalOre) &&
                  world.getBlock(0, y, 0).id ==
                      static_cast<Block_t>(BlockId::Sand),
              "read back ids " +
                  std::to_string(static_cast<int>(world.getBlock(8, y, 8).id)) +
                  "/" +
                  std::to_string(static_cast<int>(world.getBlock(9, y, 8).id)) +
                  "/" +
                  std::to_string(static_cast<int>(world.getBlock(0, y, 0).id)));

        check("S2.6/player-position-restored",
              std::abs(player.position.x - 12.5f) < 0.01f &&
                  std::abs(player.position.y - 101.f) < 0.01f &&
                  std::abs(player.position.z - 13.5f) < 0.01f,
              vecToString(player.position));
        check("S2.6/player-rotation-restored",
              std::abs(player.rotation.x - 15.f) < 0.01f &&
                  std::abs(player.rotation.y - 45.f) < 0.01f,
              vecToString(player.rotation));
        check("S2.6/player-inventory-restored",
              player.getHeldItems().getMaterial().id == Material::ID::Stone &&
                  player.getHeldItems().getNumInStack() == 7,
              "held " + player.getHeldItems().getMaterial().name + " x" +
                  std::to_string(player.getHeldItems().getNumInStack()));
        check("G3/tool-durability-survives-save-and-reload",
              player.getInventorySlot(1).getMaterial().id ==
                      Material::ID::WoodenPickaxe &&
                  player.getInventorySlot(1).getDurability() == 7);

        auto *restoredMob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(savedMobId));
        auto *restoredItem = dynamic_cast<ItemEntity *>(
            world.getActorManager().findActor(savedItemId));
        check("P2/actors-restored-after-relaunch",
              restoredMob != nullptr && restoredItem != nullptr &&
                  world.getActorManager().getActorCount() == 2,
              "actors=" + std::to_string(
                  world.getActorManager().getActorCount()));
        check("P2/mob-state-restored",
              restoredMob != nullptr &&
                  restoredMob->getType() == savedMobState.type &&
                  glm::length(restoredMob->position -
                              savedMobState.position) < 0.001f &&
                  glm::length(restoredMob->velocity -
                              savedMobState.velocity) < 0.001f &&
                  std::abs(restoredMob->getHealth() -
                           savedMobState.health) < 0.001f &&
                  std::abs(restoredMob->getWanderSpeed() -
                           savedMobState.wanderSpeed) < 0.001f &&
                  restoredMob->getDropMaterialId() ==
                      Material::ID::CoalOre &&
                  restoredMob->getDropAmount() == 2);
        check("P2/item-state-restored",
              restoredItem != nullptr &&
                  restoredItem->getMaterialId() ==
                      Material::ID::IronOre &&
                  restoredItem->getAmount() == savedItemState.amount &&
                  glm::length(restoredItem->position -
                              savedItemState.position) < 0.001f &&
                  glm::length(restoredItem->velocity -
                              savedItemState.velocity) < 0.001f &&
                  std::abs(restoredItem->getPickupDelay() -
                           savedItemState.pickupDelay) < 0.001f);
        const ActorId nextActorId = world.spawnMob(
            "validation_next_mob", glm::vec3(22.f, 101.f, 22.f));
        check("P2/restored-ids-advance-sequence",
              nextActorId > std::max(savedMobId, savedItemId),
              "next=" + std::to_string(nextActorId));

        check("S6.1/seed-restored-from-save",
              world.collectDebugStats().terrainSeed == firstSeed,
              "seed " + std::to_string(firstSeed) + " -> " +
                  std::to_string(world.collectDebugStats().terrainSeed));

        WorldSaveData meta;
        WorldSave saveFile(directory);
        saveFile.load(meta);
        check("K1/world-id-stable-after-reload",
              !savedWorldId.empty() && meta.worldId == savedWorldId,
              savedWorldId + " -> " + meta.worldId);
        check("S1.3/spawn-point-stable",
              std::abs(meta.spawnPoint.x - savedSpawn.x) < 0.01f &&
                  std::abs(meta.spawnPoint.y - savedSpawn.y) < 0.01f &&
                  std::abs(meta.spawnPoint.z - savedSpawn.z) < 0.01f,
              vecToString(savedSpawn) + " -> " + vecToString(meta.spawnPoint));
    }

    const auto legacyDirectory = freshSaveDirectory("persistence_v1");
    {
        std::ofstream legacyMeta(
            std::filesystem::path(legacyDirectory) / "world.meta");
        legacyMeta << "version 1\n"
                   << "world_id legacy\n"
                   << "world_name LegacyWorld\n"
                   << "seed 123\n"
                   << "spawn 1 2 3\n"
                   << "world_time 4\n"
                   << "generator ClassicOverWorld\n"
                   << "player_present 0\n";
    }
    WorldSaveData legacyData;
    check("P2/version-one-save-remains-readable",
          WorldSave(legacyDirectory).load(legacyData) &&
              legacyData.version == 1 && legacyData.actors.empty());
    {
        Player legacyPlayer;
        World legacyWorld(camera, config, legacyPlayer, legacyDirectory,
                          false, 0);
    }
    WorldSaveData upgradedData;
    check("P2/version-one-save-upgrades",
          WorldSave(legacyDirectory).load(upgradedData) &&
              upgradedData.version == WorldSaveFormatVersion &&
              upgradedData.actors.empty());
}

// ---------------------------------------------------------------------------
// M3 - the mesh halo snapshot must match what the world reports
// ---------------------------------------------------------------------------
void caseSectionMeshInput()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("section_mesh_input");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();

    // Section 4 of chunk (0,0) covers y 64-79, which straddles the surface.
    const int sectionY = 4;
    Chunk *chunk = chunks.findChunk(0, 0);
    if (chunk == nullptr) {
        check("M3/chunk-available", false);
        return;
    }
    ChunkSection *section = chunk->findSection(sectionY);
    if (section == nullptr) {
        check("M3/section-available", false);
        return;
    }
    check("M3/section-available", true);

    SectionMeshInput input;
    section->captureMeshInput(input);

    ChunkMeshCollection measuredMeshes;
    ChunkMeshBuilder(input, measuredMeshes).buildMesh();
    const ChunkDebugStats metricsBefore = chunks.collectDebugStats();
    constexpr double measuredBuildMs = 1.25;
    chunks.recordMeshRebuild(measuredMeshes, measuredBuildMs);
    const ChunkDebugStats metricsAfter = chunks.collectDebugStats();

    const std::size_t solidFaces =
        static_cast<std::size_t>(measuredMeshes.solidMesh.faces);
    const std::size_t transparentFaces =
        static_cast<std::size_t>(measuredMeshes.transparentMesh.faces);
    const std::size_t waterFaces =
        static_cast<std::size_t>(measuredMeshes.waterMesh.faces);
    const std::size_t floraFaces =
        static_cast<std::size_t>(measuredMeshes.floraMesh.faces);
    const auto vertices = [](const ChunkMesh &mesh) {
        return mesh.getClientMesh().vertexPositions.size() / 3;
    };
    check("M1/rebuild-count-recorded",
          metricsAfter.meshRebuilds == metricsBefore.meshRebuilds + 1);
    check("M1/build-time-recorded",
          std::abs(metricsAfter.meshBuildTotalMs -
                       metricsBefore.meshBuildTotalMs - measuredBuildMs) <
                  0.001 &&
              std::abs(metricsAfter.meshBuildLastMs - measuredBuildMs) <
                  0.001 &&
              metricsAfter.meshBuildMaxMs >= measuredBuildMs);
    check("M1/face-counts-recorded",
          metricsAfter.solidFaces == metricsBefore.solidFaces + solidFaces &&
              metricsAfter.transparentFaces ==
                  metricsBefore.transparentFaces + transparentFaces &&
              metricsAfter.waterFaces == metricsBefore.waterFaces +
                  waterFaces &&
              metricsAfter.floraFaces == metricsBefore.floraFaces +
                  floraFaces &&
              solidFaces + transparentFaces + waterFaces + floraFaces > 0);
    check("M1/vertex-counts-recorded",
          metricsAfter.solidVertices ==
                  metricsBefore.solidVertices +
                      vertices(measuredMeshes.solidMesh) &&
              metricsAfter.transparentVertices ==
                  metricsBefore.transparentVertices +
                      vertices(measuredMeshes.transparentMesh) &&
              metricsAfter.waterVertices ==
                  metricsBefore.waterVertices +
                      vertices(measuredMeshes.waterMesh) &&
              metricsAfter.floraVertices ==
                  metricsBefore.floraVertices +
                      vertices(measuredMeshes.floraMesh));
    check("M1/four-vertices-per-face",
          vertices(measuredMeshes.solidMesh) == solidFaces * 4 &&
              vertices(measuredMeshes.transparentMesh) ==
                  transparentFaces * 4 &&
              vertices(measuredMeshes.waterMesh) == waterFaces * 4 &&
              vertices(measuredMeshes.floraMesh) == floraFaces * 4);

    // Every cell of the 18^3 halo must agree with a direct world read,
    // including the border that reaches into neighbouring chunks.
    int mismatches = 0;
    int borderMismatches = 0;
    for (int y = -1; y <= CHUNK_SIZE; ++y) {
        for (int z = -1; z <= CHUNK_SIZE; ++z) {
            for (int x = -1; x <= CHUNK_SIZE; ++x) {
                const int worldY = sectionY * CHUNK_SIZE + y;
                const auto expected = world.getBlock(x, worldY, z);
                const auto actual = input.getBlock(x, y, z);
                if (expected.id != actual.id ||
                    expected.metadata != actual.metadata) {
                    ++mismatches;
                    const bool isBorder = x < 0 || x >= CHUNK_SIZE || y < 0 ||
                                          y >= CHUNK_SIZE || z < 0 ||
                                          z >= CHUNK_SIZE;
                    if (isBorder) {
                        ++borderMismatches;
                    }
                }
            }
        }
    }

    check("M3/halo-matches-world", mismatches == 0,
          "mismatches=" + std::to_string(mismatches) + " (border " +
              std::to_string(borderMismatches) + ") over " +
              std::to_string(SectionMeshInput::Volume) + " cells");

    // A block edit must invalidate a snapshot taken before it, otherwise a
    // mesh built off-lock could overwrite the edit.
    const std::uint32_t revisionBefore = section->getBlockRevision();
    world.setBlock(8, sectionY * CHUNK_SIZE + 8, 8, BlockId::CoalOre);
    check("M3/block-revision-advances",
          section->getBlockRevision() != revisionBefore,
          "revision " + std::to_string(revisionBefore) + " -> " +
              std::to_string(section->getBlockRevision()));

    // The builder must read blocks by coordinate. It used to walk a running
    // pointer that was not advanced for skipped layers, so any fully enclosed
    // layer offset every later block read.
    SectionMeshInput refreshed;
    section->captureMeshInput(refreshed);
    check("M3/edit-visible-in-new-snapshot",
          refreshed.getBlock(8, 8, 8).id ==
              static_cast<Block_t>(BlockId::CoalOre),
          "block id=" +
              std::to_string(static_cast<int>(refreshed.getBlock(8, 8, 8).id)));
}

// ---------------------------------------------------------------------------
// M4 - opaque cubes merge into material-safe rectangles while transparent
// passes keep their original topology
// ---------------------------------------------------------------------------
void caseGreedyMeshing()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("greedy_meshing");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockY = 200;
    constexpr int sectionY = blockY / CHUNK_SIZE;
    for (int z = 4; z < 12; ++z) {
        for (int x = 4; x < 12; ++x) {
            world.setBlock(x, blockY, z, BlockId::Stone);
        }
    }

    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
    check("M4/high-air-section-available", section != nullptr);
    if (section == nullptr) {
        return;
    }

    const auto buildSectionMeshes = [&]() {
        SectionMeshInput input;
        section->captureMeshInput(input);
        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes).buildMesh();
        return meshes;
    };

    ChunkMeshCollection singleMaterial = buildSectionMeshes();
    const Mesh &singleSolid = singleMaterial.solidMesh.getClientMesh();
    check("M4/flat-cuboid-reconstruction-safe-greedy",
          singleMaterial.solidMesh.faces == 14 &&
              singleSolid.vertexPositions.size() / 3 < 56 &&
              singleSolid.indices.size() == 84,
          "faces=" + std::to_string(singleMaterial.solidMesh.faces) +
              " vertices=" +
              std::to_string(singleSolid.vertexPositions.size() / 3) +
              " unshared=56 naive_faces=160");

    const auto repeatRange = std::minmax_element(
        singleSolid.textureRepeatCoords.begin(),
        singleSolid.textureRepeatCoords.end());
    const float minRepeat = singleSolid.textureRepeatCoords.empty()
                                ? 0.f
                                : *repeatRange.first;
    const float maxRepeat = singleSolid.textureRepeatCoords.empty()
                                ? 0.f
                                : *repeatRange.second;
    bool stoneTileStable = true;
    for (std::size_t index = 0; index < singleSolid.textureCoords.size();
         ++index) {
        const int expectedTile = index % 2 == 0 ? 3 : 0;
        stoneTileStable =
            stoneTileStable &&
            static_cast<int>(
                std::floor(singleSolid.textureCoords[index] * 16.f)) ==
                expectedTile;
    }
    check("M4/global-repeat-preserves-atlas-tile",
          std::abs((maxRepeat - minRepeat) - 8.f) < 0.001f &&
              minRepeat >= 0.f && maxRepeat <= CHUNK_SIZE &&
              stoneTileStable &&
              singleSolid.textureRepeatCoords.size() ==
                  singleSolid.vertexPositions.size() / 3 * 2,
          "repeat_range=" + std::to_string(minRepeat) + "/" +
              std::to_string(maxRepeat));

    for (int z = 4; z < 12; ++z) {
        for (int x = 8; x < 12; ++x) {
            world.setBlock(x, blockY, z, BlockId::Dirt);
        }
    }
    ChunkMeshCollection splitMaterials = buildSectionMeshes();
    check("M4/material-boundary-preserved",
          splitMaterials.solidMesh.faces == 20,
          "faces=" + std::to_string(splitMaterials.solidMesh.faces));

    world.setBlock(4, blockY + 2, 4, BlockId::Water);
    world.setBlock(5, blockY + 2, 4, BlockId::Water);
    world.setBlock(8, blockY + 2, 4, BlockId::TallGrass);
    world.setBlock(9, blockY + 2, 4, BlockId::TallGrass);
    ChunkMeshCollection separatePasses = buildSectionMeshes();
    check("M4/water-topology-remains-separate",
          separatePasses.waterMesh.faces == 10,
          "faces=" + std::to_string(separatePasses.waterMesh.faces));
    check("M4/flora-topology-remains-separate",
          separatePasses.floraMesh.faces == 4,
          "faces=" + std::to_string(separatePasses.floraMesh.faces));
}

// ---------------------------------------------------------------------------
// V10B3 - biome-controlled world tint and coordinate-stable atlas variants
// retain base UI coordinates and enter the real greedy merge identity
// ---------------------------------------------------------------------------
void caseTerrainAppearance()
{
    check("V10B3/appearance-contract-version-and-layout",
          TerrainAppearance::ContractVersion == 1 &&
              TerrainAppearance::VariantsPerEcology == 3 &&
              TerrainAppearance::VariantPatchSize == 4 &&
              TerrainAppearance::EcologyRowBase == 3 &&
              TerrainAppearance::ecologyRow(TerrainBiome::Desert) == 3 &&
              TerrainAppearance::ecologyRow(TerrainBiome::Grassland) == 4 &&
              TerrainAppearance::ecologyRow(TerrainBiome::LightForest) == 5 &&
              TerrainAppearance::ecologyRow(
                  TerrainBiome::TemperateForest) == 6 &&
              TerrainAppearance::ecologyRow(TerrainBiome::Ocean) == 7 &&
              TerrainAppearance::ecologyRow(
                  TerrainBiome::Mountain) == 6);

    const glm::ivec3 negativePosition{-193, 71, -257};
    const std::uint8_t negativeFirst =
        TerrainAppearance::coordinateVariant(
            kValidationSeed, negativePosition, BlockId::Grass);
    const std::uint8_t negativeSecond =
        TerrainAppearance::coordinateVariant(
            kValidationSeed, negativePosition, BlockId::Grass);
    check("V10B3/negative-coordinate-variant-is-deterministic",
          negativeFirst == negativeSecond &&
              negativeFirst < TerrainAppearance::VariantsPerEcology,
          "variant=" + std::to_string(negativeFirst));

    const std::uint8_t patchOrigin =
        TerrainAppearance::coordinateVariant(
            kValidationSeed, {8, 80, -12}, BlockId::Grass);
    bool patchIsStable = true;
    for (int y = 80; y <= 83; ++y) {
        for (int z = -12; z <= -9; ++z) {
            for (int x = 8; x <= 11; ++x) {
                patchIsStable = patchIsStable &&
                    TerrainAppearance::coordinateVariant(
                        kValidationSeed, {x, y, z}, BlockId::Grass) ==
                        patchOrigin;
            }
        }
    }
    check("V10B3/four-block-appearance-patch-is-stable",
          patchIsStable, "variant=" + std::to_string(patchOrigin));

    std::set<int> observedVariants;
    bool seedChangesAtLeastOneCoordinate = false;
    for (int x = -64; x <= 64; ++x) {
        const glm::ivec3 position{x, 80, x * 3 - 7};
        const std::uint8_t first =
            TerrainAppearance::coordinateVariant(
                kValidationSeed, position, BlockId::Grass);
        const std::uint8_t second =
            TerrainAppearance::coordinateVariant(
                kValidationSeed + 1, position, BlockId::Grass);
        observedVariants.insert(first);
        seedChangesAtLeastOneCoordinate =
            seedChangesAtLeastOneCoordinate || first != second;
    }
    check("V10B3/all-three-coordinate-variants-are-reachable",
          observedVariants.size() == 3,
          "variants=" + std::to_string(observedVariants.size()));
    check("V10B3/terrain-seed-participates-in-variant",
          seedChangesAtLeastOneCoordinate);

    const std::array<TerrainBiome, 5> biomes{{
        TerrainBiome::Desert,
        TerrainBiome::Grassland,
        TerrainBiome::LightForest,
        TerrainBiome::TemperateForest,
        TerrainBiome::Ocean,
    }};
    std::set<int> ecologyRows;
    bool allBiomesUseEcologyTiles = true;
    for (TerrainBiome biome : biomes) {
        const TerrainTileSelection grass = TerrainAppearance::select(
            BlockId::Grass, TerrainFaceKind::Top, {0, 0}, biome,
            kValidationSeed, {12, 80, -7});
        const TerrainTileSelection leaves = TerrainAppearance::select(
            BlockId::OakLeaf, TerrainFaceKind::Side, {6, 0}, biome,
            kValidationSeed, {12, 81, -7});
        const TerrainTileSelection water = TerrainAppearance::select(
            BlockId::Water, TerrainFaceKind::Top, {8, 0}, biome,
            kValidationSeed, {12, 79, -7});
        const TerrainTileSelection tallGrass = TerrainAppearance::select(
            BlockId::TallGrass, TerrainFaceKind::Resource, {11, 0},
            biome, kValidationSeed, {13, 81, -7});
        ecologyRows.insert(grass.coordinates.y);
        allBiomesUseEcologyTiles = allBiomesUseEcologyTiles &&
            grass.ecologyTinted && grass.coordinates.x >= 0 &&
            grass.coordinates.x <= 2 &&
            leaves.ecologyTinted && leaves.coordinates.x >= 6 &&
            leaves.coordinates.x <= 8 &&
            water.ecologyTinted && water.coordinates.x >= 9 &&
            water.coordinates.x <= 11 &&
            tallGrass.ecologyTinted && tallGrass.coordinates.x >= 12 &&
            tallGrass.coordinates.x <= 14 &&
            grass.coordinates.y == TerrainAppearance::ecologyRow(biome) &&
            leaves.coordinates.y == grass.coordinates.y &&
            water.coordinates.y == grass.coordinates.y &&
            tallGrass.coordinates.y == grass.coordinates.y;
    }
    check("V10B3/five-biomes-select-bounded-ecology-rows",
          allBiomesUseEcologyTiles && ecologyRows.size() == 5,
          "rows=" + std::to_string(ecologyRows.size()));

    const TerrainTileSelection grassBottom = TerrainAppearance::select(
        BlockId::Grass, TerrainFaceKind::Bottom, {2, 0},
        TerrainBiome::Desert, kValidationSeed, {1, 2, 3});
    const TerrainTileSelection stone = TerrainAppearance::select(
        BlockId::Stone, TerrainFaceKind::Top, {3, 0},
        TerrainBiome::Ocean, kValidationSeed, {1, 2, 3});
    const TerrainTileSelection wheat = TerrainAppearance::select(
        BlockId::WheatCrop, TerrainFaceKind::Resource, {11, 0},
        TerrainBiome::LightForest, kValidationSeed, {1, 2, 3});
    const Material::IconCoordinate grassIcon =
        Material::iconCoordinate(Material::ID::Grass);
    check("V10B3/non-target-and-ui-base-identities-stay-stable",
          !grassBottom.ecologyTinted &&
              grassBottom.coordinates.x == 2 &&
              grassBottom.coordinates.y == 0 &&
              !stone.ecologyTinted && stone.coordinates.x == 3 &&
              stone.coordinates.y == 0 &&
              !wheat.ecologyTinted && wheat.coordinates.x == 11 &&
              wheat.coordinates.y == 0 &&
              grassIcon.x == 0 && grassIcon.y == 0);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("terrain_appearance");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockY = 200;
    constexpr int blockZ = 8;
    int leftX = 3;
    for (; leftX < CHUNK_SIZE - 2; ++leftX) {
        const std::uint8_t left =
            TerrainAppearance::coordinateVariant(
                kValidationSeed, {leftX, blockY, blockZ},
                BlockId::Grass);
        const std::uint8_t right =
            TerrainAppearance::coordinateVariant(
                kValidationSeed, {leftX + 1, blockY, blockZ},
                BlockId::Grass);
        if (left != right) {
            break;
        }
    }
    check("V10B3/adjacent-different-variant-fixture-found",
          leftX < CHUNK_SIZE - 2,
          "x=" + std::to_string(leftX));
    if (leftX >= CHUNK_SIZE - 2) {
        return;
    }

    world.setBlock(leftX, blockY, blockZ, BlockId::Grass);
    world.setBlock(leftX + 1, blockY, blockZ, BlockId::Grass);
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section = chunk != nullptr
        ? chunk->findSection(blockY / CHUNK_SIZE)
        : nullptr;
    check("V10B3/appearance-fixture-section-available",
          section != nullptr);
    if (section == nullptr) {
        return;
    }

    SectionMeshInput input;
    section->captureMeshInput(input);
    const TerrainGenerator &generator =
        world.getChunkManager().getTerrainGenerator();
    const auto biomeIndex = [](TerrainBiome biome) {
        switch (biome) {
            case TerrainBiome::Desert: return 0;
            case TerrainBiome::Grassland: return 1;
            case TerrainBiome::LightForest: return 2;
            case TerrainBiome::TemperateForest: return 3;
            case TerrainBiome::Ocean: return 4;
            case TerrainBiome::Mountain: return -1;
        }
        return 1;
    };
    std::array<glm::ivec3, 5> biomeProbes{};
    std::array<bool, 5> foundBiomeProbe{};
    int remainingBiomeProbes = static_cast<int>(foundBiomeProbe.size());
    for (int z = -2048; z <= 2048 && remainingBiomeProbes > 0;
         z += CHUNK_SIZE) {
        for (int x = -2048; x <= 2048 && remainingBiomeProbes > 0;
             x += CHUNK_SIZE) {
            const TerrainBiome biome = generator.getBiomeAtWorld(x, z);
            const int index = biomeIndex(biome);
            if (index < 0) {
                continue;
            }
            if (foundBiomeProbe[index]) {
                continue;
            }
            bool stable = true;
            for (int dz : {-8, 0, 8}) {
                for (int dx : {-8, 0, 8}) {
                    stable = stable &&
                        generator.getBiomeAtWorld(x + dx, z + dz) == biome;
                }
            }
            if (!stable) {
                continue;
            }
            const int surface = generator.getSurfaceHeightAtWorld(x, z);
            const bool ocean = biome == TerrainBiome::Ocean;
            if ((!ocean && surface < WATER_LEVEL + 6) ||
                (ocean && surface > WATER_LEVEL)) {
                continue;
            }
            biomeProbes[index] = {
                x, std::max(surface + 8, WATER_LEVEL + 8), z,
            };
            foundBiomeProbe[index] = true;
            --remainingBiomeProbes;
        }
    }
    std::ostringstream biomeProbeDetail;
    constexpr const char *biomeNames[5] = {
        "desert", "grassland", "light_forest",
        "temperate_forest", "ocean",
    };
    for (std::size_t index = 0; index < biomeProbes.size(); ++index) {
        if (index != 0) {
            biomeProbeDetail << ';';
        }
        biomeProbeDetail << biomeNames[index] << '='
                         << biomeProbes[index].x << ','
                         << biomeProbes[index].y << ','
                         << biomeProbes[index].z;
    }
    check("V10B3/five-stable-biome-capture-probes-found",
          remainingBiomeProbes == 0, biomeProbeDetail.str());

    int biomeMismatches = 0;
    for (int z = -1; z <= CHUNK_SIZE; ++z) {
        for (int x = -1; x <= CHUNK_SIZE; ++x) {
            const int worldX =
                input.getLocation().x * CHUNK_SIZE + x;
            const int worldZ =
                input.getLocation().z * CHUNK_SIZE + z;
            if (input.getBiome(x, z) !=
                generator.getBiomeAtWorld(worldX, worldZ)) {
                ++biomeMismatches;
            }
        }
    }
    check("V10B3/snapshot-freezes-biome-halo-and-seed",
          biomeMismatches == 0 &&
              input.getTerrainSeed() == kValidationSeed,
          "biome_mismatches=" + std::to_string(biomeMismatches) +
              " seed=" + std::to_string(input.getTerrainSeed()));

    ChunkMeshCollection firstBuild;
    ChunkMeshCollection secondBuild;
    ChunkMeshBuilder(input, firstBuild).buildMesh();
    ChunkMeshBuilder(input, secondBuild).buildMesh();
    const Mesh &first = firstBuild.solidMesh.getClientMesh();
    const Mesh &second = secondBuild.solidMesh.getClientMesh();
    check("V10B3/rebuild-order-keeps-byte-identical-appearance",
          first.vertexPositions == second.vertexPositions &&
              first.textureCoords == second.textureCoords &&
              first.textureRepeatCoords == second.textureRepeatCoords &&
              first.indices == second.indices &&
              firstBuild.solidMesh.getLight() ==
                  secondBuild.solidMesh.getLight());

    int topFaces = 0;
    std::set<int> topTileColumns;
    const std::size_t faceCount = first.indices.size() / 6;
    for (std::size_t face = 0; face < faceCount; ++face) {
        const IndexedQuad quad = indexedQuad(first, face);
        if (quad.count != 4) {
            continue;
        }
        bool targetHeight = true;
        float minX = 100000.f;
        float maxX = -100000.f;
        float minZ = 100000.f;
        float maxZ = -100000.f;
        for (std::size_t vertex = 0; vertex < 4; ++vertex) {
            const std::size_t position = quad.vertices[vertex] * 3;
            targetHeight = targetHeight &&
                std::abs(first.vertexPositions[position + 1] -
                         static_cast<float>(blockY + 1)) < 0.001f;
            minX = std::min(minX, first.vertexPositions[position]);
            maxX = std::max(maxX, first.vertexPositions[position]);
            minZ = std::min(minZ, first.vertexPositions[position + 2]);
            maxZ = std::max(maxZ, first.vertexPositions[position + 2]);
        }
        if (!targetHeight || minX < leftX - 0.001f ||
            maxX > leftX + 2.f + 0.001f ||
            minZ < blockZ - 0.001f ||
            maxZ > blockZ + 1.f + 0.001f) {
            continue;
        }
        ++topFaces;
        const std::uint32_t vertex = quad.vertices[0];
        topTileColumns.insert(static_cast<int>(std::floor(
            first.textureCoords[vertex * 2] * 16.f)));
    }
    check("V10B3/greedy-key-preserves-adjacent-variants",
          topFaces == 2 && topTileColumns.size() == 2,
          "top_faces=" + std::to_string(topFaces) +
              " tile_columns=" +
              std::to_string(topTileColumns.size()));
    check("V10B3/vertex-and-terrain-identities-unchanged",
          TerrainBufferMetrics::VertexStrideBytes == 32 &&
              world.getChunkManager().getTerrainGenerationVersion() ==
                  CurrentTerrainGenerationVersion);
}

// ---------------------------------------------------------------------------
// V10A - deterministic per-vertex smooth light, ambient occlusion and
// diagonal selection retain the 32-byte terrain vertex layout
// ---------------------------------------------------------------------------
void caseVertexLighting()
{
    VertexLightCornerSamples clearSamples;
    clearSamples.centre = MAX_LIGHT_LEVEL;
    clearSamples.sideU = MAX_LIGHT_LEVEL;
    clearSamples.sideV = MAX_LIGHT_LEVEL;
    clearSamples.diagonal = MAX_LIGHT_LEVEL;
    const VertexLightCorner clear =
        VertexLighting::evaluateCorner(1.f, clearSamples);

    VertexLightCornerSamples diagonalSamples = clearSamples;
    diagonalSamples.diagonalOccludes = true;
    diagonalSamples.diagonal = MIN_LIGHT_LEVEL;
    const VertexLightCorner diagonal =
        VertexLighting::evaluateCorner(1.f, diagonalSamples);

    VertexLightCornerSamples oneSideSamples = clearSamples;
    oneSideSamples.sideUOccludes = true;
    oneSideSamples.sideU = MIN_LIGHT_LEVEL;
    const VertexLightCorner oneSide =
        VertexLighting::evaluateCorner(1.f, oneSideSamples);

    VertexLightCornerSamples enclosedCornerSamples = clearSamples;
    enclosedCornerSamples.sideUOccludes = true;
    enclosedCornerSamples.sideVOccludes = true;
    enclosedCornerSamples.sideU = MIN_LIGHT_LEVEL;
    enclosedCornerSamples.sideV = MIN_LIGHT_LEVEL;
    const VertexLightCorner enclosedCorner =
        VertexLighting::evaluateCorner(1.f, enclosedCornerSamples);

    check("V10A/vertex-lighting-contract-is-derived-version-one",
          VertexLighting::ContractVersion == 1 &&
              std::abs(clear.smoothLight - 1.f) < 0.001f &&
              std::abs(clear.finalLight - 1.f) < 0.001f &&
              clear.ambientOcclusion == 0);
    check("V10A/diagonal-and-single-side-each-occlude-once",
          diagonal.ambientOcclusion == 1 &&
              oneSide.ambientOcclusion == 1 &&
              std::abs(diagonal.finalLight - oneSide.finalLight) < 0.001f);
    check("V10A/two-sides-force-closed-corner",
          enclosedCorner.ambientOcclusion == 3 &&
              std::abs(enclosedCorner.smoothLight - 1.f) < 0.001f &&
              enclosedCorner.finalLight < oneSide.finalLight);
    const VertexLightCorner aoDisabled = VertexLighting::evaluateCorner(
        1.f, enclosedCornerSamples, false);
    check("V10A/ao-disabled-keeps-smooth-light-only",
          aoDisabled.ambientOcclusion == 0 &&
              std::abs(aoDisabled.smoothLight -
                       enclosedCorner.smoothLight) < 0.001f &&
              std::abs(aoDisabled.finalLight -
                       aoDisabled.smoothLight) < 0.001f);

    std::array<VertexLightCorner, 4> diagonalChoice{};
    diagonalChoice[0].smoothLight = 0.f;
    diagonalChoice[0].finalLight = 0.f;
    diagonalChoice[1].smoothLight = 0.5f;
    diagonalChoice[1].finalLight = 0.5f;
    diagonalChoice[2].smoothLight = 1.f;
    diagonalChoice[2].finalLight = 1.f;
    diagonalChoice[3].smoothLight = 0.5f;
    diagonalChoice[3].finalLight = 0.5f;
    check("V10A/lower-error-diagonal-is-selected",
          VertexLighting::shouldFlipDiagonal(diagonalChoice));
    diagonalChoice.fill(clear);
    check("V10A/diagonal-tie-keeps-fixed-zero-two-split",
          !VertexLighting::shouldFlipDiagonal(diagonalChoice));

    const std::array<float, 4> planar = {0.f, 0.5f, 1.f, 0.5f};
    check("V10A/quad-interpolation-is-deterministic",
          std::abs(VertexLighting::interpolateQuad(
                       planar, false, 0.5f, 0.5f) -
                   0.5f) < 0.001f &&
              std::abs(VertexLighting::interpolateQuad(
                           planar, true, 0.5f, 0.5f) -
                       0.5f) < 0.001f);

    ChunkMesh diagonalMesh;
    const std::array<float, 12> quad = {
        0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
    };
    const std::array<float, 8> texture = {0, 0, 1, 0, 1, 1, 0, 1};
    const std::array<float, 4> light = {0.2f, 0.4f, 0.6f, 0.8f};
    diagonalMesh.addFace(quad, texture, glm::ivec3(0), glm::ivec3(0),
                         light, true);
    check("V10A/four-light-values-use-existing-vertex-stream",
          diagonalMesh.getLight() ==
                  std::vector<float>(light.begin(), light.end()) &&
              TerrainBufferMetrics::VertexStrideBytes == 32);
    check("V10A/flipped-diagonal-emits-stable-indices",
          diagonalMesh.getClientMesh().indices ==
              std::vector<std::uint32_t>({0, 1, 3, 1, 2, 3}));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("vertex_lighting");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockY = 200;
    constexpr int targetX = 8;
    constexpr int targetZ = 8;
    world.setBlock(targetX, blockY, targetZ, BlockId::Stone);
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(blockY / CHUNK_SIZE) : nullptr;
    check("V10A/fixture-section-available", section != nullptr);
    if (section == nullptr) {
        return;
    }

    const auto buildSection = [](ChunkSection &source,
                                 bool ambientOcclusionEnabled = true) {
        SectionMeshInput input;
        source.captureMeshInput(input);
        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes, ambientOcclusionEnabled).buildMesh();
        return meshes;
    };
    const auto findUnitTop = [](const ChunkMeshCollection &meshes, int x,
                                int y, int z,
                                std::array<float, 4> &result) {
        const Mesh &mesh = meshes.solidMesh.getClientMesh();
        const auto &lights = meshes.solidMesh.getLight();
        const std::size_t faceCount = mesh.indices.size() / 6;
        for (std::size_t face = 0; face < faceCount; ++face) {
            const IndexedQuad quad = indexedQuad(mesh, face);
            if (quad.count != 4) {
                continue;
            }
            float minX = 100000.f;
            float maxX = -100000.f;
            float minZ = 100000.f;
            float maxZ = -100000.f;
            bool top = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t position = quad.vertices[vertex] * 3;
                minX = std::min(minX, mesh.vertexPositions[position]);
                maxX = std::max(maxX, mesh.vertexPositions[position]);
                minZ = std::min(minZ, mesh.vertexPositions[position + 2]);
                maxZ = std::max(maxZ, mesh.vertexPositions[position + 2]);
                top = top &&
                      std::abs(mesh.vertexPositions[position + 1] -
                               static_cast<float>(y + 1)) < 0.001f;
            }
            if (top && std::abs(minX - x) < 0.001f &&
                std::abs(maxX - (x + 1)) < 0.001f &&
                std::abs(minZ - z) < 0.001f &&
                std::abs(maxZ - (z + 1)) < 0.001f) {
                for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                    result[vertex] = lights[quad.vertices[vertex]];
                }
                return true;
            }
        }
        return false;
    };

    std::array<float, 4> openTop{};
    const bool foundOpen =
        findUnitTop(buildSection(*section), targetX, blockY, targetZ,
                    openTop);
    check("V10A/isolated-top-has-four-clear-corners",
          foundOpen &&
              std::all_of(openTop.begin(), openTop.end(), [](float value) {
                  return std::abs(value - 1.f) < 0.001f;
              }));

    world.setBlock(targetX - 1, blockY + 1, targetZ, BlockId::Glass);
    world.setBlock(targetX, blockY + 1, targetZ - 1, BlockId::Glass);
    world.setBlock(targetX - 1, blockY + 1, targetZ - 1, BlockId::Glass);
    std::array<float, 4> transparentTop{};
    const bool foundTransparent =
        findUnitTop(buildSection(*section), targetX, blockY, targetZ,
                    transparentTop);
    check("V10A/transparent-neighbours-do-not-occlude-ao",
          foundTransparent && transparentTop == openTop);

    world.setBlock(targetX - 1, blockY + 1, targetZ, BlockId::Stone);
    world.setBlock(targetX, blockY + 1, targetZ - 1, BlockId::Stone);
    world.setBlock(targetX - 1, blockY + 1, targetZ - 1, BlockId::Stone);
    std::array<float, 4> enclosedTop{};
    const bool foundEnclosed =
        findUnitTop(buildSection(*section), targetX, blockY, targetZ,
                    enclosedTop);
    check("V10A/l-shaped-opaque-neighbours-darken-one-corner",
          foundEnclosed &&
              *std::min_element(enclosedTop.begin(), enclosedTop.end()) <
                  0.7f &&
              *std::max_element(enclosedTop.begin(), enclosedTop.end()) >
                  0.99f);

    std::array<float, 4> aoDisabledTop{};
    const bool foundAoDisabled = findUnitTop(
        buildSection(*section, false), targetX, blockY, targetZ,
        aoDisabledTop);
    check("V10A/builder-ao-override-removes-contact-darkening",
          foundAoDisabled &&
              *std::min_element(aoDisabledTop.begin(),
                                aoDisabledTop.end()) > 0.99f);

    constexpr int boundaryX = CHUNK_SIZE;
    constexpr int boundaryZ = 6;
    world.setBlock(boundaryX - 1, blockY, boundaryZ, BlockId::Stone);
    world.setBlock(boundaryX, blockY, boundaryZ, BlockId::Stone);
    world.setBlock(boundaryX, blockY + 1, boundaryZ - 1,
                   BlockId::Stone);
    Chunk *eastChunk = world.getChunkManager().findChunk(1, 0);
    ChunkSection *eastSection =
        eastChunk != nullptr
            ? eastChunk->findSection(blockY / CHUNK_SIZE)
            : nullptr;
    check("V10A/cross-section-fixture-available", eastSection != nullptr);
    if (eastSection == nullptr) {
        return;
    }

    SectionMeshInput westInput;
    SectionMeshInput eastInput;
    section->captureMeshInput(westInput);
    eastSection->captureMeshInput(eastInput);
    ChunkMeshCollection westFirst;
    ChunkMeshCollection eastSecond;
    ChunkMeshBuilder(westInput, westFirst).buildMesh();
    ChunkMeshBuilder(eastInput, eastSecond).buildMesh();
    ChunkMeshCollection eastFirst;
    ChunkMeshCollection westSecond;
    ChunkMeshBuilder(eastInput, eastFirst).buildMesh();
    ChunkMeshBuilder(westInput, westSecond).buildMesh();

    const auto identicalMesh = [](const ChunkMesh &left,
                                   const ChunkMesh &right) {
        return left.getClientMesh().vertexPositions ==
                   right.getClientMesh().vertexPositions &&
               left.getClientMesh().textureCoords ==
                   right.getClientMesh().textureCoords &&
               left.getClientMesh().textureRepeatCoords ==
                   right.getClientMesh().textureRepeatCoords &&
               left.getClientMesh().indices ==
                   right.getClientMesh().indices &&
               left.getLight() == right.getLight();
    };
    check("V10A/rebuild-order-is-byte-deterministic",
          identicalMesh(westFirst.solidMesh, westSecond.solidMesh) &&
              identicalMesh(eastFirst.solidMesh, eastSecond.solidMesh));

    const auto sharedTopLight = [](const ChunkMeshCollection &meshes,
                                   float x, float y, float z,
                                   float &result) {
        const Mesh &mesh = meshes.solidMesh.getClientMesh();
        const auto &lights = meshes.solidMesh.getLight();
        const std::size_t faceCount = mesh.indices.size() / 6;
        for (std::size_t face = 0; face < faceCount; ++face) {
            const IndexedQuad quad = indexedQuad(mesh, face);
            if (quad.count != 4) {
                continue;
            }
            bool top = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                top = top &&
                      std::abs(mesh.vertexPositions[
                                   quad.vertices[vertex] * 3 + 1] -
                               y) < 0.001f;
            }
            if (!top) {
                continue;
            }
            const std::uint32_t first = mesh.indices[face * 6];
            const std::uint32_t second = mesh.indices[face * 6 + 1];
            const std::uint32_t third = mesh.indices[face * 6 + 2];
            const float edgeOneX =
                mesh.vertexPositions[second * 3] -
                mesh.vertexPositions[first * 3];
            const float edgeOneZ =
                mesh.vertexPositions[second * 3 + 2] -
                mesh.vertexPositions[first * 3 + 2];
            const float edgeTwoX =
                mesh.vertexPositions[third * 3] -
                mesh.vertexPositions[first * 3];
            const float edgeTwoZ =
                mesh.vertexPositions[third * 3 + 2] -
                mesh.vertexPositions[first * 3 + 2];
            const float normalY =
                edgeOneZ * edgeTwoX - edgeOneX * edgeTwoZ;
            if (normalY <= 0.f) {
                continue;
            }
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t position = quad.vertices[vertex] * 3;
                if (std::abs(mesh.vertexPositions[position] - x) < 0.001f &&
                    std::abs(mesh.vertexPositions[position + 2] - z) <
                        0.001f) {
                    result = lights[quad.vertices[vertex]];
                    return true;
                }
            }
        }
        return false;
    };
    float westLight = 0.f;
    float eastLight = 0.f;
    const bool foundWest = sharedTopLight(
        westFirst, static_cast<float>(boundaryX),
        static_cast<float>(blockY + 1), static_cast<float>(boundaryZ),
        westLight);
    const bool foundEast = sharedTopLight(
        eastFirst, static_cast<float>(boundaryX),
        static_cast<float>(blockY + 1), static_cast<float>(boundaryZ),
        eastLight);
    check("V10A/cross-section-shared-vertex-matches",
          foundWest && foundEast &&
              std::abs(westLight - eastLight) < 0.000001f,
          "west/east=" + std::to_string(westLight) + "/" +
              std::to_string(eastLight));
}

// ---------------------------------------------------------------------------
// W4 - terrain buffer layout measurement
// ---------------------------------------------------------------------------
void caseTerrainBufferMetrics()
{
    check("W4/terrain-buffer-strides",
          TerrainBufferMetrics::VertexStrideBytes == 32 &&
              TerrainBufferMetrics::IndexStrideBytes == 4,
          "vertex/index=" +
              std::to_string(TerrainBufferMetrics::VertexStrideBytes) +
              "/" +
              std::to_string(TerrainBufferMetrics::IndexStrideBytes));

    TerrainBufferMetrics metrics;
    metrics.add(10, 12);
    check("W4/resident-buffer-estimate",
          metrics.vertexBytes() == 320 &&
              metrics.indexBytes() == 48 &&
              metrics.totalBytes() == 368,
          "vertex/index/total=" +
              std::to_string(metrics.vertexBytes()) + "/" +
              std::to_string(metrics.indexBytes()) + "/" +
              std::to_string(metrics.totalBytes()));
}

// ---------------------------------------------------------------------------
// W1 - world time produces deterministic shader-facing environment values
// ---------------------------------------------------------------------------
void caseWorldEnvironment()
{
    const WorldEnvironmentState dawn = WorldEnvironment::evaluate(0.f);
    const WorldEnvironmentState noon = WorldEnvironment::evaluate(6000.f);
    const WorldEnvironmentState dusk = WorldEnvironment::evaluate(12000.f);
    const WorldEnvironmentState midnight =
        WorldEnvironment::evaluate(18000.f);
    const WorldEnvironmentState wrapped =
        WorldEnvironment::evaluate(30000.f);
    const WorldEnvironmentState negative =
        WorldEnvironment::evaluate(-6000.f);
    constexpr float epsilon = 0.00001f;
    const auto colourIsBounded = [](const glm::vec3 &colour) {
        return colour.r >= 0.f && colour.r <= 1.f &&
               colour.g >= 0.f && colour.g <= 1.f &&
               colour.b >= 0.f && colour.b <= 1.f;
    };

    check("W1/cycle-anchor-phases",
          std::abs(dawn.cycle) < epsilon &&
              std::abs(noon.cycle - 0.25f) < epsilon &&
              std::abs(dusk.cycle - 0.5f) < epsilon &&
              std::abs(midnight.cycle - 0.75f) < epsilon);
    check("W1/cycle-wraps-both-directions",
          std::abs(wrapped.cycle - noon.cycle) < epsilon &&
              std::abs(negative.cycle - midnight.cycle) < epsilon);
    check("W1/daylight-is-bounded-and-darker-at-night",
          noon.daylight <= 1.f && midnight.daylight >= 0.18f &&
              noon.daylight > midnight.daylight + 0.75f);
    check("W1/night-fog-is-denser",
          midnight.fogDensity > noon.fogDensity * 3.f &&
              noon.fogDensity >= 0.0015f - epsilon &&
              midnight.fogDensity <= 0.006f + epsilon);
    check("W1/sky-and-fog-values-change-with-cycle",
          glm::length(noon.skyZenithColour -
                      midnight.skyZenithColour) > 0.5f &&
              glm::length(noon.fogColour - midnight.fogColour) > 0.5f);
    check("W1/sky-horizon-matches-fog",
          glm::length(dawn.skyHorizonColour - dawn.fogColour) < epsilon &&
              glm::length(noon.skyHorizonColour - noon.fogColour) < epsilon &&
              glm::length(midnight.skyHorizonColour -
                          midnight.fogColour) < epsilon);
    check("W1/celestial-cycle-drives-sun-moon-and-stars",
          noon.sunDirection.y > 0.95f &&
              midnight.sunDirection.y < -0.95f &&
              glm::dot(noon.sunDirection,
                       midnight.sunDirection) < -0.99f &&
              noon.sunIntensity > 0.99f &&
              midnight.moonIntensity > 0.99f &&
              noon.starIntensity < 0.01f &&
              midnight.starIntensity > 0.99f);
    check("W1/dawn-and-dusk-light-are-continuous",
          std::abs(dawn.daylight - dusk.daylight) < epsilon &&
              dawn.daylight > midnight.daylight &&
              dawn.daylight < noon.daylight);
    check("FS2/cloud-palette-and-coverage-are-bounded",
          noon.cloudCoverage >= 0.40f && noon.cloudCoverage <= 0.52f &&
              colourIsBounded(noon.cloudLightColour) &&
              colourIsBounded(noon.cloudShadowColour) &&
              glm::length(noon.cloudLightColour -
                          noon.cloudShadowColour) > 0.45f);
    check("FS2/cloud-state-wraps-with-world-time",
          std::abs(wrapped.cloudCoverage - noon.cloudCoverage) < epsilon &&
              glm::length(wrapped.cloudLightColour -
                          noon.cloudLightColour) < epsilon &&
              glm::length(negative.cloudShadowColour -
                          midnight.cloudShadowColour) < epsilon);
    check("FS2/water-palette-has-day-night-and-depth-contrast",
          colourIsBounded(noon.waterShallowColour) &&
              colourIsBounded(noon.waterDeepColour) &&
              glm::length(noon.waterShallowColour -
                          noon.waterDeepColour) > 0.28f &&
              glm::length(noon.waterShallowColour) >
                  glm::length(midnight.waterShallowColour) + 0.35f);
    check("FS2/water-state-wraps-with-world-time",
          glm::length(wrapped.waterShallowColour -
                      noon.waterShallowColour) < epsilon &&
              glm::length(negative.waterDeepColour -
                          midnight.waterDeepColour) < epsilon);

    const glm::vec3 duskSunwardFog =
        WorldEnvironment::directionalFogColour(
            dusk, dusk.sunDirection);
    const glm::vec3 duskBacklitFog =
        WorldEnvironment::directionalFogColour(
            dusk, -dusk.sunDirection);
    check("V10C/directional-fog-is-warmer-toward-low-sun",
          duskSunwardFog.r > duskBacklitFog.r + 0.12f &&
              duskSunwardFog.r > duskSunwardFog.b &&
              glm::length(duskBacklitFog - dusk.fogColour) < epsilon,
          "sunward/backlit red=" +
              std::to_string(duskSunwardFog.r) + "/" +
              std::to_string(duskBacklitFog.r));
    check("V10C/directional-fog-fades-away-from-horizon",
          glm::length(WorldEnvironment::directionalFogColour(
                          dusk, glm::vec3(0.f, 1.f, 0.f)) -
                      dusk.fogColour) < epsilon);
    check("V10C/directional-fog-palette-is-bounded",
          colourIsBounded(dawn.fogSunwardColour) &&
              colourIsBounded(noon.fogSunwardColour) &&
              colourIsBounded(dusk.fogSunwardColour) &&
              colourIsBounded(midnight.fogSunwardColour) &&
              dawn.fogDirectionalStrength >= 0.f &&
              dusk.fogDirectionalStrength <= 1.f);

    check("V10C/cloud-layer-contract-is-bounded",
          WorldEnvironmentState::AtmosphereContractVersion == 1 &&
              noon.cloudBaseHeight >= 128.f &&
              noon.cloudBaseHeight <= 512.f &&
              noon.cloudThickness >= 8.f &&
              noon.cloudThickness <= 64.f &&
              noon.cloudHorizontalScale >= 32.f &&
              noon.cloudHorizontalScale <= 256.f &&
              noon.cloudMaxDistance >= 512.f &&
              noon.cloudMaxDistance <= 5000.f &&
              glm::length(noon.cloudVelocity) > 0.1f &&
              glm::length(noon.cloudVelocity) < 8.f);
    check("V10C/cloud-layer-state-wraps-with-world-time",
          std::abs(wrapped.cloudBaseHeight - noon.cloudBaseHeight) <
                  epsilon &&
              std::abs(wrapped.cloudThickness - noon.cloudThickness) <
                  epsilon &&
              glm::length(wrapped.cloudVelocity - noon.cloudVelocity) <
                  epsilon);

    const float cloudBottom =
        noon.cloudBaseHeight - noon.cloudThickness * 0.5f;
    const float cloudTop =
        noon.cloudBaseHeight + noon.cloudThickness * 0.5f;
    const CloudRayInterval fromBelow =
        WorldEnvironment::cloudRayInterval(
            noon, glm::vec3(0.f, cloudBottom - 32.f, 0.f),
            glm::vec3(0.2f, 1.f, 0.1f));
    const CloudRayInterval fromAbove =
        WorldEnvironment::cloudRayInterval(
            noon, glm::vec3(0.f, cloudTop + 32.f, 0.f),
            glm::vec3(-0.1f, -1.f, 0.2f));
    const CloudRayInterval insideHorizontal =
        WorldEnvironment::cloudRayInterval(
            noon, glm::vec3(0.f, noon.cloudBaseHeight, 0.f),
            glm::vec3(1.f, 0.f, 0.f));
    const CloudRayInterval lookingAway =
        WorldEnvironment::cloudRayInterval(
            noon, glm::vec3(0.f, cloudBottom - 32.f, 0.f),
            glm::vec3(0.f, -1.f, 0.f));
    check("V10C/cloud-ray-covers-below-and-above-views",
          fromBelow.visible && !fromBelow.cameraInside &&
              fromAbove.visible && !fromAbove.cameraInside &&
              fromBelow.farDistance > fromBelow.nearDistance &&
              fromAbove.farDistance > fromAbove.nearDistance);
    check("V10C/cloud-ray-is-stable-inside-layer",
          insideHorizontal.visible && insideHorizontal.cameraInside &&
              std::abs(insideHorizontal.nearDistance) < epsilon &&
              std::abs(insideHorizontal.farDistance -
                       noon.cloudMaxDistance) < epsilon);
    check("V10C/cloud-ray-rejects-away-facing-view",
          !lookingAway.visible && !lookingAway.cameraInside);

    const glm::vec2 oneStep =
        WorldEnvironment::cloudOffset(noon, 10.f);
    const glm::vec2 splitSteps =
        WorldEnvironment::cloudOffset(noon, 4.f) +
        WorldEnvironment::cloudOffset(noon, 6.f);
    check("V10C/cloud-motion-is-frame-rate-independent",
          glm::length(oneStep - splitSteps) < epsilon &&
              glm::length(WorldEnvironment::cloudOffset(noon, -1.f)) <
                  epsilon);
}

// ---------------------------------------------------------------------------
// L4 - transparent cubes use explicit passes and cull only shared media
// ---------------------------------------------------------------------------
void caseTransparentBlockRules()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("transparent_block_rules");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockY = 200;
    constexpr int sectionY = blockY / CHUNK_SIZE;
    world.setBlock(4, blockY, 4, BlockId::Glass);
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
    check("L4/section-available", section != nullptr);
    if (section == nullptr) {
        return;
    }

    const auto buildSectionMeshes = [](ChunkSection &target) {
        SectionMeshInput input;
        target.captureMeshInput(input);
        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes).buildMesh();
        return meshes;
    };

    const auto &glass =
        BlockDatabase::get().getDefinition(BlockId::Glass);
    const auto &borderless =
        BlockDatabase::get().getDefinition(BlockId::GlassBorderless);
    const auto &leaves =
        BlockDatabase::get().getDefinition(BlockId::OakLeaf);
    check("L4/definitions-select-transparent-rules",
          glass.transparent && borderless.transparent &&
              glass.collidable && borderless.collidable &&
              glass.render.shaderType == BlockShaderType::Transparent &&
              borderless.render.shaderType ==
                  BlockShaderType::Transparent &&
              leaves.transparent &&
              leaves.render.shaderType == BlockShaderType::Chunk);
    check("L4/glass-material-roundtrip",
          Material::GLASS_BLOCK.toBlockID() == BlockId::Glass &&
              Material::BORDERLESS_GLASS_BLOCK.toBlockID() ==
                  BlockId::GlassBorderless &&
              Material::toMaterial(BlockId::Glass).id ==
                  Material::ID::Glass &&
              Material::toMaterial(BlockId::GlassBorderless).id ==
                  Material::ID::GlassBorderless);

    world.setBlock(5, blockY, 4, BlockId::Glass);
    ChunkMeshCollection sameGlass = buildSectionMeshes(*section);
    const ChunkDebugStats glassMetricsBefore =
        world.getChunkManager().collectDebugStats();
    world.getChunkManager().recordMeshRebuild(sameGlass, 0.0);
    const ChunkDebugStats glassMetricsAfter =
        world.getChunkManager().collectDebugStats();
    check("L4/same-glass-culls-shared-face",
          sameGlass.transparentMesh.faces == 10 &&
              sameGlass.solidMesh.faces == 0 &&
              sameGlass.waterMesh.faces == 0 &&
              sameGlass.floraMesh.faces == 0 &&
              glassMetricsAfter.transparentFaces ==
                  glassMetricsBefore.transparentFaces + 10 &&
              glassMetricsAfter.transparentVertices ==
                  glassMetricsBefore.transparentVertices + 40,
          "transparent=" +
              std::to_string(sameGlass.transparentMesh.faces));

    world.setBlock(5, blockY, 4, BlockId::GlassBorderless);
    ChunkMeshCollection mixedGlass = buildSectionMeshes(*section);
    check("L4/glass-variants-cull-shared-face",
          mixedGlass.transparentMesh.faces == 10,
          "transparent=" +
              std::to_string(mixedGlass.transparentMesh.faces));

    world.setBlock(4, blockY, 4, BlockId::Stone);
    ChunkMeshCollection solidAgainstGlass = buildSectionMeshes(*section);
    check("L4/opaque-glass-interface-has-one-face",
          solidAgainstGlass.solidMesh.faces == 6 &&
              solidAgainstGlass.transparentMesh.faces == 5,
          "solid=" + std::to_string(solidAgainstGlass.solidMesh.faces) +
              " transparent=" +
              std::to_string(solidAgainstGlass.transparentMesh.faces));

    world.setBlock(4, blockY, 4, BlockId::OakLeaf);
    world.setBlock(5, blockY, 4, BlockId::OakLeaf);
    ChunkMeshCollection leafPair = buildSectionMeshes(*section);
    check("L4/leaves-use-static-cutout-pass",
          leafPair.solidMesh.faces == 10 &&
              leafPair.transparentMesh.faces == 0 &&
              leafPair.floraMesh.faces == 0,
          "solid=" + std::to_string(leafPair.solidMesh.faces));

    world.setBlock(5, blockY, 4, BlockId::TallGrass);
    ChunkMeshCollection leafAndFlora = buildSectionMeshes(*section);
    check("L4/flora-does-not-occlude-leaf-face",
          leafAndFlora.solidMesh.faces == 6 &&
              leafAndFlora.floraMesh.faces == 2,
          "solid=" + std::to_string(leafAndFlora.solidMesh.faces) +
              " flora=" +
              std::to_string(leafAndFlora.floraMesh.faces));

    world.setBlock(4, blockY, 4, BlockId::Water);
    world.setBlock(5, blockY, 4, BlockId::Water);
    ChunkMeshCollection waterPair = buildSectionMeshes(*section);
    check("L4/water-keeps-own-transparent-pass",
          waterPair.waterMesh.faces == 10 &&
              waterPair.transparentMesh.faces == 0 &&
              waterPair.solidMesh.faces == 0 &&
              waterPair.floraMesh.faces == 0,
          "water=" + std::to_string(waterPair.waterMesh.faces));

    world.setBlock(4, blockY, 4, BlockId::Air);
    world.setBlock(5, blockY, 4, BlockId::Air);
    world.setBlock(15, blockY, 8, BlockId::Glass);
    world.setBlock(16, blockY, 8, BlockId::GlassBorderless);
    Chunk *eastChunk = world.getChunkManager().findChunk(1, 0);
    ChunkSection *eastSection =
        eastChunk != nullptr ? eastChunk->findSection(sectionY) : nullptr;
    const int westFaces = buildSectionMeshes(*section).transparentMesh.faces;
    const int eastFaces = eastSection != nullptr
                              ? buildSectionMeshes(*eastSection)
                                    .transparentMesh.faces
                              : -1;
    check("L4/cross-chunk-glass-culls-shared-face",
          westFaces == 5 && eastFaces == 5,
          "west=" + std::to_string(westFaces) +
              " east=" + std::to_string(eastFaces));
}

// ---------------------------------------------------------------------------
// C1 - block-specific behavior is registered beside the definition
// ---------------------------------------------------------------------------
void caseBlockBehaviorDispatch()
{
    const auto &database = BlockDatabase::get();
    bool allDefinitionsHaveBehavior = true;
    for (int id = 0; id < static_cast<int>(BlockId::NUM_TYPES); ++id) {
        allDefinitionsHaveBehavior =
            allDefinitionsHaveBehavior &&
            database.getDefinition(static_cast<BlockId>(id)).behavior !=
                nullptr;
    }
    check("C1/all-definitions-have-behavior",
          allDefinitionsHaveBehavior);

    const ChunkBlock dirt(BlockId::Dirt);
    const auto &dirtDefinition =
        database.getDefinition(BlockId::Dirt);
    check("C1/default-behavior-preserves-drop",
          dirtDefinition.behavior->getDrop(dirtDefinition, dirt) ==
              Material::ID::Dirt);

    const ChunkBlock glass(BlockId::Glass);
    const auto &glassDefinition =
        database.getDefinition(BlockId::Glass);
    check("C1/special-behavior-overrides-drop",
          glassDefinition.defaultDrop == Material::ID::Glass &&
              glassDefinition.behavior->getDrop(glassDefinition, glass) ==
                  Material::ID::Nothing);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("block_behavior_dispatch");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int y = 100;
    world.setBlock(8, y, 8, BlockId::Glass);
    const std::size_t actorsBefore =
        world.collectDebugStats().actorCount;
    const bool broke = BlockInteractionSystem::breakBlock(
        world, player, {8.5f, static_cast<float>(y) + 0.5f, 8.5f});
    check("C1/interaction-dispatches-special-drop",
          broke && player.getHeldItems().isEmpty() &&
              world.collectDebugStats().actorCount == actorsBefore &&
              world.getBlock(8, y, 8) == BlockId::Air);
}

// ---------------------------------------------------------------------------
// C2 - block metadata selects behavior without allocating another block id
// ---------------------------------------------------------------------------
void caseMetadataBackedBehavior()
{
    const auto &definition =
        BlockDatabase::get().getDefinition(BlockId::TallGrass);
    const ChunkBlock immature(
        BlockId::TallGrass, BlockMetadata::TallGrass::Immature);
    const ChunkBlock mature(
        BlockId::TallGrass, BlockMetadata::TallGrass::Mature);
    check("C2/same-id-metadata-selects-drop",
          immature.id == mature.id &&
              definition.behavior->getDrop(definition, immature) ==
                  Material::ID::Nothing &&
              definition.behavior->getDrop(definition, mature) ==
                  Material::ID::TallGrass);

    TemperateForestBiome biome(kValidationSeed);
    Rand random(kValidationSeed);
    const ChunkBlock naturalPlant = biome.getPlant(random);
    check("C2/natural-tall-grass-is-mature",
          naturalPlant == mature);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("metadata_behavior");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int y = 100;
    world.setBlock(8, y, 8, immature);
    const std::size_t actorsBefore =
        world.collectDebugStats().actorCount;
    const bool brokeImmature = BlockInteractionSystem::breakBlock(
        world, player, {8.5f, static_cast<float>(y) + 0.5f, 8.5f});
    check("C2/immature-break-has-no-drop",
          brokeImmature && player.getHeldItems().isEmpty() &&
              world.collectDebugStats().actorCount == actorsBefore);

    world.setBlock(8, y, 8, mature);
    const bool brokeMature = BlockInteractionSystem::breakBlock(
        world, player, {8.5f, static_cast<float>(y) + 0.5f, 8.5f});
    check("C2/mature-break-drops-tall-grass",
          brokeMature &&
              player.getHeldItems().getMaterial().id ==
                  Material::ID::TallGrass &&
              player.getHeldItems().getNumInStack() == 1);
}

// ---------------------------------------------------------------------------
// C7 - random ticks visit only indexed sections with bounded round-robin work
// ---------------------------------------------------------------------------
void caseRandomTickScheduling()
{
    const auto &definition =
        BlockDatabase::get().getDefinition(BlockId::TallGrass);
    const ChunkBlock immature(
        BlockId::TallGrass, BlockMetadata::TallGrass::Immature);
    const ChunkBlock mature(
        BlockId::TallGrass, BlockMetadata::TallGrass::Mature);
    check("C7/metadata-selects-random-tick",
          definition.behavior != nullptr &&
              definition.behavior->receivesRandomTicks(definition,
                                                         immature) &&
              !definition.behavior->receivesRandomTicks(definition,
                                                          mature));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("random_tick_scheduling");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int sectionCount = 5;
    const auto sampledPosition = [](int chunkX, int sectionY, int chunkZ,
                                    int tick) {
        const std::size_t sample = World::randomTickBlockIndex(
            kValidationSeed, tick, {chunkX, sectionY, chunkZ}, 0);
        const int localY = static_cast<int>(sample / CHUNK_AREA);
        const int remainder = static_cast<int>(sample % CHUNK_AREA);
        const int localZ = remainder / CHUNK_SIZE;
        const int localX = remainder % CHUNK_SIZE;
        return glm::ivec3{chunkX * CHUNK_SIZE + localX,
                          sectionY * CHUNK_SIZE + localY,
                          chunkZ * CHUNK_SIZE + localZ};
    };
    std::array<glm::ivec3, sectionCount> positions;
    for (int index = 0; index < sectionCount; ++index) {
        const int tick = index < 4 ? 100 : 101;
        positions[index] = sampledPosition(0, 10 + index, 0, tick);
        world.setBlock(positions[index].x, positions[index].y,
                       positions[index].z, immature);
    }

    const WorldDebugStats indexed = world.collectDebugStats();
    check("C7/indexes-only-tickable-sections",
          indexed.randomTickSections == sectionCount &&
              indexed.randomTickBlocks == sectionCount,
          "sections=" + std::to_string(indexed.randomTickSections) +
              " blocks=" + std::to_string(indexed.randomTickBlocks));

    world.tick(100);
    int grown = 0;
    for (int index = 0; index < sectionCount; ++index) {
        if (world.getBlock(positions[index].x, positions[index].y,
                           positions[index].z) == mature) {
            ++grown;
        }
    }
    const WorldDebugStats firstTick = world.collectDebugStats();
    check("C7/section-budget-is-bounded",
          grown == static_cast<int>(World::RandomTickSectionBudgetPerTick) &&
              firstTick.randomTickSectionsProcessed ==
                  World::RandomTickSectionBudgetPerTick &&
              firstTick.randomTickSections == 1 &&
              firstTick.randomTickBlocks == 1,
          "grown=" + std::to_string(grown) +
              " processed=" +
              std::to_string(firstTick.randomTickSectionsProcessed));

    world.tick(101);
    const WorldDebugStats drained = world.collectDebugStats();
    check("C7/round-robin-drains-remaining-section",
          world.getBlock(positions[4].x, positions[4].y,
                         positions[4].z) == mature &&
              drained.randomTickSections == 0 &&
              drained.randomTickBlocks == 0 &&
              drained.randomTicksDispatched == sectionCount,
          "active=" + std::to_string(drained.randomTickSections) +
              " dispatched=" +
              std::to_string(drained.randomTicksDispatched));

    constexpr int persistedChunkX = 4;
    constexpr int persistedChunkZ = 0;
    constexpr int persistedSectionY = 6;
    const glm::ivec3 persistedPosition = sampledPosition(
        persistedChunkX, persistedSectionY, persistedChunkZ, 102);
    world.getChunkManager().loadChunk(persistedChunkX, persistedChunkZ);
    world.setBlock(persistedPosition.x, persistedPosition.y,
                   persistedPosition.z, immature);
    world.getChunkManager().unloadChunk(persistedChunkX, persistedChunkZ);
    const WorldDebugStats unloaded = world.collectDebugStats();
    check("C7/unload-removes-section-index",
          unloaded.randomTickSections == 0 &&
              unloaded.randomTickBlocks == 0);

    world.getChunkManager().loadChunk(persistedChunkX, persistedChunkZ);
    const WorldDebugStats reloaded = world.collectDebugStats();
    world.tick(102);
    check("C7/reload-restores-section-index",
          reloaded.randomTickSections == 1 &&
              reloaded.randomTickBlocks == 1 &&
              world.getBlock(persistedPosition.x, persistedPosition.y,
                             persistedPosition.z) == mature,
          "sections=" + std::to_string(reloaded.randomTickSections) +
              " blocks=" + std::to_string(reloaded.randomTickBlocks));
}

// ---------------------------------------------------------------------------
// C3 - non-cube geometry is loaded from named shape resources
// ---------------------------------------------------------------------------
void caseResourceDrivenBlockShapes()
{
    const auto &database = BlockDatabase::get();
    const auto &tallGrass =
        database.getDefinition(BlockId::TallGrass).render;
    const auto &rose = database.getDefinition(BlockId::Rose).render;
    const auto &deadShrub =
        database.getDefinition(BlockId::DeadShrub).render;
    check("C3/flora-definitions-use-resource-shape",
          tallGrass.meshType == BlockMeshType::Resource &&
              rose.meshType == BlockMeshType::Resource &&
              deadShrub.meshType == BlockMeshType::Resource &&
              tallGrass.shape.name == "Cross" &&
              rose.shape.name == "Cross" &&
              deadShrub.shape.name == "Cross");

    const BlockShapeFace expectedFirst{
        0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0,
    };
    const BlockShapeFace expectedSecond{
        0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
    };
    check("C3/cross-shape-loads-resource-vertices",
          tallGrass.shape.faces.size() == 2 &&
              tallGrass.shape.faces[0] == expectedFirst &&
              tallGrass.shape.faces[1] == expectedSecond);

    const std::filesystem::path fixtureRoot =
        freshSaveDirectory("resource_shape_fixture");
    const std::filesystem::path blockDirectory = fixtureRoot / "blocks";
    const std::filesystem::path shapeDirectory = fixtureRoot / "shapes";
    std::filesystem::create_directories(blockDirectory);
    std::filesystem::create_directories(shapeDirectory);
    {
        std::ofstream shape(shapeDirectory / "SingleQuad.shape",
                            std::ios::binary | std::ios::trunc);
        shape << "Face\n0 0 0 1 0 0 1 1 0 0 1 0\n";
    }
    {
        std::ofstream block(blockDirectory / "Fixture.block",
                            std::ios::binary | std::ios::trunc);
        block << "Name\nFixture\n\n"
              << "Id\n10\n\n"
              << "TexAll\n11 0\n\n"
              << "Opaque\n0\n\n"
              << "MeshType\n1\n\n"
              << "Shape\nSingleQuad\n\n"
              << "ShaderType\n2\n\n"
              << "Light\n0\n\n"
              << "Collidable\n0\n";
    }
    const BlockData fixture("Fixture", blockDirectory.string(),
                            shapeDirectory.string());
    const auto &fixtureShape = fixture.getBlockData().shape;
    check("C3/new-resource-shape-needs-no-builder-change",
          fixtureShape.name == "SingleQuad" &&
              fixtureShape.faces.size() == 1 &&
              fixtureShape.faces.front() == BlockShapeFace{
                  0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
              });

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("resource_shape_mesh");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockY = 200;
    world.setBlock(4, blockY, 4,
                   ChunkBlock(BlockId::TallGrass,
                              BlockMetadata::TallGrass::Mature));
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(blockY / CHUNK_SIZE) : nullptr;
    ChunkMeshCollection meshes;
    if (section != nullptr) {
        SectionMeshInput input;
        section->captureMeshInput(input);
        ChunkMeshBuilder(input, meshes).buildMesh();
    }
    check("C3/resource-shape-builds-flora-mesh",
          section != nullptr && meshes.floraMesh.faces == 2 &&
              meshes.floraMesh.getClientMesh().vertexPositions.size() ==
                  24 &&
              meshes.solidMesh.faces == 0 &&
              meshes.waterMesh.faces == 0 &&
              meshes.transparentMesh.faces == 0);
}

// ---------------------------------------------------------------------------
// L1 - sunlight is stored per voxel, captured with mesh inputs and included in
// the greedy merge key so a cave face cannot borrow the surface brightness
// ---------------------------------------------------------------------------
void caseSunlightStorage()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("sunlight_storage");
    Config config = makeConfig();
    Camera camera(config);

    constexpr int floorY = 200;
    constexpr int sectionY = floorY / CHUNK_SIZE;
    constexpr int z = 4;
    constexpr int surfaceX = 4;
    constexpr int caveX = 5;

    check("L1/light-level-conversion-bounded",
          std::abs(lightLevelToBrightness(MIN_LIGHT_LEVEL) -
                   MIN_TERRAIN_BRIGHTNESS) < 0.001f &&
              std::abs(lightLevelToBrightness(MAX_LIGHT_LEVEL) - 1.f) <
                  0.001f &&
              std::abs(lightLevelToBrightness(
                           static_cast<LightLevel>(255)) -
                       1.f) < 0.001f);

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(surfaceX, floorY, z, BlockId::Stone);
        world.setBlock(caveX, floorY, z, BlockId::Stone);
        world.setBlock(caveX, floorY + 2, z, BlockId::Stone);

        Chunk *chunk = world.getChunkManager().findChunk(0, 0);
        ChunkSection *section =
            chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
        check("L1/high-section-available", section != nullptr);
        if (chunk == nullptr || section == nullptr) {
            return;
        }

        chunk->rebuildSunlight();
        check("L1/open-column-has-full-sunlight",
              world.getSunlight(surfaceX, floorY + 1, z) ==
                  MAX_LIGHT_LEVEL,
              "level=" + std::to_string(world.getSunlight(
                               surfaceX, floorY + 1, z)));
        check("L1/roofed-column-has-no-direct-sunlight",
              world.getSunlight(caveX, floorY + 1, z) == MIN_LIGHT_LEVEL &&
                  world.getSunlight(caveX, floorY + 3, z) ==
                      MAX_LIGHT_LEVEL,
              "below/above=" +
                  std::to_string(
                      world.getSunlight(caveX, floorY + 1, z)) +
                  "/" +
                  std::to_string(
                      world.getSunlight(caveX, floorY + 3, z)));

        SectionMeshInput input;
        section->captureMeshInput(input);
        int sunlightMismatches = 0;
        for (int y = -1; y <= CHUNK_SIZE; ++y) {
            for (int localZ = -1; localZ <= CHUNK_SIZE; ++localZ) {
                for (int localX = -1; localX <= CHUNK_SIZE; ++localX) {
                    const int worldY = sectionY * CHUNK_SIZE + y;
                    if (input.getSunlight(localX, y, localZ) !=
                        world.getSunlight(localX, worldY, localZ)) {
                        ++sunlightMismatches;
                    }
                }
            }
        }
        check("L1/snapshot-halo-carries-sunlight",
              sunlightMismatches == 0,
              "mismatches=" + std::to_string(sunlightMismatches) +
                  " over " + std::to_string(SectionMeshInput::Volume) +
                  " cells");

        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes).buildMesh();
        const Mesh &solid = meshes.solidMesh.getClientMesh();
        const auto &light = meshes.solidMesh.getLight();
        int floorTopFaces = 0;
        bool foundSurfaceLight = false;
        float darkestTopLight = 1.f;
        float brightestTopLight = 0.f;
        const std::size_t faceCount = solid.indices.size() / 6;
        for (std::size_t face = 0; face < faceCount; ++face) {
            const IndexedQuad quad = indexedQuad(solid, face);
            if (quad.count != 4) {
                continue;
            }
            bool isFloorTop = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t positionIndex =
                    quad.vertices[vertex] * 3 + 1;
                isFloorTop =
                    isFloorTop &&
                    std::abs(solid.vertexPositions[positionIndex] -
                             static_cast<float>(floorY + 1)) < 0.001f;
            }
            if (!isFloorTop) {
                continue;
            }

            ++floorTopFaces;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const float vertexLight = light[quad.vertices[vertex]];
                darkestTopLight = std::min(darkestTopLight, vertexLight);
                brightestTopLight =
                    std::max(brightestTopLight, vertexLight);
                foundSurfaceLight =
                    foundSurfaceLight ||
                    std::abs(vertexLight - 1.f) < 0.001f;
            }
        }
        check("L1/mesh-distinguishes-surface-and-cave",
              foundSurfaceLight && darkestTopLight < 0.85f &&
                  brightestTopLight - darkestTopLight > 0.15f,
              "bright/dark=" + std::to_string(brightestTopLight) +
                  "/" + std::to_string(darkestTopLight));
        check("L1/greedy-splits-light-boundary", floorTopFaces == 2,
              "top_faces=" + std::to_string(floorTopFaces));

        check("L1/sunlight-fixture-saves", world.save());
    }

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("L1/load-rebuilds-derived-sunlight",
              world.getSunlight(surfaceX, floorY + 1, z) ==
                      MAX_LIGHT_LEVEL &&
                  world.getSunlight(caveX, floorY + 1, z) ==
                      MIN_LIGHT_LEVEL,
              "surface/cave=" +
                  std::to_string(
                      world.getSunlight(surfaceX, floorY + 1, z)) +
                  "/" +
                  std::to_string(
                      world.getSunlight(caveX, floorY + 1, z)));
    }
}

// ---------------------------------------------------------------------------
// L2 - emissive block data is propagated into per-voxel block light, captured
// by mesh snapshots and combined with sunlight at visible faces
// ---------------------------------------------------------------------------
void caseBlockLightStorage()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("block_light_storage");
    Config config = makeConfig();
    Camera camera(config);

    constexpr int floorY = 200;
    constexpr int sectionY = floorY / CHUNK_SIZE;
    constexpr int sourceX = 4;
    constexpr int targetX = 5;
    constexpr int z = 4;

    check("P11-0/static-emissive-definitions-are-data-driven",
          BlockDatabase::get().getDefinition(BlockId::Rose).light == 0 &&
              BlockDatabase::get().getDefinition(BlockId::Torch).light == 14,
          "rose/torch=" + std::to_string(
              BlockDatabase::get().getDefinition(BlockId::Rose).light) +
              "/" + std::to_string(
              BlockDatabase::get().getDefinition(BlockId::Torch).light));

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(sourceX, floorY, z, BlockId::Stone);
        world.setBlock(targetX, floorY, z, BlockId::Stone);
        world.setBlock(sourceX, floorY + 1, z, BlockId::Torch);
        world.setBlock(6, floorY + 1, z, BlockId::Stone);
        for (int x = 2; x <= 7; ++x) {
            world.setBlock(x, floorY + 3, z, BlockId::Stone);
        }

        Chunk *chunk = world.getChunkManager().findChunk(0, 0);
        ChunkSection *section =
            chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
        if (chunk == nullptr || section == nullptr) {
            check("L2/source-and-distance-falloff", false,
                  "fixture section missing");
            return;
        }

        chunk->rebuildSunlight();
        chunk->rebuildBlockLight();
        check("L2/source-and-distance-falloff",
              world.getBlockLight(sourceX, floorY + 1, z) == 14 &&
                  world.getBlockLight(targetX, floorY + 1, z) == 13 &&
                  world.getBlockLight(2, floorY + 1, z) == 12,
              "source/one/two=" +
                  std::to_string(
                      world.getBlockLight(sourceX, floorY + 1, z)) +
                  "/" +
                  std::to_string(
                      world.getBlockLight(targetX, floorY + 1, z)) +
                  "/" +
                  std::to_string(world.getBlockLight(2, floorY + 1, z)));
        check("L2/opaque-block-stops-light",
              world.getBlockLight(6, floorY + 1, z) == MIN_LIGHT_LEVEL,
              "level=" +
                  std::to_string(world.getBlockLight(6, floorY + 1, z)));

        SectionMeshInput input;
        section->captureMeshInput(input);
        int blockLightMismatches = 0;
        for (int y = -1; y <= CHUNK_SIZE; ++y) {
            for (int localZ = -1; localZ <= CHUNK_SIZE; ++localZ) {
                for (int localX = -1; localX <= CHUNK_SIZE; ++localX) {
                    const int worldY = sectionY * CHUNK_SIZE + y;
                    if (input.getBlockLight(localX, y, localZ) !=
                        world.getBlockLight(localX, worldY, localZ)) {
                        ++blockLightMismatches;
                    }
                }
            }
        }
        check("L2/snapshot-halo-carries-block-light",
              blockLightMismatches == 0,
              "mismatches=" + std::to_string(blockLightMismatches) +
                  " over " + std::to_string(SectionMeshInput::Volume) +
                  " cells");
        check("L2/combined-light-prefers-strongest-source",
              input.getCombinedLight(targetX, floorY + 1 -
                                                  sectionY * CHUNK_SIZE,
                                     z) == 13 &&
                  input.getCombinedLight(8, floorY + 1 -
                                                 sectionY * CHUNK_SIZE,
                                         z) == MAX_LIGHT_LEVEL);

        ChunkMeshCollection meshes;
        ChunkMeshBuilder(input, meshes).buildMesh();
        const Mesh &solid = meshes.solidMesh.getClientMesh();
        const auto &light = meshes.solidMesh.getLight();
        bool foundLitTargetFace = false;
        const std::size_t faceCount = solid.indices.size() / 6;
        for (std::size_t face = 0; face < faceCount; ++face) {
            const IndexedQuad quad = indexedQuad(solid, face);
            if (quad.count != 4) {
                continue;
            }
            float minX = 100000.f;
            float maxX = -100000.f;
            float minZ = 100000.f;
            float maxZ = -100000.f;
            bool topHeight = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t index = quad.vertices[vertex] * 3;
                minX = std::min(minX, solid.vertexPositions[index]);
                maxX = std::max(maxX, solid.vertexPositions[index]);
                minZ = std::min(minZ, solid.vertexPositions[index + 2]);
                maxZ = std::max(maxZ, solid.vertexPositions[index + 2]);
                topHeight =
                    topHeight &&
                    std::abs(solid.vertexPositions[index + 1] -
                             static_cast<float>(floorY + 1)) < 0.001f;
            }

            const bool coversTarget =
                minX <= targetX + 0.001f &&
                maxX >= targetX + 1.f - 0.001f &&
                minZ <= z + 0.001f && maxZ >= z + 1.f - 0.001f;
            if (topHeight && coversTarget) {
                for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                    foundLitTargetFace =
                        foundLitTargetFace ||
                        light[quad.vertices[vertex]] >=
                            lightLevelToBrightness(11);
                }
            }
        }
        check("L2/emissive-block-lights-nearby-mesh",
              foundLitTargetFace);

        world.save();
    }

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("L2/load-rebuilds-derived-block-light",
              world.getBlockLight(sourceX, floorY + 1, z) == 14 &&
                  world.getBlockLight(targetX, floorY + 1, z) == 13,
              "source/one=" +
                  std::to_string(
                      world.getBlockLight(sourceX, floorY + 1, z)) +
                  "/" +
                  std::to_string(
                      world.getBlockLight(targetX, floorY + 1, z)));
    }
}

// ---------------------------------------------------------------------------
// L3 - block edits update one sunlight column and locally remove/re-propagate
// block light across loaded chunk boundaries
// ---------------------------------------------------------------------------
void caseLocalRelightAfterEdits()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("local_relight");
    Config config = makeConfig();
    Camera camera(config);

    constexpr int sourceX = 15;
    constexpr int neighbourX = 16;
    constexpr int sourceY = 201;
    constexpr int z = 8;
    constexpr int sectionY = sourceY / CHUNK_SIZE;

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(sourceX, sourceY - 1, z, BlockId::Stone);
        world.setBlock(neighbourX, sourceY - 1, z, BlockId::Stone);

        for (int attempt = 0; attempt < 16 &&
                              world.collectDebugStats().queuedChunkUpdates > 0;
             ++attempt) {
            world.update(camera);
        }

        Chunk *neighbourChunk =
            world.getChunkManager().findChunk(1, 0);
        ChunkSection *neighbourSection =
            neighbourChunk != nullptr
                ? neighbourChunk->findSection(sectionY)
                : nullptr;
        const std::uint32_t revisionBefore =
            neighbourSection != nullptr
                ? neighbourSection->getBlockRevision()
                : 0;

        world.setBlock(sourceX, sourceY, z, BlockId::Torch);
        check("L3/emissive-edit-crosses-chunk-boundary",
              world.getBlockLight(sourceX, sourceY, z) == 14 &&
                  world.getBlockLight(neighbourX, sourceY, z) == 13,
              "source/neighbour=" +
                  std::to_string(
                      world.getBlockLight(sourceX, sourceY, z)) +
                  "/" +
                  std::to_string(
                      world.getBlockLight(neighbourX, sourceY, z)));
        check("L3/light-only-section-revision-advances",
              neighbourSection != nullptr &&
                  neighbourSection->getBlockRevision() != revisionBefore &&
                  (neighbourSection->getMeshState() ==
                       ChunkMeshState::Dirty ||
                   neighbourSection->getMeshState() ==
                       ChunkMeshState::Queued));

        const WorldDebugStats relightStats = world.collectDebugStats();
        check("L3/local-relight-queues-bounded-sections",
              relightStats.queuedChunkUpdates > 0 &&
                  relightStats.queuedChunkUpdates <= 12,
              "queued=" +
                  std::to_string(relightStats.queuedChunkUpdates) +
                  " sections=" +
                  std::to_string(relightStats.chunks.sections));

        world.setBlock(neighbourX, sourceY, z, BlockId::Stone);
        check("L3/opaque-edit-removes-block-light",
              world.getBlockLight(neighbourX, sourceY, z) ==
                  MIN_LIGHT_LEVEL,
              "level=" + std::to_string(
                               world.getBlockLight(neighbourX, sourceY, z)));
        world.setBlock(neighbourX, sourceY, z, BlockId::Air);
        check("L3/removing-opaque-block-restores-light",
              world.getBlockLight(neighbourX, sourceY, z) == 13,
              "level=" + std::to_string(
                               world.getBlockLight(neighbourX, sourceY, z)));

        constexpr int columnX = 4;
        constexpr int columnZ = 4;
        constexpr int floorY = 200;
        world.setBlock(columnX, floorY, columnZ, BlockId::Stone);
        const LightLevel openSunlight =
            world.getSunlight(columnX, floorY + 1, columnZ);
        world.setBlock(columnX, floorY + 2, columnZ, BlockId::Stone);
        check("L3/opaque-edit-updates-one-sunlight-column",
              openSunlight == MAX_LIGHT_LEVEL &&
                  world.getSunlight(columnX, floorY + 1, columnZ) ==
                      MIN_LIGHT_LEVEL &&
                  world.getSunlight(columnX + 1, floorY + 1, columnZ) ==
                      MAX_LIGHT_LEVEL);
        world.setBlock(columnX, floorY + 2, columnZ, BlockId::Air);
        check("L3/removing-roof-restores-sunlight",
              world.getSunlight(columnX, floorY + 1, columnZ) ==
                  MAX_LIGHT_LEVEL);

        constexpr int sectionBoundaryY = 207;
        world.setBlock(8, sectionBoundaryY + 1, 12, BlockId::Stone);
        world.setBlock(8, sectionBoundaryY + 1, 12, BlockId::Air);
        world.setBlock(8, sectionBoundaryY, 12, BlockId::Torch);
        check("P11-0/emissive-edit-crosses-section-boundary",
              world.getBlockLight(8, sectionBoundaryY, 12) == 14 &&
                  world.getBlockLight(8, sectionBoundaryY + 1, 12) == 13);
        world.setBlock(8, sectionBoundaryY, 12, BlockId::Air);

        world.save();
        world.getChunkManager().unloadChunk(0, 0);
        check("L3/unloading-source-removes-cross-chunk-light",
              world.getBlockLight(neighbourX, sourceY, z) ==
                  MIN_LIGHT_LEVEL,
              "level=" + std::to_string(
                               world.getBlockLight(neighbourX, sourceY, z)));
    }

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("L3/load-reconciles-cross-chunk-block-light",
              world.getBlockLight(sourceX, sourceY, z) == 14 &&
                  world.getBlockLight(neighbourX, sourceY, z) == 13,
              "source/neighbour=" +
                  std::to_string(
                      world.getBlockLight(sourceX, sourceY, z)) +
                  "/" +
                  std::to_string(
                      world.getBlockLight(neighbourX, sourceY, z)));
    }
}

// ---------------------------------------------------------------------------
// M6 - sections sealed by opaque neighbours complete without a mesh build
// ---------------------------------------------------------------------------
void caseEnclosedSectionSkip()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("enclosed_section_skip");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();

    constexpr int sectionY = 12;
    constexpr int baseY = sectionY * CHUNK_SIZE;
    const std::array<glm::ivec2, 5> solidChunks{{
        {0, 0},
        {-1, 0},
        {1, 0},
        {0, -1},
        {0, 1},
    }};

    for (const auto &chunkPosition : solidChunks) {
        const int baseX = chunkPosition.x * CHUNK_SIZE;
        const int baseZ = chunkPosition.y * CHUNK_SIZE;
        for (int y = baseY; y < baseY + CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    world.setBlock(baseX + x, y, baseZ + z, BlockId::Stone);
                }
            }
        }
    }
    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            world.setBlock(x, baseY - 1, z, BlockId::Stone);
            world.setBlock(x, baseY + CHUNK_SIZE, z, BlockId::Stone);
        }
    }

    Chunk *chunk = chunks.findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
    check("M6/enclosed-section-available", section != nullptr);
    if (section == nullptr) {
        return;
    }

    SectionMeshInput enclosedInput;
    section->captureMeshInput(enclosedInput);
    check("M6/enclosed-snapshot-skips-build",
          !enclosedInput.needsMeshBuild());

    ChunkMeshCollection directMeshes;
    ChunkMeshBuilder(enclosedInput, directMeshes).buildMesh();
    check("M6/enclosed-builder-emits-nothing",
          directMeshes.solidMesh.faces == 0 &&
              directMeshes.transparentMesh.faces == 0 &&
              directMeshes.waterMesh.faces == 0 &&
              directMeshes.floraMesh.faces == 0);

    const ChunkDebugStats beforeSkip = chunks.collectDebugStats();
    ChunkMeshJob skippedJob;
    const ChunkMeshWorkResult skipped =
        chunks.beginMeshJob(0, 0, 0, sectionY, skippedJob);
    check("M6/background-job-skipped",
          skipped.meshSkipped && !skipped.meshBuilt && !skippedJob.valid &&
              section->getMeshState() == ChunkMeshState::CpuReady);
    const ChunkDebugStats afterSkip = chunks.collectDebugStats();
    check("M6/skipped-build-not-counted",
          afterSkip.meshRebuilds == beforeSkip.meshRebuilds &&
              afterSkip.meshBuildTotalMs == beforeSkip.meshBuildTotalMs);

    world.setBlock(8, baseY + CHUNK_SIZE, 8, BlockId::Air);
    SectionMeshInput openedInput;
    section->captureMeshInput(openedInput);
    check("M6/opening-invalidates-enclosure",
          openedInput.needsMeshBuild() && section->isMeshDirty());

    ChunkMeshJob visibleJob;
    const ChunkMeshWorkResult scheduled =
        chunks.beginMeshJob(0, 0, 0, sectionY, visibleJob);
    ChunkMeshCollection visibleMeshes;
    if (visibleJob.valid) {
        ChunkMeshBuilder(visibleJob.input, visibleMeshes).buildMesh();
    }
    const bool installed =
        visibleJob.valid && chunks.finishMeshJob(visibleJob, visibleMeshes, 0.5);
    check("M6/opening-builds-visible-face",
          scheduled.meshBuilt && !scheduled.meshSkipped && installed &&
              section->getMeshes().solidMesh.faces == 1,
          "faces=" +
              std::to_string(section->getMeshes().solidMesh.faces));
    const ChunkDebugStats afterBuild = chunks.collectDebugStats();
    check("M6/real-build-counted-once",
          afterBuild.meshRebuilds == afterSkip.meshRebuilds + 1 &&
              std::abs(afterBuild.meshBuildTotalMs -
                           afterSkip.meshBuildTotalMs - 0.5) <
                  0.001);

    world.setBlock(8, baseY + CHUNK_SIZE, 8, BlockId::Stone);
    const bool synchronousBuildRan = section->makeMesh();
    check("M6/synchronous-build-skipped",
          !synchronousBuildRan &&
              section->getMeshState() == ChunkMeshState::CpuReady &&
              section->getMeshes().solidMesh.faces == 0);
}

// ---------------------------------------------------------------------------
// M7 - frustum priority sorts all chunk targets without filtering any out
// ---------------------------------------------------------------------------
void caseFrustumMeshPriority()
{
    constexpr int radius = 16;
    constexpr int sectionY = 4;
    const VectorXZ center{0, 0};
    const VectorXZ negativeZ{0, -12};
    const VectorXZ positiveZ{0, 12};
    const glm::vec3 eye{8.f, 72.f, 8.f};
    const glm::mat4 projection =
        glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 2000.f);

    ViewFrustum lookingNegativeZ;
    lookingNegativeZ.update(
        projection *
        glm::lookAt(eye, eye + glm::vec3(0.f, 0.f, -1.f),
                    glm::vec3(0.f, 1.f, 0.f)));
    const auto negativeZPriority = World::planChunkMeshWork(
        center, radius, sectionY, &lookingNegativeZ);

    const auto findIndex = [](const std::vector<VectorXZ> &work,
                              const VectorXZ &target) {
        const auto found = std::find(work.begin(), work.end(), target);
        return static_cast<std::size_t>(
            std::distance(work.begin(), found));
    };
    const std::size_t expectedCount =
        static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1));
    const std::size_t negativeFrontIndex =
        findIndex(negativeZPriority, negativeZ);
    const std::size_t negativeRearIndex =
        findIndex(negativeZPriority, positiveZ);
    check("M7/all-view-distance-targets-retained",
          negativeZPriority.size() == expectedCount &&
              negativeFrontIndex < negativeZPriority.size() &&
              negativeRearIndex < negativeZPriority.size(),
          "targets=" + std::to_string(negativeZPriority.size()));
    check("M7/forward-terrain-prioritised",
          negativeFrontIndex < negativeRearIndex,
          "front=" + std::to_string(negativeFrontIndex) +
              " rear=" + std::to_string(negativeRearIndex));

    ViewFrustum lookingPositiveZ;
    lookingPositiveZ.update(
        projection *
        glm::lookAt(eye, eye + glm::vec3(0.f, 0.f, 1.f),
                    glm::vec3(0.f, 1.f, 0.f)));
    const auto positiveZPriority = World::planChunkMeshWork(
        center, radius, sectionY, &lookingPositiveZ);
    const std::size_t positiveFrontIndex =
        findIndex(positiveZPriority, positiveZ);
    const std::size_t positiveRearIndex =
        findIndex(positiveZPriority, negativeZ);
    check("M7/turning-reorders-priority",
          positiveFrontIndex < positiveRearIndex,
          "front=" + std::to_string(positiveFrontIndex) +
              " rear=" + std::to_string(positiveRearIndex));

    const auto distanceOnly =
        World::planChunkMeshWork(center, radius, sectionY, nullptr);
    check("M7/missing-snapshot-falls-back-to-distance",
          distanceOnly.size() == expectedCount &&
              !distanceOnly.empty() && distanceOnly.front() == center);
}

// ---------------------------------------------------------------------------
// B2 - real streaming sources become bounded, mergeable, expiring demand
// ---------------------------------------------------------------------------
void caseStreamingDemandModel()
{
    check("B2/reason-vocabulary-and-policy",
          ChunkDemandReasonCount == 4 &&
              std::string(ChunkDemandModel::reasonName(
                  ChunkDemandReason::Player)) == "Player" &&
              std::string(ChunkDemandModel::reasonName(
                  ChunkDemandReason::Camera)) == "Camera" &&
              std::string(ChunkDemandModel::reasonName(
                  ChunkDemandReason::TeleportDestination)) ==
                  "TeleportDestination" &&
              std::string(ChunkDemandModel::reasonName(
                  ChunkDemandReason::Preload)) == "Preload" &&
              ChunkDemandModel::TeleportPriority >
                  ChunkDemandModel::PlayerPriority &&
              ChunkDemandModel::PlayerPriority >
                  ChunkDemandModel::CameraPriority &&
              ChunkDemandModel::CameraPriority >
                  ChunkDemandModel::PreloadPriority);

    ChunkDemandModel mergedModel;
    mergedModel.refresh(ChunkDemandReason::Player, {0, 0}, 1);
    const std::uint64_t stableRevision =
        mergedModel.snapshot().revision;
    mergedModel.refresh(ChunkDemandReason::Player, {0, 0}, 1);
    mergedModel.refresh(ChunkDemandReason::Camera, {0, 0}, 1);
    const ChunkDemandSnapshot overlapping = mergedModel.snapshot();
    const ChunkDemandDebugStats overlappingStats =
        mergedModel.debugStats();
    check("B2/refresh-is-stable-and-bounded",
          stableRevision == 1 && overlapping.revision == 2 &&
              overlapping.demands.size() == 2 &&
              overlappingStats.activeDemands == 2 &&
              overlappingStats.playerDemands == 1 &&
              overlappingStats.cameraDemands == 1);

    const auto mergedTargets = ChunkRuntime::planDemandWork(
        overlapping, 4, nullptr, {0, 0}, {0, 0});
    const std::uint32_t playerCameraMask =
        ChunkDemandModel::reasonBit(ChunkDemandReason::Player) |
        ChunkDemandModel::reasonBit(ChunkDemandReason::Camera);
    const auto mergedCenter = std::find_if(
        mergedTargets.begin(), mergedTargets.end(),
        [](const ChunkDemandTarget &target) {
            return target.coord == VectorXZ{0, 0};
        });
    check("B2/overlap-merges-reason-bits",
          mergedTargets.size() == 9 &&
              mergedCenter != mergedTargets.end() &&
              mergedCenter->reasonMask == playerCameraMask &&
              mergedCenter->priority ==
                  ChunkDemandModel::PlayerPriority);

    mergedModel.refresh(ChunkDemandReason::Player, {10, 0}, 0);
    const ChunkDemandSnapshot replaced = mergedModel.snapshot();
    const auto replacedPlayer = std::find_if(
        replaced.demands.begin(), replaced.demands.end(),
        [](const ChunkDemand &demand) {
            return demand.reason == ChunkDemandReason::Player;
        });
    check("B2/reason-replacement-removes-old-center",
          replaced.demands.size() == 2 &&
              replacedPlayer != replaced.demands.end() &&
              replacedPlayer->coord == VectorXZ{10, 0});

    ChunkDemandModel priorityModel;
    priorityModel.refresh(ChunkDemandReason::Player, {0, 0}, 0);
    priorityModel.refresh(ChunkDemandReason::TeleportDestination,
                          {20, 0}, 0);
    const auto priorityTargets = ChunkRuntime::planDemandWork(
        priorityModel.snapshot(), 4, nullptr, {0, 0}, {1, 0});
    check("B2/teleport-outprioritises-player",
          priorityTargets.size() == 2 &&
              priorityTargets.front().coord == VectorXZ{20, 0} &&
              priorityTargets.front().priority ==
                  ChunkDemandModel::TeleportPriority);

    ChunkDemandModel motionModel;
    motionModel.refresh(ChunkDemandReason::Player, {0, 0}, 2);
    const auto motionTargets = ChunkRuntime::planDemandWork(
        motionModel.snapshot(), 4, nullptr, {0, 0}, {0, 1});
    const auto targetIndex = [](const auto &targets,
                                const VectorXZ &coord) {
        const auto found = std::find_if(
            targets.begin(), targets.end(),
            [&coord](const ChunkDemandTarget &target) {
                return target.coord == coord;
            });
        return static_cast<std::size_t>(
            std::distance(targets.begin(), found));
    };
    check("B2/player-motion-prioritises-forward",
          targetIndex(motionTargets, {0, 1}) <
              targetIndex(motionTargets, {0, -1}));

    constexpr int viewRadius = 16;
    const glm::vec3 eye{8.f, 72.f, 8.f};
    const glm::mat4 projection =
        glm::perspective(glm::radians(90.f), 16.f / 9.f, 0.1f, 2000.f);
    ViewFrustum lookingNegativeZ;
    lookingNegativeZ.update(
        projection *
        glm::lookAt(eye, eye + glm::vec3(0.f, 0.f, -1.f),
                    glm::vec3(0.f, 1.f, 0.f)));
    ViewFrustum lookingPositiveZ;
    lookingPositiveZ.update(
        projection *
        glm::lookAt(eye, eye + glm::vec3(0.f, 0.f, 1.f),
                    glm::vec3(0.f, 1.f, 0.f)));
    ChunkDemandModel turnModel;
    turnModel.refresh(ChunkDemandReason::Camera, {0, 0}, viewRadius);
    const auto negativePlan = ChunkRuntime::planDemandWork(
        turnModel.snapshot(), 4, &lookingNegativeZ, {0, 0}, {0, 0});
    const auto positivePlan = ChunkRuntime::planDemandWork(
        turnModel.snapshot(), 4, &lookingPositiveZ, {0, 0}, {0, 0});
    check("B2/camera-turn-reorders-pending-plan",
          targetIndex(negativePlan, {0, -12}) <
                  targetIndex(negativePlan, {0, 12}) &&
              targetIndex(positivePlan, {0, 12}) <
                  targetIndex(positivePlan, {0, -12}));

    ChunkDemandModel expiryModel;
    expiryModel.refresh(ChunkDemandReason::TeleportDestination,
                        {30, 30}, 1);
    for (std::uint64_t epoch = 0;
         epoch < ChunkDemandModel::TeleportLifetimeEpochs; ++epoch) {
        expiryModel.advanceEpoch();
    }
    const bool activeThroughExpiry =
        expiryModel.debugStats().teleportDemands == 1;
    expiryModel.advanceEpoch();
    const ChunkDemandDebugStats expired = expiryModel.debugStats();
    check("B2/transient-demand-expires-exactly",
          activeThroughExpiry && expired.teleportDemands == 0 &&
              expired.activeDemands == 0 &&
              expired.expiredDemands == 1);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("b2_demand_sources");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    world.update(camera);
    const ChunkDemandDebugStats published =
        world.collectDebugStats().streamingDemand;
    check("B2/world-publishes-player-camera-preload",
          published.activeDemands == 3 &&
              published.playerDemands == 1 &&
              published.cameraDemands == 1 &&
              published.preloadDemands == 1);

    world.preloadAround({640.f, 90.f, 640.f});
    const ChunkDemandDebugStats preloaded =
        world.collectDebugStats().streamingDemand;
    check("B2/preload-replaces-single-slot",
          preloaded.activeDemands == 3 &&
              preloaded.preloadDemands == 1 &&
              world.getChunkManager().chunkLoadedAt(40, 40));
}

// ---------------------------------------------------------------------------
// B1 - data, CPU mesh and Ogre render residency are orthogonal lifecycles
// ---------------------------------------------------------------------------
void caseChunkResidencyStateMachine()
{
    const std::array<ChunkDataResidencyState,
                     ChunkDataResidencyStateCount> dataStates{{
        ChunkDataResidencyState::Absent,
        ChunkDataResidencyState::Requested,
        ChunkDataResidencyState::Loading,
        ChunkDataResidencyState::Generating,
        ChunkDataResidencyState::Resident,
        ChunkDataResidencyState::EvictRequested,
        ChunkDataResidencyState::Saving,
    }};
    std::size_t dataEdges = 0;
    for (const auto from : dataStates) {
        for (const auto to : dataStates) {
            dataEdges += canTransition(from, to) ? 1u : 0u;
        }
    }
    check("B1/data-state-vocabulary-and-graph",
          dataEdges == 12 &&
              std::string(chunkDataResidencyStateName(
                  ChunkDataResidencyState::EvictRequested)) ==
                  "EvictRequested" &&
              canTransition(ChunkDataResidencyState::Loading,
                            ChunkDataResidencyState::Generating) &&
              canTransition(ChunkDataResidencyState::Loading,
                            ChunkDataResidencyState::Resident) &&
              !canTransition(ChunkDataResidencyState::Absent,
                             ChunkDataResidencyState::Resident),
          "legal_edges=" + std::to_string(dataEdges));

    const std::array<ChunkMeshState, ChunkMeshStateCount> meshStates{{
        ChunkMeshState::Clean,
        ChunkMeshState::Dirty,
        ChunkMeshState::Queued,
        ChunkMeshState::Building,
        ChunkMeshState::CpuReady,
    }};
    std::size_t meshEdges = 0;
    for (const auto from : meshStates) {
        for (const auto to : meshStates) {
            meshEdges += canTransition(from, to) ? 1u : 0u;
        }
    }
    check("B1/mesh-state-vocabulary-and-graph",
          meshEdges == 8 &&
              std::string(chunkMeshStateName(ChunkMeshState::CpuReady)) ==
                  "CpuReady" &&
              canTransition(ChunkMeshState::Dirty,
                            ChunkMeshState::Queued) &&
              !canTransition(ChunkMeshState::Clean,
                             ChunkMeshState::CpuReady),
          "legal_edges=" + std::to_string(meshEdges));

    const std::array<ChunkRenderState, ChunkRenderStateCount> renderStates{{
        ChunkRenderState::NotResident,
        ChunkRenderState::UploadPending,
        ChunkRenderState::GpuResident,
        ChunkRenderState::Stale,
    }};
    std::size_t renderEdges = 0;
    for (const auto from : renderStates) {
        for (const auto to : renderStates) {
            renderEdges += canTransition(from, to) ? 1u : 0u;
        }
    }
    check("B1/render-state-vocabulary-and-graph",
          renderEdges == 8 &&
              std::string(chunkRenderStateName(
                  ChunkRenderState::UploadPending)) == "UploadPending" &&
              canTransition(ChunkRenderState::GpuResident,
                            ChunkRenderState::Stale) &&
              !canTransition(ChunkRenderState::NotResident,
                             ChunkRenderState::GpuResident),
          "legal_edges=" + std::to_string(renderEdges));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    Config config = makeConfig();
    Camera camera(config);
    Player player;
    const auto directory = freshSaveDirectory("b1_residency");
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();
    Chunk *chunk = chunks.findChunk(0, 0);
    check("B1/generated-chunk-is-resident",
          chunk != nullptr &&
              chunk->getDataResidencyState() ==
                  ChunkDataResidencyState::Resident);

    const WorldDebugStats residentStats = world.collectDebugStats();
    check("B1/data-states-are-separately-counted",
          residentStats.chunks.dataResidentChunks ==
                  residentStats.chunks.existingChunks &&
              residentStats.chunks.dataAbsentChunks == 0 &&
              residentStats.chunks.dataSavingChunks == 0,
          "resident=" +
              std::to_string(residentStats.chunks.dataResidentChunks) +
              " existing=" +
              std::to_string(residentStats.chunks.existingChunks));

    constexpr int sectionY = 0;
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
    check("B1/mesh-section-is-available", section != nullptr);
    if (section != nullptr) {
        section->markMeshDirty();
        section->markMeshQueued();
        section->beginMeshBuild();
        ChunkMeshCollection empty;
        section->adoptMesh(empty);
        world.acknowledgeSectionMeshUploads(
            {{section->getLocation(), section->getBlockRevision()}});
        check("B1/mesh-dirty-to-clean-flow",
              section->getMeshState() == ChunkMeshState::Clean);
    }

    constexpr int persistedY = 100;
    world.setBlock(8, persistedY, 8, BlockId::CoalOre);
    chunks.unloadChunk(0, 0);
    check("B1/dirty-eviction-reaches-absent-after-save",
          chunks.findChunk(0, 0) == nullptr);
    chunks.loadChunk(0, 0);
    chunk = chunks.findChunk(0, 0);
    check("B1/reload-restores-resident-data",
          chunk != nullptr &&
              chunk->getDataResidencyState() ==
                  ChunkDataResidencyState::Resident &&
              !chunk->needsSave() &&
              world.getBlock(8, persistedY, 8).id ==
                  static_cast<Block_t>(BlockId::CoalOre));

    const auto failureDirectory =
        freshSaveDirectory("b1_failed_eviction");
    {
        std::ofstream blocker(
            std::filesystem::path(failureDirectory) / "chunks",
            std::ios::binary | std::ios::trunc);
        blocker << "not-a-directory";
    }
    Player failurePlayer;
    World failureWorld(camera, config, failurePlayer, failureDirectory,
                       false, 1);
    auto &failureChunks = failureWorld.getChunkManager();
    failureWorld.setBlock(8, persistedY, 8, BlockId::IronOre);
    failureChunks.unloadChunk(0, 0);
    Chunk *retained = failureChunks.findChunk(0, 0);
    check("B1/failed-save-cancels-eviction",
          retained != nullptr && retained->needsSave() &&
              retained->getDataResidencyState() ==
                  ChunkDataResidencyState::Resident);
}

// ---------------------------------------------------------------------------
// E5 - the renderer consumes versioned CPU mesh snapshots without sharing
// mutable section pointers with the background loader
// ---------------------------------------------------------------------------
void caseSectionMeshUploadSnapshot()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("mesh_upload_snapshot");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    constexpr int blockX = 8;
    constexpr int blockY = 72;
    constexpr int blockZ = 8;
    const int sectionY = blockY / CHUNK_SIZE;
    Chunk *chunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *section =
        chunk != nullptr ? chunk->findSection(sectionY) : nullptr;
    check("E5/mesh-snapshot-section-available", section != nullptr);
    if (section == nullptr) {
        return;
    }

    section->makeMesh();
    WorldMeshSnapshot first = world.collectSectionMeshSnapshot();
    check("E5/cpu-ready-snapshot-produced",
          first.cpuReadySections.size() == 1,
          "ready=" + std::to_string(first.cpuReadySections.size()) +
              " live=" + std::to_string(first.liveSections.size()));
    if (first.cpuReadySections.empty()) {
        return;
    }

    const WorldSectionMeshVersion staleVersion{
        first.cpuReadySections.front().location,
        first.cpuReadySections.front().blockRevision};
    const ChunkBlock previous = world.getBlock(blockX, blockY, blockZ);
    world.setBlock(blockX, blockY, blockZ,
                   previous.id == static_cast<Block_t>(BlockId::CoalOre)
                       ? ChunkBlock(BlockId::Stone)
                       : ChunkBlock(BlockId::CoalOre));
    section->makeMesh();
    world.acknowledgeSectionMeshUploads({staleVersion});
    check("E5/stale-upload-not-acknowledged",
          section->getMeshState() == ChunkMeshState::CpuReady,
          "revision=" + std::to_string(section->getBlockRevision()));

    WorldMeshSnapshot refreshed = world.collectSectionMeshSnapshot();
    if (refreshed.cpuReadySections.empty()) {
        check("E5/current-upload-acknowledged", false,
              "no refreshed CPU mesh");
        return;
    }
    const WorldSectionMeshSnapshot &current =
        refreshed.cpuReadySections.front();
    world.acknowledgeSectionMeshUploads(
        {{current.location, current.blockRevision}});
    check("E5/current-upload-acknowledged",
          section->getMeshState() == ChunkMeshState::Clean,
          "revision=" + std::to_string(current.blockRevision));
}

// ---------------------------------------------------------------------------
// S2.4 - unloading a chunk flushes it to storage before it is dropped
// ---------------------------------------------------------------------------
void caseUnloadPersistence()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("unload_persistence");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    auto &chunks = world.getChunkManager();

    const int y = 100;
    world.setBlock(8, y, 8, BlockId::CoalOre);
    check("S2.4/edit-marks-chunk-dirty",
          chunks.findChunk(0, 0) != nullptr &&
              chunks.findChunk(0, 0)->needsSave());

    // Drop the chunk the way the view-distance unload path does.
    chunks.unloadChunk(0, 0);
    check("S2.4/chunk-dropped-on-unload", chunks.findChunk(0, 0) == nullptr);

    // Reloading in the same session must come back from storage, not from a
    // fresh procedural generation pass.
    chunks.loadChunk(0, 0);
    check("S2.4/edit-survives-unload-reload",
          world.getBlock(8, y, 8).id == static_cast<Block_t>(BlockId::CoalOre),
          "block id=" +
              std::to_string(static_cast<int>(world.getBlock(8, y, 8).id)));
    check("S2.4/reloaded-chunk-is-clean",
          chunks.findChunk(0, 0) != nullptr &&
              !chunks.findChunk(0, 0)->needsSave());
}

// ---------------------------------------------------------------------------
// D1 - stateful block ownership and lifecycle
// ---------------------------------------------------------------------------
void caseBlockEntityLifecycle()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("block_entity_lifecycle");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    ChunkManager &chunks = world.getChunkManager();

    const glm::ivec3 position{-7, 100, -5};
    const glm::ivec3 secondPosition{-6, 100, -5};
    world.setBlock(position.x, position.y, position.z, BlockId::Stone);
    world.setBlock(secondPosition.x, secondPosition.y, secondPosition.z,
                   BlockId::Stone);

    check("D1/create-owned-record",
          world.createBlockEntity(position, "hellomine:test_state",
                                  "{\"value\":1}"));
    check("D1/reject-duplicate-position",
          !world.createBlockEntity(position, "hellomine:other_state", "{}"));
    check("D1/find-world-position",
          world.getBlockEntity(position).has_value() &&
              world.getBlockEntity(position)->position.x == position.x &&
              world.getBlockEntity(position)->payload == "{\"value\":1}");

    check("D1/update-owned-record",
          world.updateBlockEntity(position, "{\"value\":2}") &&
              world.getBlockEntity(position).has_value() &&
              world.getBlockEntity(position)->payload == "{\"value\":2}");
    check("D1/reject-invalid-type",
          !world.createBlockEntity(secondPosition, "Invalid Type", "{}"));

    Chunk *chunk = chunks.findChunk(-1, -1);
    bool duplicateLoadRejected = false;
    bool invalidPositionRejected = false;
    bool originalPreserved = false;
    if (chunk != nullptr) {
        const std::vector<BlockEntityRecord> original =
            chunk->getBlockEntities();
        std::vector<BlockEntityRecord> duplicate = original;
        duplicate.insert(duplicate.end(), original.begin(), original.end());
        duplicateLoadRejected = !chunk->loadBlockEntities(duplicate);

        std::vector<BlockEntityRecord> invalid = original;
        if (!invalid.empty()) {
            invalid.front().position.x = CHUNK_SIZE;
        }
        invalidPositionRejected = !chunk->loadBlockEntities(invalid);
        originalPreserved = chunk->getBlockEntities().size() == 1 &&
                            chunk->getBlockEntities().front().payload ==
                                "{\"value\":2}";
    }
    check("D1/reject-duplicate-load", duplicateLoadRejected);
    check("D1/reject-invalid-position-load", invalidPositionRejected);
    check("D1/rejected-load-is-atomic", originalPreserved);

    chunks.unloadChunk(-1, -1);
    chunks.loadChunk(-1, -1);
    const auto restored = world.getBlockEntity(position);
    check("D1/unload-reload-preserves-state",
          restored.has_value() && restored->type == "hellomine:test_state" &&
              restored->payload == "{\"value\":2}");

    world.setBlock(position.x, position.y, position.z, BlockId::Air);
    check("D1/block-change-removes-state",
          !world.getBlockEntity(position).has_value());
    check("D1/remove-missing-is-safe",
          !world.removeBlockEntity(position).has_value());
}

// ---------------------------------------------------------------------------
// D2 - chest container, transfers, persistence and break policy
// ---------------------------------------------------------------------------
void caseChestContainer()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("chest_container");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    EventRecorder events(world.getEventBus());

    const glm::ivec3 chestPosition{8, 100, 8};
    player.addItem(Material::CHEST_BLOCK, 1);
    const bool placed = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(chestPosition));
    const auto record = world.getBlockEntity(chestPosition);
    check("D2/place-initializes-chest",
          placed &&
              static_cast<BlockId>(world.getBlock(
                  chestPosition.x, chestPosition.y, chestPosition.z).id) ==
                  BlockId::Chest &&
              record.has_value() &&
              record->type == ChestContainer::BlockEntityType);

    const bool opened = BlockInteractionSystem::useBlock(
        world, player, glm::vec3(chestPosition));
    check("D2/use-opens-container",
          opened && player.hasOpenContainer() &&
              player.getOpenContainer()->x == chestPosition.x);

    player.addItem(Material::STONE_BLOCK, 120);
    auto countPlayer = [&player](Material::ID materialId) {
        int total = 0;
        for (const InventorySlotState &slot :
             player.getSaveState().inventory) {
            if (slot.materialId == materialId) {
                total += slot.amount;
            }
        }
        return total;
    };

    const bool stored =
        ChestContainer::transferFromPlayer(world, player, 0, 60);
    auto view = ChestContainer::view(world, player);
    check("D2/store-preserves-total",
          stored && view.has_value() &&
              countPlayer(Material::ID::Stone) +
                      view->inventory.count(Material::ID::Stone) ==
                  120,
          "player=" + std::to_string(countPlayer(Material::ID::Stone)) +
              " chest=" +
              std::to_string(view ? view->inventory.count(
                                         Material::ID::Stone)
                                  : -1));

    const bool taken =
        ChestContainer::transferToPlayer(world, player, 0, 25);
    view = ChestContainer::view(world, player);
    check("D2/take-preserves-total",
          taken && view.has_value() &&
              countPlayer(Material::ID::Stone) +
                      view->inventory.count(Material::ID::Stone) ==
                  120);
    player.addItem(Material::WOODEN_PICKAXE, 1, 7);
    int toolSlot = -1;
    for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
        if (player.getInventorySlot(slot).getMaterial().id ==
            Material::ID::WoodenPickaxe) {
            toolSlot = slot;
            break;
        }
    }
    check("G3/container-rejects-durability-bearing-tools",
          toolSlot >= 0 &&
              !ChestContainer::transferFromPlayer(
                  world, player, toolSlot, 1) &&
              player.getInventorySlot(toolSlot).getDurability() == 7);
    check("D2/transfers-publish-inventory-events",
          events.count(SandboxEventType::PlayerInventoryChanged) >= 2);

    ContainerInventory full(ChestContainer::SlotCount);
    const int fullCapacity =
        ChestContainer::SlotCount * Material::STONE_BLOCK.maxStackSize;
    const int accepted = full.addItem(Material::STONE_BLOCK,
                                      fullCapacity + 10);
    ContainerInventory roundTrip(1);
    check("D2/capacity-is-bounded",
          accepted == fullCapacity &&
              full.addItem(Material::STONE_BLOCK, 1) == 0 &&
              ContainerInventory::deserialize(full.serialize(),
                                              roundTrip) &&
              roundTrip.count(Material::ID::Stone) == fullCapacity);
    check("D2/rejects-invalid-payload",
          !ContainerInventory::deserialize("v1|1|3,1000", roundTrip));

    ContainerInventory persisted(ChestContainer::SlotCount);
    persisted.addItem(Material::IRON_ORE_BLOCK, 7);
    check("D2/prepare-persisted-contents",
          world.updateBlockEntity(chestPosition, persisted.serialize()));
    player.closeContainer();
    world.getChunkManager().unloadChunk(0, 0);
    world.getChunkManager().loadChunk(0, 0);
    const bool reopened = ChestContainer::open(world, player, chestPosition);
    view = ChestContainer::view(world, player);
    check("D2/save-reload-preserves-contents",
          reopened && view.has_value() &&
              view->inventory.count(Material::ID::IronOre) == 7);

    const bool blockedPlace = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(9.f, 100.f, 8.f));
    const bool blockedBreak = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(chestPosition));
    check("D2/open-ui-blocks-world-actions",
          !blockedPlace && !blockedBreak &&
              static_cast<BlockId>(world.getBlock(
                  chestPosition.x, chestPosition.y, chestPosition.z).id) ==
                  BlockId::Chest);

    player.closeContainer();
    check("R3A/container-close-clears-ui-capture",
          !player.hasOpenContainer());
    const std::size_t actorsBefore =
        world.getActorManager().getActorCount();
    const bool broken = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(chestPosition));
    const std::vector<ActorSaveState> drops =
        world.getActorManager().collectSaveStates();
    const bool contentsDropped = std::any_of(
        drops.begin(), drops.end(), [](const ActorSaveState &state) {
            return state.kind == ActorSaveKind::Item &&
                   state.materialId ==
                       static_cast<int>(Material::ID::IronOre) &&
                   state.amount == 7;
        });
    check("D2/break-spills-contents",
          broken && contentsDropped &&
              world.getActorManager().getActorCount() == actorsBefore + 1);
    check("D2/break-removes-container-state",
          !world.getBlockEntity(chestPosition).has_value() &&
              static_cast<BlockId>(world.getBlock(
                  chestPosition.x, chestPosition.y, chestPosition.z).id) ==
                  BlockId::Air &&
              !player.hasOpenContainer());
}

// ---------------------------------------------------------------------------
// N2 - strict smelting, dedicated slots and fixed-tick persistence
// ---------------------------------------------------------------------------
void caseFurnaceProgression()
{
    const std::string smeltingDefinitions =
        "# HelloMine3D smelting registry v1\n"
        "smelt hellomine:iron_ingot\n"
        "input hellomine:iron_ore\n"
        "output hellomine:iron_ingot 1\n"
        "ticks 100\n"
        "end\n"
        "fuel hellomine:coal_ore\n"
        "ticks 160\n"
        "end\n";
    auto rejectsSmelting = [](const std::string &source,
                              const std::string &expected) {
        try {
            SmeltingRegistry invalid;
            invalid.freeze({{"invalid.smelting", source}});
        }
        catch (const std::exception &error) {
            return std::string(error.what()).find(expected) !=
                   std::string::npos;
        }
        return false;
    };
    SmeltingRegistry localRegistry;
    localRegistry.freeze({{"base.smelting", smeltingDefinitions}});
    const SmeltingRecipeDefinition *ironRecipe =
        localRegistry.findRecipe(Material::ID::IronOre);
    const SmeltingFuelDefinition *coalFuel =
        localRegistry.findFuel(Material::ID::CoalOre);
    check("N2/smelting-registry-freezes-bounded-base-definitions",
          localRegistry.isFrozen() && localRegistry.recipes().size() == 1 &&
              localRegistry.fuels().size() == 1 && ironRecipe != nullptr &&
              ironRecipe->outputMaterialId == Material::ID::IronIngot &&
              ironRecipe->durationTicks == 100 && coalFuel != nullptr &&
              coalFuel->burnTicks == 160);
    bool rejectedSecondFreeze = false;
    try {
        localRegistry.freeze({{"again.smelting", smeltingDefinitions}});
    }
    catch (const std::exception &error) {
        rejectedSecondFreeze =
            std::string(error.what()).find("already frozen") !=
            std::string::npos;
    }
    check("N2/smelting-registry-freezes-once", rejectedSecondFreeze);
    check("N2/smelting-registry-rejects-header-and-missing-fields",
          rejectsSmelting("not smelting\n", "unsupported or missing") &&
              rejectsSmelting(
                  "# HelloMine3D smelting registry v1\n"
                  "smelt hellomine:bad\n"
                  "input hellomine:iron_ore\n"
                  "output hellomine:iron_ingot 1\n"
                  "end\n"
                  "fuel hellomine:coal_ore\n"
                  "ticks 1\n"
                  "end\n",
                  "requires distinct input/output and ticks"));
    check("N2/smelting-registry-rejects-duplicate-input",
          rejectsSmelting(
              smeltingDefinitions +
                  "smelt hellomine:duplicate\n"
                  "input hellomine:iron_ore\n"
                  "output hellomine:dirt 1\n"
                  "ticks 1\n"
                  "end\n",
              "Duplicate or excessive smelting recipe"));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    const auto directory = freshSaveDirectory("furnace_progression");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    EventRecorder events(world.getEventBus());
    const glm::ivec3 position{8, 100, 8};

    player.addItem(Material::FURNACE_BLOCK, 1);
    const bool placed = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(position));
    const bool opened = BlockInteractionSystem::useBlock(
        world, player, glm::vec3(position));
    check("N2/furnace-place-creates-versioned-block-entity",
          placed && opened && player.hasOpenContainer() &&
              world.getBlockEntity(position).has_value() &&
              world.getBlockEntity(position)->type ==
                  FurnaceContainer::BlockEntityType);

    player.addItem(Material::IRON_ORE_BLOCK, 2);
    player.addItem(Material::COAL_ORE_BLOCK, 1);
    player.addItem(Material::DIRT_BLOCK, 1);
    auto findSlot = [&player](Material::ID materialId) {
        for (int index = 0; index < player.getInventorySlotCount(); ++index) {
            if (player.getInventorySlot(index).getMaterial().id == materialId) {
                return index;
            }
        }
        return -1;
    };
    const int ironSlot = findSlot(Material::ID::IronOre);
    const int coalSlot = findSlot(Material::ID::CoalOre);
    const int dirtSlot = findSlot(Material::ID::Dirt);
    check("N2/dedicated-slots-reject-wrong-items-and-output-insert",
          ironSlot >= 0 && coalSlot >= 0 && dirtSlot >= 0 &&
              !FurnaceContainer::transferFromPlayer(
                  world, player, FurnaceSlot::Fuel, ironSlot, 1,
                  runtimeSmeltingRegistry()) &&
              !FurnaceContainer::transferFromPlayer(
                  world, player, FurnaceSlot::Input, dirtSlot, 1,
                  runtimeSmeltingRegistry()) &&
              !FurnaceContainer::transferFromPlayer(
                  world, player, FurnaceSlot::Output, ironSlot, 1,
                  runtimeSmeltingRegistry()));
    check("N2/input-and-fuel-transfer-are-independent",
          FurnaceContainer::transferFromPlayer(
              world, player, FurnaceSlot::Input, ironSlot, 2,
              runtimeSmeltingRegistry()) &&
              FurnaceContainer::transferFromPlayer(
                  world, player, FurnaceSlot::Fuel, coalSlot, 1,
                  runtimeSmeltingRegistry()));

    for (int tick = 0; tick < 40; ++tick) {
        world.tick(tick);
    }
    auto furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/fixed-tick-advances-progress-and-fuel-exactly",
          furnace && furnace->state.progressTicks == 40 &&
              furnace->state.burnTicksRemaining == 120 &&
              furnace->state.input.amount == 2 &&
              furnace->state.output.amount == 0);
    const ChunkBlock activeFurnaceBlock = world.getBlock(
        position.x, position.y, position.z);
    check("P11-0/active-furnace-projects-lit-metadata-and-light",
          (activeFurnaceBlock.metadata &
               BlockMetadata::Furnace::LitBit) != 0 &&
              world.getBlockLight(position.x, position.y, position.z) == 13);

    world.setBlock(
        position.x, position.y, position.z,
        ChunkBlock(BlockId::Furnace,
                   static_cast<BlockMetadata_t>(
                       activeFurnaceBlock.metadata &
                       ~BlockMetadata::Furnace::LitBit)));
    check("P11-0/metadata-only-update-preserves-block-entity",
          world.getBlockEntity(position).has_value());

    player.closeContainer();
    world.getChunkManager().unloadChunk(0, 0);
    world.getChunkManager().loadChunk(0, 0);
    FurnaceContainer::open(world, player, position,
                           runtimeSmeltingRegistry());
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/unload-reload-resumes-exact-state-without-catchup",
          furnace && furnace->state.progressTicks == 40 &&
              furnace->state.burnTicksRemaining == 120);
    const ChunkBlock normalizedFurnaceBlock = world.getBlock(
        position.x, position.y, position.z);
    check("P11-0/load-normalizes-active-v11-furnace-before-reconnect",
          (normalizedFurnaceBlock.metadata &
               BlockMetadata::Furnace::LitBit) != 0 &&
              world.getBlockLight(position.x, position.y, position.z) == 13);

    for (int tick = 40; tick < 100; ++tick) {
        world.tick(tick);
    }
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/completion-consumes-input-and-publishes-domain-event",
          furnace && furnace->state.input.amount == 1 &&
              furnace->state.output.materialId == Material::ID::IronIngot &&
              furnace->state.output.amount == 1 &&
              furnace->state.progressTicks == 0 &&
              furnace->state.burnTicksRemaining == 60 &&
              events.count(SandboxEventType::SmeltCompleted) == 1);

    FurnaceState blocked = furnace->state;
    blocked.output.amount = Material::IRON_INGOT.maxStackSize;
    check("N2/full-output-state-can-be-persisted",
          world.updateBlockEntity(position,
                                  FurnaceContainer::serialize(blocked)));
    for (int tick = 100; tick < 110; ++tick) {
        world.tick(tick);
    }
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/full-output-pauses-both-progress-and-fuel",
          furnace && furnace->state.progressTicks == 0 &&
              furnace->state.burnTicksRemaining == 60 &&
              furnace->state.output.amount ==
                  Material::IRON_INGOT.maxStackSize);
    check("P11-0/blocked-furnace-turns-off-without-burning-fuel",
          (world.getBlock(position.x, position.y, position.z).metadata &
               BlockMetadata::Furnace::LitBit) == 0 &&
              world.getBlockLight(position.x, position.y, position.z) == 0);

    blocked.output.amount = 1;
    world.updateBlockEntity(position, FurnaceContainer::serialize(blocked));
    world.tick(110);
    check("P11-0/unblocked-furnace-relights-from-frozen-burn-time",
          (world.getBlock(position.x, position.y, position.z).metadata &
               BlockMetadata::Furnace::LitBit) != 0 &&
              world.getBlockLight(position.x, position.y, position.z) == 13);
    Chunk *furnaceChunk = world.getChunkManager().findChunk(0, 0);
    ChunkSection *furnaceSection = furnaceChunk != nullptr
        ? furnaceChunk->findSection(position.y / CHUNK_SIZE)
        : nullptr;
    const std::uint32_t litRevision = furnaceSection != nullptr
        ? furnaceSection->getBlockRevision()
        : 0;
    world.tick(111);
    check("P11-0/stable-lit-state-does-not-repeat-block-relight",
          furnaceSection != nullptr &&
              furnaceSection->getBlockRevision() == litRevision);
    for (int tick = 112; tick < 170; ++tick) {
        world.tick(tick);
    }
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/fuel-exhaustion-preserves-partial-progress",
          furnace && furnace->state.progressTicks == 60 &&
              furnace->state.burnTicksRemaining == 0 &&
              furnace->state.input.amount == 1);
    check("P11-0/exhausted-furnace-turns-off",
          (world.getBlock(position.x, position.y, position.z).metadata &
               BlockMetadata::Furnace::LitBit) == 0 &&
              world.getBlockLight(position.x, position.y, position.z) == 0);

    player.addItem(Material::COAL_ORE_BLOCK, 1);
    const int refillSlot = findSlot(Material::ID::CoalOre);
    FurnaceContainer::transferFromPlayer(
        world, player, FurnaceSlot::Fuel, refillSlot, 1,
        runtimeSmeltingRegistry());
    for (int tick = 170; tick < 210; ++tick) {
        world.tick(tick);
    }
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/refuel-resumes-partial-recipe",
          furnace && furnace->state.input.amount == 0 &&
              furnace->state.output.amount == 2 &&
              furnace->state.progressTicks == 0 &&
              events.count(SandboxEventType::SmeltCompleted) == 2);
    check("P11-0/completed-empty-furnace-turns-off-with-burn-frozen",
          furnace && furnace->state.burnTicksRemaining > 0 &&
              (world.getBlock(position.x, position.y, position.z).metadata &
               BlockMetadata::Furnace::LitBit) == 0);
    check("N2/output-take-is-capacity-checked-and-atomic",
          FurnaceContainer::transferToPlayer(
              world, player, FurnaceSlot::Output, 2,
              runtimeSmeltingRegistry()) &&
              findSlot(Material::ID::IronIngot) >= 0 &&
              FurnaceContainer::view(
                  world, player, runtimeSmeltingRegistry())
                      ->state.output.amount == 0);

    FurnaceState spill;
    spill.input = {Material::ID::IronOre, 1, 0};
    spill.fuel = {Material::ID::CoalOre, 1, 0};
    spill.output = {Material::ID::IronIngot, 1, 0};
    world.updateBlockEntity(position, FurnaceContainer::serialize(spill));
    player.closeContainer();
    const std::size_t actorsBefore =
        world.getActorManager().getActorCount();
    const bool broken = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(position));
    check("N2/breaking-furnace-spills-three-dedicated-slots",
          broken && !world.getBlockEntity(position) &&
              world.getActorManager().getActorCount() == actorsBefore + 3);

    FurnaceState parsed;
    check("N2/furnace-payload-rejects-invalid-version-and-materials",
          !FurnaceContainer::deserialize(
              "v2|0,0|0,0|0,0|0|0|0",
              runtimeSmeltingRegistry(), parsed) &&
              !FurnaceContainer::deserialize(
                  "v1|3,1|12,1|0,0|0|0|0",
                  runtimeSmeltingRegistry(), parsed));
}

// ---------------------------------------------------------------------------
// N3 - active food recovery, fixed-tick cooldown and current persistence
// ---------------------------------------------------------------------------
void caseFoodRecovery()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    Config config = makeConfig();
    Camera camera(config);

    const auto countMaterial = [](const Player &player,
                                  Material::ID materialId) {
        int amount = 0;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            const ItemStack &stack = player.getInventorySlot(slot);
            if (stack.getMaterial().id == materialId) {
                amount += stack.getNumInStack();
            }
        }
        return amount;
    };

    const auto directory = freshSaveDirectory("food_recovery");
    Player player;
    World world(camera, config, player, directory, false, 1);
    EventRecorder events(world.getEventBus());
    player.addItem(Material::BREAD, 3);

    check("N3/full-health-rejects-without-consuming",
          world.useHeldFood() == FoodUseResult::FullHealth &&
              countMaterial(player, Material::ID::Bread) == 3 &&
              world.getFoodCooldownTicksRemaining() == 0 &&
              events.count(SandboxEventType::FoodConsumed) == 0);

    const bool damaged = world.damagePlayer(8.f);
    const int beforeRejectedUses =
        countMaterial(player, Material::ID::Bread);
    const FoodUseResult paused = world.useHeldFood(false);
    player.openCrafting(CraftingSession::PlayerGridSize);
    const FoodUseResult uiBusy = world.useHeldFood();
    player.closeCrafting();
    check("N3/pause-and-ui-busy-reject-before-inventory-change",
          damaged && paused == FoodUseResult::SimulationPaused &&
              uiBusy == FoodUseResult::UiBusy &&
              countMaterial(player, Material::ID::Bread) ==
                  beforeRejectedUses &&
              std::abs(world.getPlayerHealth() - 12.f) < 0.001f);

    const FoodUseResult firstUse = world.useHeldFood();
    check("N3/success-consumes-once-heals-and-publishes-once",
          firstUse == FoodUseResult::Consumed &&
              countMaterial(player, Material::ID::Bread) == 2 &&
              std::abs(world.getPlayerHealth() - 18.f) < 0.001f &&
              world.getFoodCooldownTicksRemaining() == 20 &&
              events.count(SandboxEventType::FoodConsumed) == 1 &&
              events.count(SandboxEventType::PlayerInventoryChanged) == 1);

    const FoodUseResult immediateRepeat = world.useHeldFood();
    for (int tick = 0; tick < 19; ++tick) {
        world.tick(1000 + tick);
    }
    const FoodUseResult earlyRepeat = world.useHeldFood();
    check("N3/cooldown-is-exact-and-rejected-uses-are-atomic",
          immediateRepeat == FoodUseResult::CoolingDown &&
              earlyRepeat == FoodUseResult::CoolingDown &&
              world.getFoodCooldownTicksRemaining() == 1 &&
              countMaterial(player, Material::ID::Bread) == 2 &&
              events.count(SandboxEventType::FoodConsumed) == 1);

    world.tick(1019);
    const FoodUseResult secondUse = world.useHeldFood();
    check("N3/recovery-clamps-to-max-health",
          secondUse == FoodUseResult::Consumed &&
              countMaterial(player, Material::ID::Bread) == 1 &&
              std::abs(world.getPlayerHealth() -
                       world.getPlayerMaxHealth()) < 0.001f &&
              events.count(SandboxEventType::FoodConsumed) == 2);

    for (int tick = 0; tick < 20; ++tick) {
        world.tick(1100 + tick);
    }
    check("N3/full-health-remains-non-consuming-after-cooldown",
          world.useHeldFood() == FoodUseResult::FullHealth &&
              countMaterial(player, Material::ID::Bread) == 1 &&
              world.getFoodCooldownTicksRemaining() == 0);

    const bool lethal = world.damagePlayer(100.f);
    const int breadBeforeDeathUse =
        countMaterial(player, Material::ID::Bread);
    const FoodUseResult deadUse = world.useHeldFood();
    const bool deadSaved = world.save();
    WorldSaveData deadState;
    const bool deadLoaded = WorldSave(directory).load(deadState);
    check("N3/dead-player-cannot-eat-and-zero-health-is-saveable",
          lethal && deadUse == FoodUseResult::PlayerDead &&
              countMaterial(player, Material::ID::Bread) ==
                  breadBeforeDeathUse &&
              deadSaved && deadLoaded &&
              deadState.version == WorldSaveFormatVersion &&
              deadState.playerState.health == 0.f);
    world.tick(1200);
    check("N3/pending-death-respawns-on-next-fixed-tick",
          std::abs(world.getPlayerHealth() -
                   world.getPlayerMaxHealth()) < 0.001f &&
              glm::length(player.position - world.getPlayerSpawnPoint()) <
                  0.001f);

    const auto persistedDirectory =
        freshSaveDirectory("food_recovery_persisted");
    {
        Player persistedPlayer;
        World persistedWorld(camera, config, persistedPlayer,
                             persistedDirectory, false, 1);
        persistedPlayer.addItem(Material::BREAD, 2);
        const bool persistenceDamage = persistedWorld.damagePlayer(10.f);
        const FoodUseResult persistenceUse =
            persistedWorld.useHeldFood();
        check("N3/prepare-damaged-cooldown-save-state",
              persistenceDamage &&
                  persistenceUse == FoodUseResult::Consumed &&
                  std::abs(persistedWorld.getPlayerHealth() - 16.f) <
                      0.001f &&
                  persistedWorld.getFoodCooldownTicksRemaining() == 20 &&
                  persistedWorld.save());
    }
    {
        Player restoredPlayer;
        World restoredWorld(camera, config, restoredPlayer,
                            persistedDirectory, false, 1);
        const bool restoredExactly =
            std::abs(restoredWorld.getPlayerHealth() - 16.f) < 0.001f &&
            restoredWorld.getFoodCooldownTicksRemaining() == 20 &&
            countMaterial(restoredPlayer, Material::ID::Bread) == 1;
        for (int tick = 0; tick < 20; ++tick) {
            restoredWorld.tick(1300 + tick);
        }
        const FoodUseResult restoredUse = restoredWorld.useHeldFood();
        const int restoredBread = countMaterial(
            restoredPlayer, Material::ID::Bread);
        check("N3/health-cooldown-and-inventory-resume-after-reload",
              restoredExactly &&
                  restoredUse == FoodUseResult::Consumed &&
                  std::abs(restoredWorld.getPlayerHealth() - 20.f) <
                      0.001f && restoredBread == 0,
              "exact=" + std::to_string(restoredExactly ? 1 : 0) +
                  " health=" +
                  std::to_string(restoredWorld.getPlayerHealth()) +
                  " cooldown=" + std::to_string(
                      restoredWorld.getFoodCooldownTicksRemaining()) +
                  " bread=" + std::to_string(restoredBread) +
                  " result=" +
                  std::to_string(static_cast<int>(restoredUse)));
    }

    const auto validationDirectory =
        freshSaveDirectory("food_recovery_save_validation");
    WorldSaveData valid;
    valid.worldId = "n3-recovery-contract";
    valid.worldName = "N3 Recovery Contract";
    valid.seed = kValidationSeed;
    valid.createdUtc = LegacyWorldTimestampUtc;
    valid.lastPlayedUtc = LegacyWorldTimestampUtc;
    valid.lastBuildIdentity = "validation";
    valid.hasPlayerState = true;
    valid.playerState.health = 7.f;
    valid.playerState.foodCooldownTicks = 13;
    valid.playerState.inventory = {{Material::ID::Bread, 1, 0}};
    WorldSave recoverySave(validationDirectory);
    WorldSaveData validRoundTrip;
    check("N3/current-recovery-state-roundtrips",
          recoverySave.save(valid) &&
              recoverySave.load(validRoundTrip) &&
              validRoundTrip.playerState.health == 7.f &&
              validRoundTrip.playerState.foodCooldownTicks == 13);
    WorldSaveData invalidHealth = valid;
    invalidHealth.playerState.health = 21.f;
    WorldSaveData invalidCooldown = valid;
    invalidCooldown.playerState.foodCooldownTicks = 1201;
    WorldSaveData preserved;
    check("N3/invalid-recovery-state-preserves-last-good-save",
          !recoverySave.save(invalidHealth) &&
              !recoverySave.save(invalidCooldown) &&
              recoverySave.load(preserved) &&
              preserved.playerState.health == 7.f &&
              preserved.playerState.foodCooldownTicks == 13);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("food_recovery_v5_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n3-v5-objectives";
    std::filesystem::create_directories(migratedWorld);
    std::filesystem::copy_file(
        ResourcePaths::join(
            ResourcePaths::projectRoot(),
            "tools/fixtures/food/world-v5-objectives.meta"),
        migratedWorld / "world.meta",
        std::filesystem::copy_options::overwrite_existing);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("n3-v5-objectives");
    WorldSaveData migratedData;
    const bool migratedLoaded =
        migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    const bool preservedUnknownObjective =
        std::find(migratedData.objectiveState.completedIds.begin(),
                  migratedData.objectiveState.completedIds.end(),
                  "future.optional") !=
        migratedData.objectiveState.completedIds.end();
    check("N3/version-five-objectives-survive-recovery-migration",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.playerState.health == 20.f &&
              migratedData.playerState.foodCooldownTicks == 0 &&
              migratedData.objectiveState.progress.size() == 1 &&
              migratedData.objectiveState.progress.front().id ==
                  "alpha.craft_workbench" &&
              migratedData.objectiveState.progress.front().value == 1 &&
              preservedUnknownObjective);

    const std::string objectiveSource =
        "# HelloMine3D objective registry v1\n"
        "version 3\n"
        "objective n3.consume_bread\n"
        "type consume_item\n"
        "target hellomine:bread\n"
        "required 1\n"
        "prerequisite none\n"
        "visible 1\n"
        "optional 0\n"
        "title \"Eat Bread\"\n"
        "instruction \"Recover health.\"\n"
        "feedback \"Recovered\"\n"
        "end\n";
    ObjectiveRegistry foodObjectives;
    foodObjectives.freeze({{"n3.objective", objectiveSource}});
    bool rejectedNonFoodTarget = false;
    try {
        std::string invalidObjective = objectiveSource;
        invalidObjective.replace(
            invalidObjective.find("target hellomine:bread"),
            std::string("target hellomine:bread").size(),
            "target hellomine:wheat");
        ObjectiveRegistry invalid;
        invalid.freeze({{"invalid.objective", invalidObjective}});
    }
    catch (const std::exception &error) {
        rejectedNonFoodTarget =
            std::string(error.what()).find("not food") !=
            std::string::npos;
    }
    check("N3/consume-objective-requires-food-target",
          rejectedNonFoodTarget &&
              foodObjectives.find("n3.consume_bread") != nullptr);
    Player objectivePlayer;
    SandboxEventBus objectiveBus;
    ObjectiveSystem foodObjectiveSystem(
        foodObjectives, objectivePlayer, objectiveBus, {}, 0u, false);
    objectiveBus.publish(FoodConsumedEvent(
        DefaultPlayerActorId, Material::ID::Wheat, 3.f, 10.f, {}));
    objectiveBus.publish(FoodConsumedEvent(
        DefaultPlayerActorId, Material::ID::Bread, 0.f, 10.f, {}));
    check("N3/failed-or-wrong-food-event-does-not-advance-objective",
          foodObjectiveSystem.progress("n3.consume_bread") == 0);
    objectiveBus.publish(FoodConsumedEvent(
        DefaultPlayerActorId, Material::ID::Bread, 2.f, 12.f, {}));
    check("N3/successful-food-event-completes-objective-once",
          foodObjectiveSystem.isCompleted("n3.consume_bread") &&
              foodObjectiveSystem.snapshot().sessionComplete);
}

// ---------------------------------------------------------------------------
// N10 - expanded food/smelting choices and current-format capacity metrics
// ---------------------------------------------------------------------------
void caseExpandedResourceEconomy()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    Config config = makeConfig();
    Camera camera(config);

    struct FoodExpectation
    {
        const Material *material;
        float restored;
        int cooldownTicks;
    };
    const FoodExpectation choices[] = {
        {&Material::BREAD, 6.f, 20},
        {&Material::COOKED_MEAT, 9.f, 28},
        {&Material::CACTUS_SALAD, 4.f, 12},
        {&Material::TRAIL_RATION, 14.f, 40},
    };
    bool foodChoicesExact = true;
    for (std::size_t index = 0;
         index < sizeof(choices) / sizeof(choices[0]); ++index) {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory(
                        "n10_food_" + std::to_string(index)),
                    false, 1);
        player.addItem(*choices[index].material, 1);
        foodChoicesExact = foodChoicesExact && world.damagePlayer(19.f) &&
            world.useHeldFood() == FoodUseResult::Consumed &&
            std::abs(world.getPlayerHealth() -
                     (1.f + choices[index].restored)) < 0.001f &&
            world.getFoodCooldownTicksRemaining() ==
                choices[index].cooldownTicks &&
            player.getInventoryCount(choices[index].material->id) == 0;
    }
    check("N10/four-food-runtime-outcomes-are-distinct-and-exact",
          foodChoicesExact);

    Player furnacePlayer;
    World furnaceWorld(camera, config, furnacePlayer,
                       freshSaveDirectory("n10_smelting_paths"), false, 1);
    EventRecorder events(furnaceWorld.getEventBus());
    const glm::ivec3 position{8, 100, 8};
    furnacePlayer.addItem(Material::FURNACE_BLOCK, 1);
    const bool furnaceReady = BlockInteractionSystem::placeBlock(
        furnaceWorld, furnacePlayer, glm::vec3(position));
    FurnaceState meatState;
    meatState.input = {Material::ID::RawMeat, 1, 0};
    meatState.fuel = {Material::ID::PlantFiber, 2, 0};
    const bool meatPrepared = furnaceWorld.updateBlockEntity(
        position, FurnaceContainer::serialize(meatState));
    for (int tick = 0; tick < 60; ++tick) {
        furnaceWorld.tick(20000 + tick);
    }
    FurnaceState cookedState;
    const auto cookedRecord = furnaceWorld.getBlockEntity(position);
    const bool cooked = cookedRecord && FurnaceContainer::deserialize(
        cookedRecord->payload, runtimeSmeltingRegistry(), cookedState) &&
        cookedState.input.amount == 0 &&
        cookedState.output.materialId == Material::ID::CookedMeat &&
        cookedState.output.amount == 1 &&
        cookedState.fuel.amount == 0 &&
        cookedState.burnTicksRemaining == 20;
    check("N10/raw-meat-smelts-with-bounded-plant-fiber-fuel",
          furnaceReady && meatPrepared && cooked &&
              events.count(SandboxEventType::SmeltCompleted) == 1);

    FurnaceState sandState;
    sandState.input = {Material::ID::Sand, 1, 0};
    sandState.fuel = {Material::ID::PlantFiber, 2, 0};
    const bool sandPrepared = furnaceWorld.updateBlockEntity(
        position, FurnaceContainer::serialize(sandState));
    for (int tick = 0; tick < 80; ++tick) {
        furnaceWorld.tick(21000 + tick);
    }
    FurnaceState glassState;
    const auto glassRecord = furnaceWorld.getBlockEntity(position);
    const bool glass = glassRecord && FurnaceContainer::deserialize(
        glassRecord->payload, runtimeSmeltingRegistry(), glassState) &&
        glassState.input.amount == 0 &&
        glassState.output.materialId == Material::ID::Glass &&
        glassState.output.amount == 1 && glassState.fuel.amount == 0 &&
        glassState.burnTicksRemaining == 0;
    check("N10/sand-smelts-to-glass-with-exact-fixed-tick-cost",
          sandPrepared && glass &&
              events.count(SandboxEventType::SmeltCompleted) == 2);

    Inventory expandedInventory;
    const bool capacityFilled =
        expandedInventory.addItem(Material::RAW_MEAT, 99) == 99 &&
        expandedInventory.addItem(Material::COOKED_MEAT, 99) == 99 &&
        expandedInventory.addItem(Material::CACTUS_SALAD, 99) == 99 &&
        expandedInventory.addItem(Material::TRAIL_RATION, 99) == 99 &&
        expandedInventory.addItem(Material::PLANT_FIBER, 99) == 99 &&
        expandedInventory.capacityFor(Material::RAW_MEAT) == 0;
    WorldSaveData baseline;
    baseline.worldId = "n10-baseline";
    baseline.worldName = "N10 Baseline";
    baseline.seed = kValidationSeed;
    baseline.createdUtc = LegacyWorldTimestampUtc;
    baseline.lastPlayedUtc = LegacyWorldTimestampUtc;
    baseline.lastBuildIdentity = "n10-validation";
    baseline.hasPlayerState = true;
    baseline.playerState.inventory = {{Material::ID::Bread, 1, 0}};
    WorldSaveData expanded = baseline;
    expanded.worldId = "n10-expanded";
    expanded.worldName = "N10 Expanded";
    expanded.playerState.inventory = expandedInventory.getSaveState();

    const std::string baselineDirectory =
        freshSaveDirectory("n10_save_baseline");
    const std::string expandedDirectory =
        freshSaveDirectory("n10_save_expanded");
    WorldSave baselineSave(baselineDirectory);
    WorldSave expandedSave(expandedDirectory);
    const auto baselineStart = std::chrono::steady_clock::now();
    const bool baselineSaved = baselineSave.save(baseline);
    const auto baselineElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - baselineStart).count();
    const auto expandedStart = std::chrono::steady_clock::now();
    const bool expandedSaved = expandedSave.save(expanded);
    const auto expandedElapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - expandedStart).count();
    const std::uintmax_t baselineBytes = baselineSaved
        ? std::filesystem::file_size(baselineSave.metadataPath()) : 0;
    const std::uintmax_t expandedBytes = expandedSaved
        ? std::filesystem::file_size(expandedSave.metadataPath()) : 0;
    WorldSaveData roundTrip;
    const bool expandedLoaded = expandedSaved &&
        expandedSave.load(roundTrip);
    check("N10/new-material-ids-roundtrip-in-current-save",
          capacityFilled && expandedLoaded &&
              roundTrip.version == WorldSaveFormatVersion &&
              roundTrip.playerState.inventory ==
                  expanded.playerState.inventory);
    check("N10/inventory-save-size-and-time-stay-bounded",
          baselineSaved && expandedSaved && baselineBytes > 0 &&
              expandedBytes >= baselineBytes &&
              expandedBytes <= baselineBytes + 256 &&
              baselineElapsed < 2000000 && expandedElapsed < 2000000,
          "slots=5 units=495 baseline_bytes=" +
              std::to_string(baselineBytes) + " expanded_bytes=" +
              std::to_string(expandedBytes) + " baseline_us=" +
              std::to_string(baselineElapsed) + " expanded_us=" +
              std::to_string(expandedElapsed));
}

// ---------------------------------------------------------------------------
// N11A - versioned world difficulty, transactional apply and pressure profile
// ---------------------------------------------------------------------------
void caseDifficultyProfiles()
{
    const DifficultyProfile casual = difficultyProfile(
        WorldDifficulty::Casual);
    const DifficultyProfile normal = difficultyProfile(
        WorldDifficulty::Normal);
    const DifficultyProfile challenging = difficultyProfile(
        WorldDifficulty::Challenging);
    check("N11A/three-stable-versioned-profiles-are-centralized",
          CurrentDifficultyProfileVersion == 1 &&
              std::string(worldDifficultyName(WorldDifficulty::Casual)) ==
                  "Casual" &&
              std::string(worldDifficultyName(WorldDifficulty::Normal)) ==
                  "Normal" &&
              std::string(worldDifficultyName(
                  WorldDifficulty::Challenging)) == "Challenging" &&
              casual.playerOutgoingDamageMultiplier >
                  normal.playerOutgoingDamageMultiplier &&
              normal.playerOutgoingDamageMultiplier >
                  challenging.playerOutgoingDamageMultiplier &&
              casual.playerIncomingDamageMultiplier <
                  normal.playerIncomingDamageMultiplier &&
              normal.playerIncomingDamageMultiplier <
                  challenging.playerIncomingDamageMultiplier &&
              casual.naturalSpawnAttemptsPerCycle <
                  normal.naturalSpawnAttemptsPerCycle &&
              normal.naturalSpawnAttemptsPerCycle <
                  challenging.naturalSpawnAttemptsPerCycle &&
              casual.scaleLootAmount(2) == 3 &&
              normal.scaleLootAmount(2) == 2 &&
              challenging.scaleLootAmount(2) == 1);

    const std::string catalogueRoot =
        freshSaveDirectory("n11a_create_difficulty");
    const WorldManagementService management(catalogueRoot);
    const WorldManagementResult casualCreated = management.createWorld(
        "Casual World", kValidationSeed, WorldDifficulty::Casual);
    const WorldManagementResult normalCreated = management.createWorld(
        "Normal World", kValidationSeed, WorldDifficulty::Normal);
    const WorldManagementResult challengingCreated = management.createWorld(
        "Challenging World", kValidationSeed,
        WorldDifficulty::Challenging);
    const WorldManagementResult invalidCreated = management.createWorld(
        "Invalid World", kValidationSeed, WorldDifficulty::Count);
    const WorldManagementListResult listed = management.listWorlds();
    const auto findDifficulty = [&](const std::string &id,
                                    WorldDifficulty difficulty) {
        return std::any_of(
            listed.worlds.begin(), listed.worlds.end(),
            [&](const WorldCatalogueEntry &entry) {
                return entry.id == id && entry.seed == kValidationSeed &&
                    entry.saveFormatVersion == WorldSaveFormatVersion &&
                    entry.difficultyProfileVersion ==
                        CurrentDifficultyProfileVersion &&
                    entry.difficulty == difficulty;
            });
    };
    check("N11A/create-command-and-catalogue-preserve-difficulty",
          casualCreated.succeeded() && normalCreated.succeeded() &&
              challengingCreated.succeeded() && listed.succeeded() &&
              listed.worlds.size() == 3 &&
              findDifficulty(casualCreated.worldId,
                             WorldDifficulty::Casual) &&
              findDifficulty(normalCreated.worldId,
                             WorldDifficulty::Normal) &&
              findDifficulty(challengingCreated.worldId,
                             WorldDifficulty::Challenging) &&
              invalidCreated.status ==
                  WorldManagementStatus::InvalidArgument);

    WorldSaveData casualData;
    WorldSaveData challengingData;
    const bool createdMetadataLoaded = WorldSave(
        casualCreated.directoryPath).load(casualData) &&
        WorldSave(challengingCreated.directoryPath).load(challengingData);
    check("N11A/difficulty-never-changes-seed-or-terrain-identity",
          createdMetadataLoaded && casualData.seed == challengingData.seed &&
              casualData.terrainGenerationVersion ==
                  challengingData.terrainGenerationVersion &&
              casualData.difficulty == WorldDifficulty::Casual &&
              challengingData.difficulty ==
                  WorldDifficulty::Challenging);

    const std::string currentDirectory =
        freshSaveDirectory("n11a_current_save");
    WorldSaveData current;
    current.worldId = "n11a-current-save";
    current.worldName = "N11A Current Save";
    current.seed = kValidationSeed;
    current.createdUtc = LegacyWorldTimestampUtc;
    current.lastPlayedUtc = LegacyWorldTimestampUtc;
    current.lastBuildIdentity = "n11a-validation";
    current.difficulty = WorldDifficulty::Challenging;
    WorldSave currentSave(currentDirectory);
    WorldSaveData currentRoundTrip;
    const bool currentSaved = currentSave.save(current) &&
        currentSave.load(currentRoundTrip);
    WorldSaveData invalidId = current;
    invalidId.difficulty = WorldDifficulty::Count;
    WorldSaveData invalidProfile = current;
    invalidProfile.difficultyProfileVersion = 2;
    WorldSaveData preserved;
    check("N11A/v10-roundtrip-and-invalid-save-preserve-last-good-state",
          currentSaved &&
              currentRoundTrip.version == WorldSaveFormatVersion &&
              currentRoundTrip.difficultyProfileVersion == 1 &&
              currentRoundTrip.difficulty ==
                  WorldDifficulty::Challenging &&
              !currentSave.save(invalidId) &&
              !currentSave.save(invalidProfile) &&
              currentSave.load(preserved) &&
              preserved.difficulty == WorldDifficulty::Challenging);

    const std::string validText = readTextFile(currentSave.metadataPath());
    const auto removeField = [](std::string text,
                                const std::string &field) {
        const std::size_t begin = text.find(field);
        if (begin != std::string::npos) {
            const std::size_t end = text.find('\n', begin);
            text.erase(begin, end == std::string::npos
                                  ? text.size() - begin : end - begin + 1);
        }
        return text;
    };
    const std::filesystem::path malformedRoot =
        freshSaveDirectory("n11a_malformed_saves");
    const std::filesystem::path missingPath = malformedRoot / "missing.meta";
    const std::filesystem::path duplicatePath =
        malformedRoot / "duplicate.meta";
    {
        std::ofstream missing(missingPath,
                              std::ios::binary | std::ios::trunc);
        missing << removeField(validText, "difficulty_id ");
        std::ofstream duplicate(duplicatePath,
                                std::ios::binary | std::ios::trunc);
        duplicate << validText << "difficulty_id 1\n";
    }
    WorldSaveData malformed;
    std::string malformedError;
    const bool missingRejected = !WorldSave::loadFromPath(
        missingPath.string(), malformed, &malformedError);
    const bool duplicateRejected = !WorldSave::loadFromPath(
        duplicatePath.string(), malformed, &malformedError);
    check("N11A/v10-missing-and-duplicate-difficulty-fields-reject",
          missingRejected && duplicateRejected);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("n11a_v9_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n11a-v9-migration";
    std::filesystem::create_directories(migratedWorld);
    std::string versionNine = removeField(
        removeField(
            removeField(
                removeField(
                    removeField(validText,
                                "difficulty_profile_version "),
                    "difficulty_id "),
                "post_victory_event_version "),
            "post_victory_completed_events "),
        "exploration_reward_version ");
    const std::size_t versionPosition = versionNine.find("version 12");
    if (versionPosition != std::string::npos) {
        versionNine.replace(versionPosition, 10, "version 9");
    }
    const std::size_t idPosition = versionNine.find(
        "world_id n11a-current-save");
    if (idPosition != std::string::npos) {
        versionNine.replace(idPosition,
                            std::string("world_id n11a-current-save").size(),
                            "world_id n11a-v9-migration");
    }
    {
        std::ofstream legacy(migratedWorld / "world.meta",
                             std::ios::binary | std::ios::trunc);
        legacy << versionNine;
    }
    const WorldManagementService migrationManagement(
        migrationRoot.string());
    const WorldManagementResult migrated =
        migrationManagement.prepareWorldForOpen("n11a-v9-migration");
    WorldSaveData migratedData;
    const bool migratedLoaded = migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    check("N11A/v1-v9-worlds-migrate-to-normal-v1-profile",
          migratedLoaded && migratedData.version == WorldSaveFormatVersion &&
              migratedData.difficultyProfileVersion ==
                  CurrentDifficultyProfileVersion &&
              migratedData.difficulty == WorldDifficulty::Normal &&
              migratedData.seed == kValidationSeed &&
              migratedData.terrainGenerationVersion ==
                  current.terrainGenerationVersion);

    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "");
    Config config = makeConfig();
    Camera camera(config);
    const auto makeRuntimeSave = [&](const std::string &name,
                                     WorldDifficulty difficulty) {
        const std::string directory = freshSaveDirectory(name);
        WorldSaveData data;
        data.worldId = name;
        data.worldName = name;
        data.seed = kValidationSeed;
        data.createdUtc = LegacyWorldTimestampUtc;
        data.lastPlayedUtc = LegacyWorldTimestampUtc;
        data.lastBuildIdentity = "n11a-runtime";
        data.spawnPoint = {8.f, 100.f, 8.f};
        data.hasPlayerState = true;
        data.playerState.position = data.spawnPoint;
        data.playerState.health = 20.f;
        data.difficulty = difficulty;
        if (!WorldSave(directory).save(data)) {
            return std::string();
        }
        return directory;
    };

    struct PressureObservation
    {
        float outgoingDamage = 0.f;
        float incomingDamage = 0.f;
        std::size_t spawnAttempts = 0;
        std::size_t worldCap = 0;
        std::size_t localCap = 0;
        int scaledLoot = 0;
    };
    const auto observe = [&](const std::string &name,
                             WorldDifficulty difficulty) {
        PressureObservation result;
        Player player;
        const std::string directory = makeRuntimeSave(name, difficulty);
        if (directory.empty()) return result;
        World world(camera, config, player, directory, false, 1);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, {9.f, 100.f, 8.f});
        auto *mob = dynamic_cast<LivingActor *>(
            world.getActorManager().findActor(mobId));
        const float mobHealth = mob != nullptr ? mob->getHealth() : 0.f;
        if (mob != nullptr && world.attackActor(mobId, 4.f)) {
            result.outgoingDamage = mobHealth - mob->getHealth();
        }
        const float playerHealth = world.getPlayerHealth();
        if (world.damagePlayer(4.f, mobId)) {
            result.incomingDamage = playerHealth - world.getPlayerHealth();
        }
        const WorldDebugStats before = world.collectDebugStats();
        world.tick(20);
        const WorldDebugStats after = world.collectDebugStats();
        result.spawnAttempts = after.naturalMobSpawnAttempts -
            before.naturalMobSpawnAttempts;
        result.worldCap = after.naturalMobWorldCap;
        result.localCap = after.naturalMobLocalCap;
        result.scaledLoot = world.scaleDifficultyLootAmount(2);
        return result;
    };
    const PressureObservation casualObserved = observe(
        "n11a-casual-runtime", WorldDifficulty::Casual);
    const PressureObservation normalObserved = observe(
        "n11a-normal-runtime", WorldDifficulty::Normal);
    const PressureObservation challengingObserved = observe(
        "n11a-challenging-runtime", WorldDifficulty::Challenging);
    check("N11A/three-difficulties-produce-monotonic-runtime-pressure",
          casualObserved.outgoingDamage > normalObserved.outgoingDamage &&
              normalObserved.outgoingDamage >
                  challengingObserved.outgoingDamage &&
              casualObserved.incomingDamage < normalObserved.incomingDamage &&
              normalObserved.incomingDamage <
                  challengingObserved.incomingDamage &&
              casualObserved.worldCap < normalObserved.worldCap &&
              normalObserved.worldCap < challengingObserved.worldCap &&
              casualObserved.localCap < normalObserved.localCap &&
              normalObserved.localCap < challengingObserved.localCap &&
              casualObserved.spawnAttempts <=
                  casual.naturalSpawnAttemptsPerCycle &&
              normalObserved.spawnAttempts <=
                  normal.naturalSpawnAttemptsPerCycle &&
              challengingObserved.spawnAttempts <=
                  challenging.naturalSpawnAttemptsPerCycle &&
              casualObserved.scaledLoot == 3 &&
              normalObserved.scaledLoot == 2 &&
              challengingObserved.scaledLoot == 1,
          "damage_out=" + std::to_string(casualObserved.outgoingDamage) +
              "/" + std::to_string(normalObserved.outgoingDamage) + "/" +
              std::to_string(challengingObserved.outgoingDamage) +
              " damage_in=" +
              std::to_string(casualObserved.incomingDamage) + "/" +
              std::to_string(normalObserved.incomingDamage) + "/" +
              std::to_string(challengingObserved.incomingDamage));

    const std::string pendingDirectory = makeRuntimeSave(
        "n11a-pending-runtime", WorldDifficulty::Normal);
    Player pendingPlayer;
    World pendingWorld(camera, config, pendingPlayer, pendingDirectory,
                       false, 1);
    const WorldDebugStats pendingBefore = pendingWorld.collectDebugStats();
    const DifficultyChangeResult queued = pendingWorld.requestDifficulty(
        WorldDifficulty::Challenging);
    const DifficultyRuntimeSnapshot beforeTick =
        pendingWorld.getDifficultySnapshot();
    const DifficultyChangeResult duplicate = pendingWorld.requestDifficulty(
        WorldDifficulty::Challenging);
    const DifficultyChangeResult invalid = pendingWorld.requestDifficulty(
        WorldDifficulty::Count);
    pendingWorld.tick(1);
    const DifficultyRuntimeSnapshot afterTick =
        pendingWorld.getDifficultySnapshot();
    const WorldDebugStats pendingAfter = pendingWorld.collectDebugStats();
    const bool pendingSaved = pendingWorld.save();
    WorldSaveData pendingReload;
    const bool pendingLoaded = pendingSaved &&
        WorldSave(pendingDirectory).load(pendingReload);
    check("N11A/pause-command-applies-once-at-next-fixed-tick-and-persists",
          queued == DifficultyChangeResult::Queued &&
              duplicate == DifficultyChangeResult::Unchanged &&
              invalid == DifficultyChangeResult::Invalid &&
              beforeTick.active == WorldDifficulty::Normal &&
              beforeTick.changePending &&
              beforeTick.pending == WorldDifficulty::Challenging &&
              beforeTick.applicationEpoch == 0 &&
              afterTick.active == WorldDifficulty::Challenging &&
              !afterTick.changePending && afterTick.applicationEpoch == 1 &&
              pendingBefore.terrainSeed == pendingAfter.terrainSeed &&
              pendingBefore.terrainGenerationVersion ==
                  pendingAfter.terrainGenerationVersion &&
              pendingLoaded &&
              pendingReload.difficulty == WorldDifficulty::Challenging &&
              pendingAfter.difficultyProfileVersion == 1 &&
              pendingAfter.difficulty == WorldDifficulty::Challenging);

    // Keep the following legacy world-interaction fixtures independent from
    // the runtime worlds above.  They place and mine blocks around this fixed
    // validation position.
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
}

// ---------------------------------------------------------------------------
// N11B - bounded, optional and idempotent post-victory Waystone events
// ---------------------------------------------------------------------------
void casePostVictoryEvents()
{
    WaystoneEncounterState migratedPayload;
    const std::string legacyPayload =
        "version 1\n"
        "wave 3\n"
        "remaining 0\n"
        "reward_epoch 1\n";
    const bool legacyPayloadLoaded = WaystoneEncounter::deserialize(
        legacyPayload, migratedPayload);
    WaystoneEncounterState activePayload{
        3, 0, WaystoneEncounter::RewardEpoch, 3, 2, 2};
    WaystoneEncounterState activeRoundTrip;
    const std::string currentPayload =
        WaystoneEncounter::serialize(activePayload);
    check("N11B/three-events-use-a-versioned-bounded-schedule",
          PostVictoryEvents::CurrentVersion == 1 &&
              PostVictoryEvents::MaximumEvents == 3 &&
              PostVictoryEvents::guardianCount(1, 1) == 1 &&
              PostVictoryEvents::guardianCount(2, 1) == 2 &&
              PostVictoryEvents::guardianCount(2, 2) == 1 &&
              PostVictoryEvents::guardianCount(3, 2) == 2 &&
              legacyPayloadLoaded &&
              migratedPayload.postVictoryEvent == 0 &&
              currentPayload.find("version 2") != std::string::npos &&
              WaystoneEncounter::deserialize(
                  currentPayload, activeRoundTrip) &&
              activeRoundTrip.postVictoryEvent == 3 &&
              activeRoundTrip.postVictoryWave == 2 &&
              activeRoundTrip.postVictoryRemainingGuardians == 2 &&
              !WaystoneEncounter::validState(
                  {3, 0, WaystoneEncounter::RewardEpoch, 4, 1, 1}));

    const std::string currentDirectory =
        freshSaveDirectory("n11b_current_save");
    WorldSaveData current;
    current.worldId = "n11b-current-save";
    current.worldName = "N11B Current Save";
    current.seed = kValidationSeed;
    current.createdUtc = LegacyWorldTimestampUtc;
    current.lastPlayedUtc = LegacyWorldTimestampUtc;
    current.lastBuildIdentity = "n11b-validation";
    current.worldOutcome = {
        WorldOutcomePhase::RewardClaimed,
        WaystoneEncounter::RewardEpoch,
        WaystoneEncounter::RewardEpoch};
    WorldSave currentSave(currentDirectory);
    WorldSaveData currentRoundTrip;
    const bool currentSaved = currentSave.save(current) &&
        currentSave.load(currentRoundTrip);
    WorldSaveData invalidProgress = current;
    invalidProgress.completedPostVictoryEvents =
        PostVictoryEvents::MaximumEvents + 1;
    WorldSaveData preserved;
    const std::string validText = readTextFile(currentSave.metadataPath());
    const auto removeField = [](std::string text,
                                const std::string &field) {
        const std::size_t begin = text.find(field);
        if (begin != std::string::npos) {
            const std::size_t end = text.find('\n', begin);
            text.erase(begin, end == std::string::npos
                                  ? text.size() - begin
                                  : end - begin + 1);
        }
        return text;
    };
    const std::filesystem::path malformedRoot =
        freshSaveDirectory("n11b_malformed_saves");
    const std::filesystem::path missingPath =
        malformedRoot / "missing.meta";
    const std::filesystem::path duplicatePath =
        malformedRoot / "duplicate.meta";
    {
        std::ofstream missing(missingPath,
                              std::ios::binary | std::ios::trunc);
        missing << removeField(
            validText, "post_victory_completed_events ");
        std::ofstream duplicate(duplicatePath,
                                std::ios::binary | std::ios::trunc);
        duplicate << validText
                  << "post_victory_completed_events 0\n";
    }
    WorldSaveData malformed;
    std::string malformedError;
    check("N11B/v11-progress-roundtrips-and-malformed-state-rejects",
          currentSaved &&
              currentRoundTrip.version == WorldSaveFormatVersion &&
              currentRoundTrip.postVictoryEventVersion == 1 &&
              currentRoundTrip.completedPostVictoryEvents == 0 &&
              !currentSave.save(invalidProgress) &&
              currentSave.load(preserved) &&
              preserved.completedPostVictoryEvents == 0 &&
              !WorldSave::loadFromPath(
                  missingPath.string(), malformed, &malformedError) &&
              !WorldSave::loadFromPath(
                  duplicatePath.string(), malformed, &malformedError));

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("n11b_v10_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n11b-v10-migration";
    std::filesystem::create_directories(migratedWorld);
    std::string versionTen = removeField(
        removeField(
            removeField(validText, "post_victory_event_version "),
            "post_victory_completed_events "),
        "exploration_reward_version ");
    const std::size_t versionPosition = versionTen.find("version 12");
    if (versionPosition != std::string::npos) {
        versionTen.replace(versionPosition, 10, "version 10");
    }
    const std::size_t idPosition = versionTen.find(
        "world_id n11b-current-save");
    if (idPosition != std::string::npos) {
        versionTen.replace(
            idPosition,
            std::string("world_id n11b-current-save").size(),
            "world_id n11b-v10-migration");
    }
    {
        std::ofstream legacy(migratedWorld / "world.meta",
                             std::ios::binary | std::ios::trunc);
        legacy << versionTen;
    }
    const WorldManagementService migrationManagement(
        migrationRoot.string());
    const WorldManagementResult migrated =
        migrationManagement.prepareWorldForOpen(
            "n11b-v10-migration");
    WorldSaveData migratedData;
    const bool migratedLoaded = migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    check("N11B/v1-v10-worlds-migrate-with-zero-completed-events",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.postVictoryEventVersion == 1 &&
              migratedData.completedPostVictoryEvents == 0 &&
              migratedData.worldOutcome.phase ==
                  WorldOutcomePhase::RewardClaimed);

    setEnv("HELLOMINE3D_SEED", "20260825");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    setEnv("HELLOMINE3D_WORLD_TIME", "1");
    const std::string runtimeDirectory =
        freshSaveDirectory("n11b_bounded_runtime");
    WorldSaveData runtimeSave = current;
    runtimeSave.worldId = "n11b-bounded-runtime";
    runtimeSave.worldName = "N11B Bounded Runtime";
    runtimeSave.spawnPoint = {8.f, 100.f, 8.f};
    runtimeSave.hasPlayerState = true;
    runtimeSave.playerState.position = runtimeSave.spawnPoint;
    runtimeSave.playerState.health = 20.f;
    const bool runtimeSaved =
        WorldSave(runtimeDirectory).save(runtimeSave);
    const glm::ivec3 core{9, 100, 8};
    const Config config = makeConfig();
    const auto prepareArena = [&core](World &world,
                                      const glm::ivec3 &anchor) {
        for (int x = 1; x <= 15; ++x) {
            for (int z = 1; z <= 15; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
            }
        }
        world.setBlock(anchor.x, anchor.y, anchor.z,
                       BlockId::WaystoneCore);
        return world.initializeWaystone(anchor);
    };
    const auto guardianIds = [](const World &world) {
        std::vector<ActorId> result;
        for (const ActorSnapshot &actor :
             world.getActorManager().collectSnapshots()) {
            if (actor.type == WaystoneEncounter::StalkerType ||
                actor.type == WaystoneEncounter::BruteType) {
                result.push_back(actor.id);
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    };
    const auto finishCombat = [&guardianIds](World &world) {
        int iterations = 0;
        while (iterations++ < 12) {
            const PostVictoryEventSnapshot snapshot =
                world.getPostVictoryEventSnapshot();
            if (snapshot.rewardPending || snapshot.activeEvent == 0) {
                break;
            }
            const std::vector<ActorId> ids = guardianIds(world);
            if (ids.empty()) {
                world.tick(iterations + 10);
                continue;
            }
            for (ActorId id : ids) {
                world.attackActor(id, 100.f);
            }
        }
        return world.getPostVictoryEventSnapshot().rewardPending;
    };

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, runtimeDirectory, false, 1);
        const bool arenaReady = runtimeSaved &&
            prepareArena(world, core);
        const WaystoneActionResult firstStart =
            world.useWaystone(core, player, true);
        const PostVictoryEventSnapshot firstActive =
            world.getPostVictoryEventSnapshot();
        const bool deathAccepted = world.damagePlayer(100.f);
        const PostVictoryEventSnapshot abandoned =
            world.getPostVictoryEventSnapshot();
        world.tick(2);
        const WaystoneActionResult restarted =
            world.useWaystone(core, player, true);
        check("N11B/death-restarts-current-event-without-consuming-progress",
              arenaReady &&
                  firstStart ==
                      WaystoneActionResult::PostVictoryEventStarted &&
                  firstActive.activeEvent == 1 &&
                  firstActive.wave == 1 &&
                  firstActive.loadedGuardians == 1 &&
                  deathAccepted && abandoned.activeEvent == 0 &&
                  abandoned.completedEvents == 0 &&
                  abandoned.loadedGuardians == 0 &&
                  restarted ==
                      WaystoneActionResult::PostVictoryEventStarted);

        const bool firstPending = finishCombat(world);
        const WaystoneActionResult firstReward =
            world.useWaystone(core, player, true);
        const WaystoneActionResult secondStart =
            world.useWaystone(core, player, true);
        std::vector<ActorId> secondGuardians = guardianIds(world);
        if (!secondGuardians.empty()) {
            world.attackActor(secondGuardians.front(), 100.f);
        }
        const PostVictoryEventSnapshot partial =
            world.getPostVictoryEventSnapshot();
        check("N11B/reward-is-once-only-and-next-event-is-explicit",
              firstPending &&
                  firstReward ==
                      WaystoneActionResult::PostVictoryRewardClaimed &&
                  player.getInventoryCount(Material::ID::IronIngot) ==
                      PostVictoryEvents::RewardIronIngots &&
                  world.getPostVictoryEventSnapshot().completedEvents == 1 &&
                  secondStart ==
                      WaystoneActionResult::PostVictoryEventStarted &&
                  partial.activeEvent == 2 && partial.wave == 1 &&
                  partial.remainingGuardians == 1 &&
                  partial.loadedGuardians == 1 && world.save());
    }

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, runtimeDirectory, false, 1);
        const PostVictoryEventSnapshot restored =
            world.getPostVictoryEventSnapshot();
        check("N11B/mid-event-reopen-restores-exactly-one-guardian",
              restored.completedEvents == 1 &&
                  restored.activeEvent == 2 && restored.wave == 1 &&
                  restored.remainingGuardians == 1 &&
                  restored.loadedGuardians == 1 &&
                  guardianIds(world).size() == 1);

        const bool secondPending = finishCombat(world);
        const WaystoneActionResult secondReward =
            world.useWaystone(core, player, true);
        const WaystoneActionResult thirdStart =
            world.useWaystone(core, player, true);
        const bool thirdPending = finishCombat(world);
        const WaystoneActionResult thirdReward =
            world.useWaystone(core, player, true);
        const int rewardCount =
            player.getInventoryCount(Material::ID::IronIngot);
        const WaystoneActionResult complete =
            world.useWaystone(core, player, true);
        const WaystoneActionResult repeated =
            world.useWaystone(core, player, true);
        const PostVictoryEventSnapshot finished =
            world.getPostVictoryEventSnapshot();
        check("N11B/three-events-finish-with-bounded-actors-and-rewards",
              secondPending &&
                  secondReward ==
                      WaystoneActionResult::PostVictoryRewardClaimed &&
                  thirdStart ==
                      WaystoneActionResult::PostVictoryEventStarted &&
                  thirdPending &&
                  thirdReward ==
                      WaystoneActionResult::PostVictoryRewardClaimed &&
                  complete == WaystoneActionResult::PostVictoryComplete &&
                  repeated == WaystoneActionResult::PostVictoryComplete &&
                  finished.complete && finished.completedEvents == 3 &&
                  finished.activeEvent == 0 &&
                  finished.loadedGuardians == 0 &&
                  rewardCount ==
                      PostVictoryEvents::MaximumEvents *
                          PostVictoryEvents::RewardIronIngots &&
                  player.getInventoryCount(Material::ID::IronIngot) ==
                      rewardCount && world.save());
    }

    {
        Camera camera(config);
        Player player;
        World world(camera, config, player, runtimeDirectory, false, 1);
        const glm::ivec3 replacement{10, 100, 8};
        world.onWaystoneBroken(core);
        world.setBlock(core.x, core.y, core.z, BlockId::Air);
        world.setBlock(replacement.x, replacement.y, replacement.z,
                       BlockId::WaystoneCore);
        const bool initialized = world.initializeWaystone(replacement);
        const int rewardCount =
            player.getInventoryCount(Material::ID::IronIngot);
        const WaystoneActionResult replacementUse =
            world.useWaystone(replacement, player, true);
        check("N11B/breaking-or-replacing-core-cannot-reset-rewards",
              initialized &&
                  world.getPostVictoryEventSnapshot().completedEvents == 3 &&
                  replacementUse ==
                      WaystoneActionResult::PostVictoryComplete &&
                  player.getInventoryCount(Material::ID::IronIngot) ==
                      rewardCount);
    }
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
}

std::string validAudioDefinitions()
{
    return R"(# HelloMine3D audio definitions v3
sample ui.click ui 2d "media/audio/samples/ui-click.wav" 0.30 2 "audio.ui.click.caption" "Menu selection"
sample block.break effects 3d "media/audio/samples/block-break.wav" 0.58 4 "audio.block.break.caption" "Block broken"
sample block.place effects 3d "media/audio/samples/block-place.wav" 0.48 4 "audio.block.place.caption" "Block placed"
sample item.pickup effects 3d "media/audio/samples/item-pickup.wav" 0.42 3 "audio.item.pickup.caption" "Item collected"
sample craft.success effects 2d "media/audio/samples/craft-success.wav" 0.42 2 "audio.craft.success.caption" "Crafting complete"
sample combat.hit effects 3d "media/audio/samples/combat-hit.wav" 0.62 4 "audio.combat.hit.caption" "Combat hit"
sample combat.windup effects 3d "media/audio/samples/combat-windup.wav" 0.50 4 "audio.combat.windup.caption" "Enemy attack warning"
sample combat.guard effects 3d "media/audio/samples/combat-guard.wav" 0.55 3 "audio.combat.guard.caption" "Attack blocked"
sample ambient.wind ambient 2d "media/audio/samples/ambient-wind.wav" 0.20 1 "audio.ambient.wind.caption" "Wind"
)";
}

// ---------------------------------------------------------------------------
// G5 - strict audio definitions, event routing and silent degradation
// ---------------------------------------------------------------------------
void caseAudioFeedback()
{
    AudioDefinitionRegistry loaded;
    std::string loadError;
    const bool loadedBase = loaded.tryFreezeFromFile(
        ResourcePaths::media("audio/Base.audio"), loadError);
    const AudioDefinition *ui = loaded.find("ui.click");
    const AudioDefinition *block = loaded.find("block.break");
    check("G5/base-audio-definitions-freeze-complete-cue-set",
          loadedBase && loadError.empty() && loaded.isFrozen() &&
              loaded.definitions().size() == 9 && ui != nullptr &&
              block != nullptr && ui->caption == "Menu selection" &&
              block->caption == "Block broken" &&
              ui->samplePath == "media/audio/samples/ui-click.wav" &&
              ui->captionKey == "audio.ui.click.caption");
    check("G5/audio-definitions-carry-category-and-spatial-mode",
          ui != nullptr && ui->category == AudioCategory::Ui &&
              !ui->spatial && block != nullptr && block->spatial &&
              block->category == AudioCategory::Effects);

    auto rejects = [](const std::string &source,
                      const std::string &expected) {
        try {
            AudioDefinitionRegistry invalid;
            invalid.freeze({{"invalid.audio", source}});
        }
        catch (const std::exception &error) {
            return std::string(error.what()).find(expected) !=
                   std::string::npos;
        }
        return false;
    };
    check("G5/duplicate-audio-cue-is-rejected",
          rejects(validAudioDefinitions() +
                      "sample ui.click ui 2d \"media/audio/samples/ui-click.wav\" 0.2 1 \"audio.ui.click.caption\" \"Again\"\n",
                  "duplicate cue id"));
    std::string missingCaption = validAudioDefinitions();
    missingCaption.replace(missingCaption.find(" \"Menu selection\""),
                           std::string(" \"Menu selection\"").size(), "");
    check("N6/audio-caption-is-required-and-bounded",
          rejects(missingCaption, "expected sample") ||
              rejects(missingCaption, "caption"));
    std::string invalidPath = validAudioDefinitions();
    invalidPath.replace(
        invalidPath.find("media/audio/samples/ui-click.wav"),
        std::string("media/audio/samples/ui-click.wav").size(),
        "media/audio/../ui-click.wav");
    check("N12B/audio-sample-path-is-strictly-validated",
          rejects(invalidPath, "sample path must be a canonical"));
    std::string invalidCaptionKey = validAudioDefinitions();
    invalidCaptionKey.replace(
        invalidCaptionKey.find("audio.ui.click.caption"),
        std::string("audio.ui.click.caption").size(),
        "audio.wrong.caption");
    check("N12B/audio-caption-key-is-cue-derived",
          rejects(invalidCaptionKey, "caption key must be"));
    const std::size_t ambientLine =
        validAudioDefinitions().find("sample ambient.wind");
    std::string missingCue = validAudioDefinitions().substr(0, ambientLine);
    check("G5/missing-required-audio-cue-is-rejected",
          rejects(missingCue, "missing required cue"));

    AudioDefinitionRegistry unavailable;
    std::string unavailableError;
    const bool missingLoaded = unavailable.tryFreezeFromFile(
        ResourcePaths::bin("validation_runs/missing.audio"),
        unavailableError);
    std::unique_ptr<AudioRuntime> degraded = AudioRuntime::create(
        std::move(unavailable), userSettings(makeConfig()));
    check("G5/missing-audio-resource-freezes-empty-view",
          !missingLoaded && !unavailableError.empty() &&
              degraded->definitions().isFrozen() &&
              degraded->definitions().definitions().empty());
    check("G5/missing-audio-resource-selects-silent-dummy",
          std::string(degraded->backendName()) == "dummy" &&
              !degraded->usesRealBackend() &&
              degraded->degradedReason().find("unavailable") !=
                  std::string::npos);

    AudioDefinitionRegistry routedDefinitions;
    routedDefinitions.freeze({{"runtime.audio", validAudioDefinitions()}});
    Config config = makeConfig();
    UserSettings settings = userSettings(config);
    SandboxEventBus eventBus;
    std::unique_ptr<AudioRuntime> audio = AudioRuntime::createDummy(
        std::move(routedDefinitions), settings);
    check("N12B/bundled-pcm16-samples-freeze-into-bounded-cache",
          audio->samples().isFrozen() && audio->samples().cueCount() == 9 &&
              audio->samples().uniqueSampleCount() == 9 &&
              audio->samples().decodedBytes() > 0 &&
              audio->samples().decodedBytes() <=
                  AudioSampleBank::MaximumDecodedBytes &&
              audio->samples().find("ambient.wind") != nullptr &&
              audio->samples().find("ambient.wind")->sampleRate ==
                  AudioSampleBank::RequiredSampleRate);

    std::string sharedSampleDefinitions = validAudioDefinitions();
    const std::array<const char *, 8> otherSamples = {{
        "block-break.wav", "block-place.wav", "item-pickup.wav",
        "craft-success.wav", "combat-hit.wav", "combat-windup.wav",
        "combat-guard.wav", "ambient-wind.wav"}};
    for (const char *name : otherSamples) {
        const std::string needle = name;
        sharedSampleDefinitions.replace(
            sharedSampleDefinitions.find(needle), needle.size(),
            "ui-click.wav");
    }
    AudioDefinitionRegistry sharedRegistry;
    sharedRegistry.freeze({{"shared.audio", sharedSampleDefinitions}});
    std::unique_ptr<AudioRuntime> sharedSamples = AudioRuntime::createDummy(
        std::move(sharedRegistry), settings);
    check("N12B/sample-cache-deduplicates-shared-logical-paths",
          sharedSamples->samples().cueCount() == 9 &&
              sharedSamples->samples().uniqueSampleCount() == 1);

    const std::string invalidWaveRoot =
        freshSaveDirectory("n12b-invalid-wave");
    const std::filesystem::path invalidWavePath =
        std::filesystem::path(invalidWaveRoot) / "invalid.wav";
    {
        std::ofstream invalidWave(
            invalidWavePath, std::ios::binary | std::ios::trunc);
        invalidWave << "RIFF invalid sampled audio";
    }
    AudioDefinitionRegistry invalidWaveRegistry;
    invalidWaveRegistry.freeze(
        {{"invalid-wave.audio", validAudioDefinitions()}});
    std::unique_ptr<AudioRuntime> invalidWave = AudioRuntime::createDummy(
        std::move(invalidWaveRegistry), settings,
        [&invalidWavePath](const std::string &)
        {
            return invalidWavePath.string();
        });
    check("N12B/malformed-wave-selects-bounded-silent-degradation",
          invalidWave->samples().cueCount() == 0 &&
              std::string(invalidWave->backendName()) == "dummy" &&
              invalidWave->degradedReason().find("outside 44..524288") !=
                  std::string::npos);

    const std::filesystem::path stereoWavePath =
        std::filesystem::path(invalidWaveRoot) / "stereo.wav";
    std::filesystem::copy_file(
        ResourcePaths::media("audio/samples/ui-click.wav"),
        stereoWavePath,
        std::filesystem::copy_options::overwrite_existing);
    {
        std::fstream stereoWave(stereoWavePath,
                                std::ios::binary | std::ios::in |
                                    std::ios::out);
        const char stereoChannels[2] = {2, 0};
        stereoWave.seekp(22, std::ios::beg);
        stereoWave.write(stereoChannels, sizeof(stereoChannels));
    }
    AudioDefinitionRegistry stereoWaveRegistry;
    stereoWaveRegistry.freeze(
        {{"stereo-wave.audio", validAudioDefinitions()}});
    std::unique_ptr<AudioRuntime> stereoWave = AudioRuntime::createDummy(
        std::move(stereoWaveRegistry), settings,
        [&stereoWavePath](const std::string &)
        {
            return stereoWavePath.string();
        });
    check("N12B/non-mono-or-non-pcm16-wave-is-rejected",
          stereoWave->samples().cueCount() == 0 &&
              stereoWave->degradedReason().find(
                  "44100 Hz mono PCM16") != std::string::npos);

    std::string missingSampleDefinitions = validAudioDefinitions();
    missingSampleDefinitions.replace(
        missingSampleDefinitions.find("ui-click.wav"),
        std::string("ui-click.wav").size(), "missing.wav");
    AudioDefinitionRegistry missingSampleRegistry;
    missingSampleRegistry.freeze(
        {{"missing-sample.audio", missingSampleDefinitions}});
    std::unique_ptr<AudioRuntime> missingSample = AudioRuntime::createDummy(
        std::move(missingSampleRegistry), settings);
    missingSample->emitUiClick();
    check("N12B/missing-sample-is-silent-and-nonfatal",
          missingSample->samples().cueCount() == 0 &&
              missingSample->stats().missingSamples == 1 &&
              missingSample->stats().playedEvents == 0 &&
              missingSample->degradedReason().find("unable to open") !=
                  std::string::npos);
    std::vector<std::string> captions;
    audio->setCaptionSink([&captions](std::string cueId,
                                     std::string caption) {
        captions.push_back(std::move(cueId) + ":" + caption);
    });
    audio->attach(eventBus);
    eventBus.publish(BlockBreakEvent({1, 2, 3}, BlockId::Stone));
    eventBus.publish(BlockPlaceEvent({2, 2, 3}, BlockId::Dirt));
    eventBus.publish(ItemPickupEvent(
        DefaultPlayerActorId, 7, Material::ID::Stone, 1,
        glm::vec3(2.f, 2.f, 3.f)));
    eventBus.publish(EntityDamageEvent(
        9, DefaultPlayerActorId, 2.f, 8.f, glm::vec3(3.f, 2.f, 3.f)));
    eventBus.publish(CraftCompletedEvent(
        "hellomine:workbench", Material::ID::Workbench, 1, 1,
        glm::vec3(4.f, 2.f, 3.f)));
    check("G5/domain-events-route-once-to-five-audio-cues",
          audio->stats().submittedEvents == 5 &&
              audio->stats().playedEvents == 5 &&
              audio->stats().suppressedEvents == 0);

    for (int index = 0; index < 5; ++index) {
        eventBus.publish(EntityDamageEvent(
            10 + index, DefaultPlayerActorId, 1.f, 7.f,
            glm::vec3(3.f + index, 2.f, 3.f)));
    }
    check("G5/per-cue-concurrency-is-bounded",
          audio->stats().playedEvents == 8 &&
              audio->stats().suppressedEvents == 2);

    const AudioRuntimeStats beforeCombatReadability = audio->stats();
    eventBus.publish(CombatWindupEvent(
        12, DefaultPlayerActorId, glm::vec3(1.f, 2.f, 3.f), 6));
    eventBus.publish(CombatGuardEvent(
        DefaultPlayerActorId, 12, glm::vec3(2.f, 2.f, 3.f),
        CombatDirection::Front));
    check("N8A/windup-and-guard-events-route-once-to-audio",
          audio->stats().submittedEvents ==
                  beforeCombatReadability.submittedEvents + 2 &&
              audio->stats().playedEvents ==
                  beforeCombatReadability.playedEvents + 2);

    AudioListenerState listener;
    audio->update(0.f, false, listener);
    const AudioRuntimeStats beforePause = audio->stats();
    audio->setWorldPaused(true);
    eventBus.publish(BlockBreakEvent({1, 2, 3}, BlockId::Stone));
    audio->emitUiClick();
    check("G5/pause-suppresses-world-audio-but-keeps-ui",
          audio->stats().suppressedEvents ==
                  beforePause.suppressedEvents + 1 &&
              audio->stats().playedEvents ==
                  beforePause.playedEvents + 1);

    audio->setWorldPaused(false);
    audio->update(0.f, false, listener);
    settings.effectsVolume = 0.f;
    audio->setUserSettings(settings);
    const AudioRuntimeStats beforeEffectsZero = audio->stats();
    eventBus.publish(BlockPlaceEvent({1, 2, 3}, BlockId::Dirt));
    audio->emitUiClick();
    check("G5/category-volume-is-applied-independently",
          audio->stats().suppressedEvents ==
                  beforeEffectsZero.suppressedEvents + 1 &&
              audio->stats().playedEvents ==
                  beforeEffectsZero.playedEvents + 1);

    audio->update(0.f, false, listener);
    settings.masterVolume = 0.f;
    audio->setUserSettings(settings);
    const std::size_t suppressedBeforeMaster =
        audio->stats().suppressedEvents;
    const std::size_t captionsBeforeMaster = captions.size();
    audio->emitUiClick();
    check("N6/captions-remain-observable-when-volume-is-zero",
          audio->stats().suppressedEvents ==
              suppressedBeforeMaster + 1 &&
              captions.size() == captionsBeforeMaster + 1 &&
              captions.back() == "ui.click:Menu selection");

    settings.audioCaptions = false;
    audio->setUserSettings(settings);
    const std::size_t captionsBeforeDisabled = captions.size();
    audio->emitUiClick();
    check("N6/disabled-audio-captions-do-not-reach-the-sink",
          captions.size() == captionsBeforeDisabled);

    settings.masterVolume = 1.f;
    settings.effectsVolume = 1.f;
    settings.audioCaptions = true;
    audio->setUserSettings(settings);
    audio->update(0.f, false, listener);
    audio->setMuted(true);
    const std::size_t suppressedBeforeMute =
        audio->stats().suppressedEvents;
    audio->emitUiClick();
    audio->setMuted(false);
    audio->setSuspended(true);
    audio->emitUiClick();
    audio->setSuspended(false);
    audio->update(0.f, false, listener);
    const std::size_t playedBeforeResume = audio->stats().playedEvents;
    audio->emitUiClick();
    check("G5/mute-and-device-suspend-are-nonfatal",
          audio->stats().suppressedEvents ==
                  suppressedBeforeMute + 2 &&
              audio->stats().playedEvents == playedBeforeResume + 1);

    const std::size_t missingBefore = audio->stats().missingDefinitions;
    audio->submit({"missing.cue", false, glm::vec3(0.f), 1.f});
    check("G5/missing-cue-is-diagnosed-without-backend-call",
          audio->stats().missingDefinitions == missingBefore + 1);
    const std::size_t submittedBeforeDetach =
        audio->stats().submittedEvents;
    audio->detach();
    eventBus.publish(BlockBreakEvent({1, 2, 3}, BlockId::Stone));
    check("G5/detach-removes-all-domain-subscriptions",
          audio->stats().submittedEvents == submittedBeforeDetach);

    AudioDefinitionRegistry ambientDefinitions;
    ambientDefinitions.freeze(
        {{"ambient.audio", validAudioDefinitions()}});
    std::unique_ptr<AudioRuntime> ambient = AudioRuntime::createDummy(
        std::move(ambientDefinitions), userSettings(makeConfig()));
    ambient->update(AudioRuntime::AmbientIntervalSeconds - 0.1f, true,
                    listener);
    const bool earlySilent = ambient->stats().ambientEvents == 0;
    ambient->update(0.2f, true, listener);
    const bool intervalPlayed = ambient->stats().ambientEvents == 1 &&
                                ambient->stats().playedEvents == 1;
    ambient->setWorldPaused(true);
    ambient->update(AudioRuntime::AmbientIntervalSeconds * 2.f, true,
                    listener);
    check("G5/ambient-cadence-is-bounded-and-pause-aware",
          earlySilent && intervalPlayed &&
              ambient->stats().ambientEvents == 1);

    RecipeRegistry recipes;
    std::ifstream recipeInput(ResourcePaths::media("recipes/Base.recipe"),
                              std::ios::binary);
    std::ostringstream recipeContent;
    recipeContent << recipeInput.rdbuf();
    recipes.freeze({{"base.recipe", recipeContent.str()}});
    Player crafter;
    SandboxEventBus craftingBus;
    int completedEvents = 0;
    std::string completedRecipeId;
    Material::ID completedMaterialId = Material::ID::Nothing;
    int completedCrafts = 0;
    int completedOutput = 0;
    craftingBus.subscribe(
        SandboxEventType::CraftCompleted,
        [&completedEvents, &completedRecipeId, &completedMaterialId,
         &completedCrafts, &completedOutput](const SandboxEvent &event) {
            ++completedEvents;
            const auto &completed =
                static_cast<const CraftCompletedEvent &>(event);
            completedRecipeId = completed.recipeId;
            completedMaterialId = completed.outputMaterialId;
            completedCrafts = completed.craftsCompleted;
            completedOutput = completed.outputAdded;
        });
    crafter.attachEventBus(craftingBus);
    crafter.addItem(Material::OAK_BARK_BLOCK, 4);
    CraftingSession crafting(CraftingSession::PlayerGridSize);
    for (int index = 0; index < crafting.cellCount(); ++index) {
        crafting.setCell(index, Material::ID::OakBark);
    }
    const CraftingPreview preview = crafter.previewCrafting(
        crafting, recipes);
    const CraftingCommitResult committed = crafter.commitCrafting(
        crafting, recipes, preview, 1);
    const CraftingCommitResult rejected = crafter.commitCrafting(
        crafting, recipes, preview, 1);
    check("G5/successful-craft-publishes-one-domain-event",
          committed.succeeded() && !rejected.succeeded() &&
              completedEvents == 1 &&
              completedRecipeId == "hellomine:workbench" &&
              completedMaterialId == Material::ID::Workbench &&
              completedCrafts == 1 && completedOutput == 1);
    crafter.detachEventBus(craftingBus);
}

std::string validMusicDefinitions()
{
    return R"(# HelloMine3D music definitions v1
track overworld.quiet "media/music/tracks/quiet-horizons.wav" 0.36 20000 2500 1800 6000 45000 90000
)";
}

// ---------------------------------------------------------------------------
// N12C - strict single-track definitions and bounded streamed playback
// ---------------------------------------------------------------------------
void caseStreamedMusic()
{
    MusicDefinitionRegistry loaded;
    std::string loadError;
    const bool loadedBase = loaded.tryFreezeFromFile(
        ResourcePaths::media("music/Base.music"), loadError);
    const MusicTrackDefinition *track = loaded.track();
    check("N12C/base-music-definition-freezes-one-low-density-track",
          loadedBase && loadError.empty() && loaded.isFrozen() &&
              loaded.tracks().size() == 1 && track != nullptr &&
              track->id == "overworld.quiet" &&
              track->streamPath ==
                  "media/music/tracks/quiet-horizons.wav" &&
              track->durationMilliseconds == 20000 &&
              track->minimumGapMilliseconds == 45000 &&
              track->maximumGapMilliseconds == 90000);

    const MusicStreamFileInfo stream = inspectMusicStreamFile(
        ResourcePaths::media("music/tracks/quiet-horizons.wav"));
    check("N12C/music-stream-header-is-strict-and-bounded",
          stream.sampleRate == 44100 && stream.channels == 1 &&
              stream.bitsPerSample == 16 &&
              stream.durationMilliseconds == 20000 &&
              stream.dataOffset == 44 && stream.dataBytes > 0 &&
              stream.dataBytes < stream.fileBytes &&
              stream.fileBytes <= 32u * 1024u * 1024u);

    auto rejects = [](const std::string &source,
                      const std::string &expected) {
        try {
            MusicDefinitionRegistry invalid;
            invalid.freeze({{"invalid.music", source}});
        }
        catch (const std::exception &error) {
            return std::string(error.what()).find(expected) !=
                   std::string::npos;
        }
        return false;
    };
    check("N12C/duplicate-or-wrong-track-identity-is-rejected",
          rejects(validMusicDefinitions() +
                      "track overworld.quiet \"media/music/tracks/quiet-horizons.wav\" 0.2 20000 500 500 0 10000 10000\n",
                  "duplicate track id") &&
              rejects(
                  "# HelloMine3D music definitions v1\n"
                  "track overworld.loud \"media/music/tracks/quiet-horizons.wav\" 0.2 20000 500 500 0 10000 10000\n",
                  "requires exactly overworld.quiet"));
    std::string traversal = validMusicDefinitions();
    traversal.replace(
        traversal.find("media/music/tracks/quiet-horizons.wav"),
        std::string("media/music/tracks/quiet-horizons.wav").size(),
        "media/music/tracks/../quiet-horizons.wav");
    check("N12C/music-stream-path-traversal-is-rejected",
          rejects(traversal, "stream path must be a canonical"));
    std::string invalidBounds = validMusicDefinitions();
    invalidBounds.replace(invalidBounds.find("0.36 20000 2500"),
                          std::string("0.36 20000 2500").size(),
                          "1.1 9000 50");
    check("N12C/music-gain-duration-and-fades-are-bounded",
          rejects(invalidBounds, "gain must be between 0 and 1"));
    std::string invalidGap = validMusicDefinitions();
    invalidGap.replace(invalidGap.find("45000 90000"),
                       std::string("45000 90000").size(),
                       "90000 45000");
    check("N12C/music-gap-order-is-bounded",
          rejects(invalidGap, "low-density gap is outside"));

    Config config = makeConfig();
    UserSettings settings = userSettings(config);
    std::unique_ptr<MusicRuntime> music = MusicRuntime::createDummy(
        std::move(loaded), settings);
    music->update(5.9f, true, false);
    const bool initialDelayHeld =
        music->state() == MusicPlaybackState::Waiting &&
        music->stats().playsStarted == 0;
    music->update(0.2f, true, false);
    const bool startedAfterDelay =
        music->state() == MusicPlaybackState::FadingIn &&
        music->stats().playsStarted == 1;
    music->update(2.5f, true, false);
    check("N12C/world-entry-delay-and-fade-in-are-deterministic",
          initialDelayHeld && startedAfterDelay &&
              music->state() == MusicPlaybackState::Playing &&
              music->stats().playsStarted == 1);
    check("N12C/streaming-does-not-preload-the-whole-track",
          music->stats().streamedBytes > 0 &&
              music->stats().streamedBytes < music->stream().dataBytes);

    music->update(1.8f, true, true);
    const std::size_t bytesAtPause = music->stats().streamedBytes;
    music->update(5.f, true, true);
    check("N12C/pause-fades-and-holds-stream-progress",
          music->state() == MusicPlaybackState::Paused &&
              music->stats().pauses == 1 &&
              music->stats().streamedBytes == bytesAtPause);
    music->setSuspended(true);
    music->update(30.f, true, false);
    const bool suspendedHeld =
        music->stats().streamedBytes == bytesAtPause;
    music->setSuspended(false);
    music->update(2.5f, true, false);
    check("N12C/device-suspend-and-resume-are-nonfatal",
          suspendedHeld &&
              music->state() == MusicPlaybackState::Playing &&
              music->stats().playsStarted == 1);

    music->setMuted(true);
    music->update(1.8f, true, false);
    check("N12C/mute-fades-to-a-bounded-stop",
          music->state() == MusicPlaybackState::Stopped &&
              music->stats().stops == 1 &&
              !music->stats().workerActive);
    music->stopImmediately();
    check("N12C/explicit-shutdown-leaves-no-stream-worker",
          music->state() == MusicPlaybackState::Stopped &&
              !music->stats().workerActive);

    MusicDefinitionRegistry completionDefinitions;
    completionDefinitions.freeze(
        {{"completion.music", validMusicDefinitions()}});
    std::unique_ptr<MusicRuntime> completion =
        MusicRuntime::createDummy(std::move(completionDefinitions),
                                  settings);
    completion->update(6.1f, true, false);
    completion->update(2.5f, true, false);
    completion->update(20.f, true, false);
    check("N12C/completion-enters-a-low-density-replay-gap",
          completion->state() == MusicPlaybackState::Waiting &&
              completion->stats().playsStarted == 1 &&
              completion->stats().playsCompleted == 1);

    MusicDefinitionRegistry zeroVolumeDefinitions;
    zeroVolumeDefinitions.freeze(
        {{"zero-volume.music", validMusicDefinitions()}});
    std::unique_ptr<MusicRuntime> zeroVolume =
        MusicRuntime::createDummy(std::move(zeroVolumeDefinitions),
                                  settings);
    zeroVolume->update(6.1f, true, false);
    zeroVolume->update(2.5f, true, false);
    UserSettings silentSettings = settings;
    silentSettings.musicVolume = 0.f;
    zeroVolume->setUserSettings(silentSettings);
    zeroVolume->update(0.9f, true, false);
    const bool zeroVolumeStillFading =
        zeroVolume->state() == MusicPlaybackState::FadingOut;
    zeroVolume->update(0.9f, true, false);
    check("N12C/zero-music-volume-stops-without-restart",
          zeroVolumeStillFading &&
              zeroVolume->state() == MusicPlaybackState::Stopped &&
              zeroVolume->stats().playsStarted == 1 &&
              zeroVolume->stats().stops == 1);

    MusicDefinitionRegistry unavailable;
    std::string unavailableError;
    const bool missingLoaded = unavailable.tryFreezeFromFile(
        ResourcePaths::bin("validation_runs/missing.music"),
        unavailableError);
    std::unique_ptr<MusicRuntime> missing = MusicRuntime::createDummy(
        std::move(unavailable), settings);
    missing->update(100.f, true, false);
    check("N12C/missing-music-is-silent-and-nonfatal",
          !missingLoaded && !unavailableError.empty() &&
              missing->definitions().isFrozen() &&
              missing->definitions().tracks().empty() &&
              !missing->streamAvailable() &&
              missing->state() == MusicPlaybackState::Degraded &&
              missing->stats().playsStarted == 0);

    const std::string invalidWaveRoot =
        freshSaveDirectory("n12c-invalid-wave");
    const std::filesystem::path invalidWavePath =
        std::filesystem::path(invalidWaveRoot) / "invalid.wav";
    {
        std::ofstream invalidWave(
            invalidWavePath, std::ios::binary | std::ios::trunc);
        invalidWave << "RIFF invalid streamed music";
    }
    MusicDefinitionRegistry invalidWaveDefinitions;
    invalidWaveDefinitions.freeze(
        {{"invalid-wave.music", validMusicDefinitions()}});
    std::unique_ptr<MusicRuntime> invalidWave =
        MusicRuntime::createDummy(
            std::move(invalidWaveDefinitions), settings,
            [&invalidWavePath](const std::string &) {
                return invalidWavePath.string();
            });
    check("N12C/malformed-stream-selects-silent-degradation",
          !invalidWave->streamAvailable() &&
              invalidWave->state() == MusicPlaybackState::Degraded &&
              invalidWave->degradedReason().find("outside 44..33554432") !=
                  std::string::npos);

    std::string durationMismatch = validMusicDefinitions();
    durationMismatch.replace(durationMismatch.find("20000"), 5, "21000");
    MusicDefinitionRegistry mismatchedDefinitions;
    mismatchedDefinitions.freeze(
        {{"mismatched.music", durationMismatch}});
    std::unique_ptr<MusicRuntime> mismatched =
        MusicRuntime::createDummy(std::move(mismatchedDefinitions),
                                  settings);
    check("N12C/declared-and-stream-duration-must-match",
          !mismatched->streamAvailable() &&
              mismatched->state() == MusicPlaybackState::Degraded &&
              mismatched->degradedReason().find(
                  "duration does not match") != std::string::npos);
}

std::string validToolDefinitions()
{
    return R"(# HelloMine3D tool registry v1
tool hellomine:wooden_pickaxe
class pickaxe
tier 1
speed 2
durability 16
attack 2
attack_cooldown 12
attack_reach 3
end
tool hellomine:stone_pickaxe
class pickaxe
tier 2
speed 4
durability 32
attack 3
attack_cooldown 11
attack_reach 3
end
tool hellomine:iron_pickaxe
class pickaxe
tier 3
speed 6
durability 64
attack 4
attack_cooldown 10
attack_reach 3
end
tool hellomine:iron_sword
class weapon
tier 3
speed 1
durability 80
attack 7
attack_cooldown 8
attack_reach 3.75
end
tool hellomine:wooden_sword
class weapon
tier 1
speed 1
durability 32
attack 4
attack_cooldown 10
attack_reach 3.25
end
tool hellomine:stone_sword
class weapon
tier 2
speed 1
durability 56
attack 5
attack_cooldown 9
attack_reach 3.5
end
tool hellomine:wooden_axe
class axe
tier 1
speed 3
durability 16
attack 3
attack_cooldown 11
attack_reach 3
end
tool hellomine:wooden_shovel
class shovel
tier 1
speed 4
durability 16
attack 2
attack_cooldown 12
attack_reach 3
end
)";
}

std::string validEnemyDefinitions()
{
    return R"(# HelloMine3D enemy registry v3
enemy hellomine:natural_mob
health 10
dimensions 0.35 0.9 0.35
wander_speed 1.2
chase_radius 12
chase_speed 2.4
contact_damage 2
combat_mode melee
attack_range 0.75
attack_windup_ticks 8
attack_recover_ticks 8
attack_cooldown_ticks 20
knockback 2.5
natural 0
loot hellomine:plant_fiber 1 1
end
enemy hellomine:stalker
health 8
dimensions 0.30 0.75 0.30
wander_speed 1.6
chase_radius 14
chase_speed 3.2
contact_damage 1
combat_mode melee
attack_range 0.75
attack_windup_ticks 6
attack_recover_ticks 7
attack_cooldown_ticks 16
knockback 2
natural 1
loot hellomine:plant_fiber 1 1
loot hellomine:wheat 1 2
loot hellomine:raw_meat 1 1
end
enemy hellomine:brute
health 16
dimensions 0.45 1.05 0.45
wander_speed 0.8
chase_radius 10
chase_speed 1.6
contact_damage 4
combat_mode melee
attack_range 0.9
attack_windup_ticks 14
attack_recover_ticks 12
attack_cooldown_ticks 28
knockback 4
natural 1
loot hellomine:plant_fiber 1 1
loot hellomine:coal_ore 1 1
loot hellomine:wheat 1 1
loot hellomine:raw_meat 1 2
end
enemy hellomine:spitter
health 7
dimensions 0.32 0.80 0.32
wander_speed 1.1
chase_radius 18
chase_speed 2
contact_damage 0
combat_mode ranged
attack_range 12
attack_windup_ticks 12
attack_recover_ticks 10
attack_cooldown_ticks 32
knockback 1.5
projectile_speed 10
projectile_damage 2
projectile_lifetime_ticks 50
projectile_max_distance 20
projectile_radius 0.15
projectile_world_limit 24
projectile_local_limit 8
projectile_active_radius 32
natural 1
loot hellomine:plant_fiber 1 1
loot hellomine:wheat_seeds 1 2
end
enemy hellomine:waystone_stalker
health 8
dimensions 0.30 0.75 0.30
wander_speed 1.6
chase_radius 18
chase_speed 3.2
contact_damage 1
combat_mode melee
attack_range 0.75
attack_windup_ticks 6
attack_recover_ticks 7
attack_cooldown_ticks 16
knockback 2
natural 0
loot hellomine:iron_ore 1 1
end
enemy hellomine:waystone_brute
health 16
dimensions 0.45 1.05 0.45
wander_speed 0.8
chase_radius 18
chase_speed 1.6
contact_damage 4
combat_mode melee
attack_range 0.9
attack_windup_ticks 14
attack_recover_ticks 12
attack_cooldown_ticks 28
knockback 4
natural 0
loot hellomine:iron_ingot 1 1
end
)";
}

std::string validSmeltingDefinitions()
{
    return R"(# HelloMine3D smelting registry v1
smelt hellomine:iron_ingot
input hellomine:iron_ore
output hellomine:iron_ingot 1
ticks 100
end
smelt hellomine:cooked_meat
input hellomine:raw_meat
output hellomine:cooked_meat 1
ticks 60
end
smelt hellomine:glass
input hellomine:sand
output hellomine:glass 1
ticks 80
end
smelt hellomine:stone
input hellomine:cobblestone
output hellomine:stone 1
ticks 80
end
fuel hellomine:coal_ore
ticks 160
end
fuel hellomine:plant_fiber
ticks 40
end
)";
}

std::string validFoodDefinitions()
{
    return R"(# HelloMine3D food registry v1
food hellomine:bread
restore 6
cooldown_ticks 20
end
food hellomine:cooked_meat
restore 9
cooldown_ticks 28
end
food hellomine:cactus_salad
restore 4
cooldown_ticks 12
end
food hellomine:trail_ration
restore 14
cooldown_ticks 40
end
)";
}

// ---------------------------------------------------------------------------
// G2 - player and workbench crafting focus boundary
// ---------------------------------------------------------------------------
void caseWorkbenchCrafting()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    const auto directory = freshSaveDirectory("workbench_crafting");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    const glm::ivec3 workbenchPosition{8, 100, 8};
    player.addItem(Material::WORKBENCH_BLOCK, 1);
    const bool placed = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(workbenchPosition));
    check("G2/workbench-material-places-appended-block-id",
          placed &&
              static_cast<BlockId>(world.getBlock(
                  workbenchPosition.x, workbenchPosition.y,
                  workbenchPosition.z).id) == BlockId::Workbench);

    const bool opened = BlockInteractionSystem::useBlock(
        world, player, glm::vec3(workbenchPosition));
    check("G2/workbench-use-opens-three-by-three-session",
          opened && player.hasOpenCrafting() &&
              player.getCraftingGridSize() ==
                  CraftingSession::WorkbenchGridSize &&
              player.getOpenWorkbench().has_value());

    player.addItem(Material::DIRT_BLOCK, 1);
    check("G2/crafting-focus-blocks-world-actions",
          !BlockInteractionSystem::placeBlock(
              world, player, glm::vec3(9.f, 100.f, 8.f)) &&
              !BlockInteractionSystem::breakBlock(
                  world, player, glm::vec3(workbenchPosition)));

    const ChunkBlock workbenchBlock = world.getBlock(
        workbenchPosition.x, workbenchPosition.y, workbenchPosition.z);
    BlockDatabase::get()
        .getDefinition(BlockId::Workbench)
        .behavior->onBroken(world, player, workbenchPosition,
                            workbenchBlock);
    const bool closedByLifecycle = !player.hasOpenCrafting();
    const bool broken = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(workbenchPosition));
    check("G2/closed-workbench-can-be-broken-and-recovered",
          closedByLifecycle && broken && !player.hasOpenCrafting() &&
              static_cast<BlockId>(world.getBlock(
                  workbenchPosition.x, workbenchPosition.y,
                  workbenchPosition.z).id) == BlockId::Air);

    player.openCrafting(CraftingSession::PlayerGridSize);
    PlayerSaveState state = player.getSaveState();
    player.applySaveState(state);
    check("G2/player-crafting-is-two-by-two-and-not-persisted",
          !player.hasOpenCrafting() &&
              player.getCraftingGridSize() == 0 &&
              !player.getOpenWorkbench().has_value());
}

// ---------------------------------------------------------------------------
// G3 - tool tiers, hold-to-mine progress and durability persistence
// ---------------------------------------------------------------------------
void caseToolMiningProgression()
{
    ItemStack empty(Material::NOTHING, 0);
    ItemStack wooden(Material::WOODEN_PICKAXE, 1);
    ItemStack stone(Material::STONE_PICKAXE, 1);
    const BlockMiningEvaluation handStone =
        BlockInteractionSystem::evaluateMining(BlockId::Stone, empty);
    const BlockMiningEvaluation woodStone =
        BlockInteractionSystem::evaluateMining(BlockId::Stone, wooden);
    const BlockMiningEvaluation stoneStone =
        BlockInteractionSystem::evaluateMining(BlockId::Stone, stone);
    check("G3/matching-tools-accelerate-mining-by-tier",
          handStone.requiredSeconds > woodStone.requiredSeconds &&
              woodStone.requiredSeconds > stoneStone.requiredSeconds &&
              !handStone.dropAllowed && woodStone.dropAllowed &&
              stoneStone.dropAllowed);

    const BlockMiningEvaluation woodIron =
        BlockInteractionSystem::evaluateMining(BlockId::IronOre, wooden);
    const BlockMiningEvaluation stoneIron =
        BlockInteractionSystem::evaluateMining(BlockId::IronOre, stone);
    check("G3/required-tier-gates-drops-not-breaking",
          woodIron.matchingClass && !woodIron.meetsTier &&
              !woodIron.dropAllowed && stoneIron.matchingClass &&
              stoneIron.meetsTier && stoneIron.dropAllowed);

    BlockMiningProgress progress;
    const glm::ivec3 firstTarget{1, 2, 3};
    const glm::ivec3 secondTarget{2, 2, 3};
    check("G3/partial-hold-does-not-break",
          !progress.advance(firstTarget, BlockId::Stone,
                            Material::ID::Nothing, 1.5f, 0.1f) &&
              progress.snapshot().active &&
              progress.snapshot().normalized() > 0.0f &&
              progress.snapshot().normalized() < 1.0f);
    progress.advance(secondTarget, BlockId::Stone,
                     Material::ID::Nothing, 1.5f, 0.1f);
    check("G3/target-change-resets-progress",
          progress.snapshot().target == secondTarget &&
              std::abs(progress.snapshot().elapsedSeconds - 0.1f) <
                  0.0001f);
    progress.advance(secondTarget, BlockId::Stone,
                     Material::ID::WoodenPickaxe, 0.75f, 10.0f);
    check("G3/tool-change-resets-and-clamps-frame-progress",
          progress.snapshot().toolMaterialId ==
                  Material::ID::WoodenPickaxe &&
              std::abs(progress.snapshot().elapsedSeconds -
                       BlockMiningProgress::MaxFrameContributionSeconds) <
                  0.0001f);
    progress.cancel();
    check("G3/cancel-clears-mining-progress",
          !progress.snapshot().active &&
              progress.snapshot().normalized() == 0.0f);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    const auto directory = freshSaveDirectory("tool_mining");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);
    player.addItem(Material::WOODEN_PICKAXE, 1, 2);
    world.setBlock(8, 100, 8, BlockId::Stone);
    world.setBlock(9, 100, 8, BlockId::Stone);
    const bool firstBreak = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(8.5f, 100.5f, 8.5f));
    check("G3/successful-break-consumes-one-durability",
          firstBreak && player.getHeldItems().getMaterial().id ==
                            Material::ID::WoodenPickaxe &&
              player.getHeldItems().getDurability() == 1);
    const bool secondBreak = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(9.5f, 100.5f, 8.5f));
    check("G3/broken-tool-advances-to-occupied-slot",
          secondBreak && player.getHeldItems().getMaterial().id ==
                             Material::ID::Cobblestone &&
              player.getHeldItems().getNumInStack() == 2);

    world.setBlock(10, 100, 8, BlockId::IronOre);
    const bool wrongToolBreak = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(10.5f, 100.5f, 8.5f));
    check("G3/wrong-tool-breaks-without-forbidden-drop",
          wrongToolBreak && world.getBlock(10, 100, 8).id == 0 &&
              player.getInventorySlot(0).getMaterial().id !=
                  Material::ID::IronOre &&
              player.getInventorySlot(1).getMaterial().id !=
                  Material::ID::IronOre);

    player.addItem(Material::STONE_PICKAXE, 1);
    PlayerSaveState selectedTool = player.getSaveState();
    selectedTool.heldItem = 0;
    player.applySaveState(selectedTool);
    world.setBlock(11, 100, 8, BlockId::IronOre);
    const bool correctToolBreak = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(11.5f, 100.5f, 8.5f));
    check("G3/stone-pickaxe-harvests-iron-and-loses-durability",
          correctToolBreak && player.getInventorySlot(0).getDurability() ==
                                  31 &&
              player.getSaveState().inventory[2].materialId ==
                  Material::ID::IronOre);

    const auto saveDirectory = freshSaveDirectory("tool_save_contract");
    WorldSaveData toolSave;
    toolSave.worldId = "g3-tool-save";
    toolSave.worldName = "G3 Tool Save";
    toolSave.seed = kValidationSeed;
    toolSave.createdUtc = LegacyWorldTimestampUtc;
    toolSave.lastPlayedUtc = LegacyWorldTimestampUtc;
    toolSave.lastBuildIdentity = "validation";
    toolSave.hasPlayerState = true;
    toolSave.playerState.inventory = {
        {Material::ID::WoodenPickaxe, 1, 7}};
    WorldSave save(saveDirectory);
    WorldSaveData loadedTool;
    const bool validSaved = save.save(toolSave) && save.load(loadedTool);
    WorldSaveData invalidTool = toolSave;
    invalidTool.playerState.inventory[0].durability = 17;
    const bool invalidRejected = !save.save(invalidTool);
    WorldSaveData preservedTool;
    check("G3/save-validates-and-preserves-exact-durability",
          validSaved && loadedTool.playerState.inventory[0].durability == 7 &&
              invalidRejected && save.load(preservedTool) &&
              preservedTool.playerState.inventory[0].durability == 7);

    const auto legacyDirectory =
        freshSaveDirectory("tool_inventory_legacy");
    const std::filesystem::path legacyPath =
        std::filesystem::path(legacyDirectory) / "world.meta";
    {
        std::ifstream fixture(
            ResourcePaths::join(
                ResourcePaths::projectRoot(),
                "tools/fixtures/alpha/world-v3-empty-journey.meta"),
            std::ios::binary);
        std::ofstream legacy(legacyPath,
                             std::ios::binary | std::ios::trunc);
        legacy << fixture.rdbuf();
    }
    WorldSave legacySave(legacyDirectory);
    WorldSaveData legacyData;
    const bool legacyLoaded = legacySave.load(legacyData);
    const int loadedLegacyVersion = legacyData.version;
    if (legacyLoaded) {
        legacyData.version = WorldSaveFormatVersion;
        legacyData.objectiveState.definitionVersion =
            ObjectiveSaveState::CurrentDefinitionVersion;
        legacyData.objectiveState.completedIds =
            ObjectiveState::completedFromLegacyFlags(
                legacyData.alphaJourneyFlags);
        legacyData.objectiveState.progress.clear();
    }
    const bool legacyUpgraded = legacyLoaded && legacySave.save(legacyData);
    std::ifstream upgradedInput(legacyPath, std::ios::binary);
    const std::string upgraded(
        (std::istreambuf_iterator<char>(upgradedInput)),
        std::istreambuf_iterator<char>());
    check("G3/legacy-inventory-migrates-to-durability-format",
          legacyUpgraded && legacyData.playerState.inventory.size() == 1 &&
              legacyData.playerState.inventory[0].durability == 0 &&
              upgraded.find("inventory_format 2") != std::string::npos &&
              upgraded.find("inventory_slot 3 3 0") != std::string::npos);
    check("N1/version-three-migrates-with-empty-objective-state",
          legacyLoaded && loadedLegacyVersion == 3 &&
              legacyData.alphaJourneyFlags == 0u && legacyUpgraded &&
              upgraded.find("version 12") != std::string::npos &&
              upgraded.find("alpha_journey_flags 0") !=
                  std::string::npos &&
              upgraded.find("objective_definition_version 3") !=
                  std::string::npos &&
              upgraded.find("objective_completed_count 0") !=
                  std::string::npos &&
              upgraded.find("objective_progress_count 0") !=
                  std::string::npos);
}

// ---------------------------------------------------------------------------
// P11-1 - minimum building set, openable door and tool-class ownership
// ---------------------------------------------------------------------------
void caseP11MinimumBuildingAndTools()
{
    const auto &database = BlockDatabase::get();
    const auto &closedDoor =
        database.getDefinition(BlockId::OakDoorClosed);
    const auto &openDoor = database.getDefinition(BlockId::OakDoorOpen);
    check("P11-1/appended-identities-preserve-existing-save-values",
          static_cast<int>(BlockId::Torch) == 21 &&
              static_cast<int>(BlockId::OakPlank) == 22 &&
              static_cast<int>(BlockId::Cobblestone) == 23 &&
              static_cast<int>(BlockId::OakDoorClosed) == 24 &&
              static_cast<int>(BlockId::OakDoorOpen) == 25 &&
              static_cast<int>(Material::ID::Torch) == 34 &&
              static_cast<int>(Material::ID::OakPlank) == 35 &&
              static_cast<int>(Material::ID::WoodenShovel) == 39 &&
              static_cast<int>(Material::ID::AncientCompass) == 40 &&
              static_cast<int>(Material::ID::RaiderWard) == 41 &&
              Material::toMaterial(BlockId::OakDoorOpen).id ==
                  Material::ID::OakDoor &&
              Material::OAK_DOOR.toBlockID() ==
                  BlockId::OakDoorClosed);
    check("P11-1/door-states-own-render-collision-and-use-contract",
          closedDoor.collidable && !openDoor.collidable &&
              closedDoor.render.shape.faces.size() == 2 &&
              openDoor.render.shape.faces.size() == 2 &&
              closedDoor.behavior->supportsUse() &&
              openDoor.behavior->supportsUse());

    ItemStack empty(Material::NOTHING, 0);
    ItemStack axe(Material::WOODEN_AXE, 1);
    ItemStack shovel(Material::WOODEN_SHOVEL, 1);
    const BlockMiningEvaluation handOak =
        BlockInteractionSystem::evaluateMining(BlockId::OakBark, empty);
    const BlockMiningEvaluation axeOak =
        BlockInteractionSystem::evaluateMining(BlockId::OakBark, axe);
    const BlockMiningEvaluation shovelOak =
        BlockInteractionSystem::evaluateMining(BlockId::OakBark, shovel);
    const BlockMiningEvaluation axeDirt =
        BlockInteractionSystem::evaluateMining(BlockId::Dirt, axe);
    const BlockMiningEvaluation shovelDirt =
        BlockInteractionSystem::evaluateMining(BlockId::Dirt, shovel);
    check("P11-1/axe-and-shovel-accelerate-only-owned-materials",
          axeOak.matchingClass && !shovelOak.matchingClass &&
              axeOak.requiredSeconds < handOak.requiredSeconds &&
              shovelOak.requiredSeconds == handOak.requiredSeconds &&
              !axeDirt.matchingClass && shovelDirt.matchingClass &&
              shovelDirt.requiredSeconds < axeDirt.requiredSeconds);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("p11_minimum_building");
    Config config = makeConfig();
    Camera camera(config);
    const glm::ivec3 stonePosition{9, 100, 8};
    const glm::ivec3 plankPosition{10, 100, 8};
    const glm::ivec3 doorPosition{11, 100, 8};
    bool saved = false;
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(stonePosition.x, stonePosition.y, stonePosition.z,
                       BlockId::Stone);
        player.addItem(Material::WOODEN_PICKAXE, 1);
        const bool stoneBroken = BlockInteractionSystem::breakBlock(
            world, player, glm::vec3(stonePosition) + glm::vec3(0.5f));
        check("P11-1/stone-harvest-produces-cobblestone-not-stone",
              stoneBroken &&
                  player.getInventoryCount(Material::ID::Cobblestone) == 1 &&
                  player.getInventoryCount(Material::ID::Stone) == 0);

        world.setBlock(plankPosition.x, plankPosition.y, plankPosition.z,
                       BlockId::Air);
        world.setBlock(doorPosition.x, doorPosition.y, doorPosition.z,
                       BlockId::Air);
        player.addItem(Material::OAK_PLANK_BLOCK, 1);
        player.addItem(Material::OAK_DOOR, 1);
        const auto selectMaterial = [&player](Material::ID materialId) {
            for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
                if (player.getInventorySlot(slot).getMaterial().id ==
                    materialId) {
                    PlayerInputState input;
                    input.hotbarSlot = slot;
                    player.applyInput(input);
                    return true;
                }
            }
            return false;
        };
        const bool plankPlaced =
            selectMaterial(Material::ID::OakPlank) &&
            BlockInteractionSystem::placeBlock(
                world, player,
                glm::vec3(plankPosition) + glm::vec3(0.5f));
        const bool doorPlaced =
            selectMaterial(Material::ID::OakDoor) &&
            BlockInteractionSystem::placeBlock(
                world, player,
                glm::vec3(doorPosition) + glm::vec3(0.5f));
        check("P11-1/planks-and-closed-door-place-as-appended-blocks",
              plankPlaced && doorPlaced &&
                  static_cast<BlockId>(world.getBlock(
                      plankPosition.x, plankPosition.y,
                      plankPosition.z).id) == BlockId::OakPlank &&
                  static_cast<BlockId>(world.getBlock(
                      doorPosition.x, doorPosition.y,
                      doorPosition.z).id) == BlockId::OakDoorClosed);

        const bool opened = BlockInteractionSystem::useBlock(
            world, player,
            glm::vec3(doorPosition) + glm::vec3(0.5f));
        check("P11-1/door-use-toggles-to-noncollidable-open-state",
              opened && static_cast<BlockId>(world.getBlock(
                  doorPosition.x, doorPosition.y,
                  doorPosition.z).id) == BlockId::OakDoorOpen &&
                  !database.getDefinition(BlockId::OakDoorOpen).collidable);

        player.addItem(Material::WOODEN_AXE, 1);
        player.addItem(Material::WOODEN_SHOVEL, 1);
        saved = world.save();
    }

    Player restoredPlayer;
    World restoredWorld(camera, config, restoredPlayer, directory, false, 1);
    const bool restoredOpen = static_cast<BlockId>(restoredWorld.getBlock(
        doorPosition.x, doorPosition.y, doorPosition.z).id) ==
        BlockId::OakDoorOpen;
    const bool closed = BlockInteractionSystem::useBlock(
        restoredWorld, restoredPlayer,
        glm::vec3(doorPosition) + glm::vec3(0.5f));
    const bool reopened = BlockInteractionSystem::useBlock(
        restoredWorld, restoredPlayer,
        glm::vec3(doorPosition) + glm::vec3(0.5f));
    const bool recovered = BlockInteractionSystem::breakBlock(
        restoredWorld, restoredPlayer,
        glm::vec3(doorPosition) + glm::vec3(0.5f));
    check("P11-1/new-content-roundtrips-without-save-migration",
          saved && restoredOpen && closed && reopened && recovered &&
              restoredPlayer.getInventoryCount(Material::ID::OakDoor) == 1 &&
              restoredPlayer.getInventoryCount(Material::ID::WoodenAxe) == 1 &&
              restoredPlayer.getInventoryCount(
                  Material::ID::WoodenShovel) == 1 &&
              static_cast<BlockId>(restoredWorld.getBlock(
                  plankPosition.x, plankPosition.y,
                  plankPosition.z).id) == BlockId::OakPlank);
}

// ---------------------------------------------------------------------------
// P11C - parallel first-session opportunities and earned recipe knowledge
// ---------------------------------------------------------------------------
void caseP11CFirstThirtyMinutes()
{
    ensureRuntimeObjectiveRegistry();
    ensureRuntimeRecipeRegistry();
    const ObjectiveRegistry &registry = runtimeObjectiveRegistry();
    check("P11C/version-three-adds-bounded-parallel-opportunities",
          registry.definitionVersion() ==
                  ObjectiveSaveState::CurrentDefinitionVersion &&
              registry.definitions().size() == 34 &&
              ObjectiveSystem::MaxVisibleOpportunities == 3 &&
              registry.find("shelter.craft_planks") != nullptr &&
              registry.find("exploration.find_coal") != nullptr);

    const auto hasOpportunity = [](const ObjectiveSnapshot &snapshot,
                                   const std::string &id) {
        return std::any_of(
            snapshot.opportunities.begin(),
            snapshot.opportunities.end(),
            [&id](const ObjectiveOpportunitySnapshot &opportunity) {
                return opportunity.id == id;
            });
    };
    const auto hasRecipe = [](const RecipeDiscoverySnapshot &snapshot,
                              const std::string &id) {
        return std::find(snapshot.discoveredIds.begin(),
                         snapshot.discoveredIds.end(), id) !=
               snapshot.discoveredIds.end();
    };

    ObjectiveSaveState branchedState;
    std::uint32_t branchedFlags = 0;
    {
        Player player;
        SandboxEventBus eventBus;
        ObjectiveSystem objectives(registry, player, eventBus, {}, 0u,
                                   false);
        const ObjectiveSnapshot initial = objectives.snapshot();
        check("P11C/new-session-begins-with-one-clear-opportunity",
              initial.currentId == "alpha.gather_wood" &&
                  initial.opportunities.size() == 1 &&
                  initial.totalObjectives == 33 &&
                  objectives.recipeDiscoverySnapshot()
                          .discoveredIds.empty());

        const int added = player.addItem(Material::OAK_BARK_BLOCK, 11);
        eventBus.publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, Material::ID::OakBark, added,
            "p11c_first_material"));
        for (int count = 0; count < 11; ++count)
        {
            eventBus.publish(
                BlockBreakEvent({count, 0, 0}, BlockId::OakBark));
        }
        const ObjectiveSnapshot opened = objectives.snapshot();
        check("P11C/wood-opens-growth-shelter-and-exploration",
              opened.opportunities.size() == 3 &&
                  hasOpportunity(opened, "alpha.craft_workbench") &&
                  hasOpportunity(opened, "shelter.craft_planks") &&
                  hasOpportunity(opened, "exploration.find_coal"));

        const RecipeDiscoverySnapshot barkKnowledge =
            objectives.recipeDiscoverySnapshot();
        check("P11C/material-acquisition-reveals-related-recipes-only",
              barkKnowledge.totalRecipes ==
                      runtimeRecipeRegistry().recipes().size() &&
                  hasRecipe(barkKnowledge, "hellomine:oak_planks") &&
                  hasRecipe(barkKnowledge, "hellomine:workbench") &&
                  !hasRecipe(barkKnowledge, "hellomine:bread"));

        eventBus.publish(CraftCompletedEvent(
            "hellomine:workbench", Material::ID::Workbench, 1, 1, {}));
        eventBus.publish(CraftCompletedEvent(
            "hellomine:oak_planks", Material::ID::OakPlank, 1, 4, {}));
        for (int count = 0; count < 3; ++count)
        {
            eventBus.publish(
                BlockPlaceEvent({count, 1, 0}, BlockId::OakPlank));
        }
        const ObjectiveSnapshot switched = objectives.snapshot();
        check("P11C/branch-progress-survives-primary-opportunity-switching",
              switched.currentId == "alpha.place_workbench" &&
                  objectives.progress("shelter.place_planks") == 3 &&
                  hasOpportunity(switched, "shelter.place_planks") &&
                  hasOpportunity(switched, "exploration.find_coal"));

        branchedState = objectives.saveState();
        branchedFlags = objectives.legacyAlphaFlags();
        const std::string plankToken =
            ObjectiveSystem::recipeDiscoveryToken(
                "hellomine:oak_planks");
        check("P11C/discovery-token-is-canonical-bounded-and-persisted",
              plankToken.size() == 23 &&
                  ObjectiveState::isCanonicalId(plankToken) &&
                  std::find(branchedState.completedIds.begin(),
                            branchedState.completedIds.end(),
                            plankToken) != branchedState.completedIds.end());
    }

    ObjectiveSaveState learnedState;
    {
        Player restoredPlayer;
        SandboxEventBus restoredBus;
        ObjectiveSystem restored(registry, restoredPlayer, restoredBus,
                                 branchedState, branchedFlags, false);
        const ObjectiveSnapshot restoredSnapshot = restored.snapshot();
        const bool wheatAdded =
            restoredPlayer.addItem(Material::WHEAT, 1) == 1;
        restoredBus.publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, Material::ID::Wheat, 1,
            "p11c_new_material"));
        learnedState = restored.saveState();
        check("P11C/branches-and-recipe-knowledge-restore-independently",
              restoredSnapshot.currentId == "alpha.place_workbench" &&
                  restored.progress("shelter.place_planks") == 3 &&
                  restored.isRecipeDiscovered(
                      "hellomine:oak_planks") &&
                  !branchedState.completedIds.empty() && wheatAdded &&
                  restored.isRecipeDiscovered("hellomine:bread"));
    }

    const auto saveDirectory =
        freshSaveDirectory("p11c_objective_v2_migration");
    WorldSaveData migrationData;
    migrationData.worldId = "p11c-objective-v2";
    migrationData.worldName = "P11C Objective V2";
    migrationData.seed = kValidationSeed;
    migrationData.createdUtc = LegacyWorldTimestampUtc;
    migrationData.lastPlayedUtc = LegacyWorldTimestampUtc;
    migrationData.lastBuildIdentity = "validation";
    migrationData.alphaJourneyFlags = branchedFlags;
    migrationData.objectiveState = learnedState;
    WorldSave migrationSave(saveDirectory);
    const bool currentSaved = migrationSave.save(migrationData);
    std::string metadata = readTextFile(migrationSave.metadataPath());
    const std::string currentField =
        "objective_definition_version 3";
    const std::size_t versionPosition = metadata.find(currentField);
    bool downgraded = versionPosition != std::string::npos;
    if (downgraded)
    {
        metadata.replace(versionPosition, currentField.size(),
                         "objective_definition_version 2");
        std::ofstream output(migrationSave.metadataPath(),
                             std::ios::binary | std::ios::trunc);
        output << metadata;
        downgraded = output.good();
    }
    WorldSaveData migrated;
    const bool migratedLoaded =
        downgraded && migrationSave.load(migrated);
    const std::string breadToken =
        ObjectiveSystem::recipeDiscoveryToken("hellomine:bread");
    check("P11C/version-two-state-migrates-with-discoveries-intact",
          currentSaved && migratedLoaded &&
              migrated.objectiveState.definitionVersion ==
                  ObjectiveSaveState::CurrentDefinitionVersion &&
              std::find(migrated.objectiveState.completedIds.begin(),
                        migrated.objectiveState.completedIds.end(),
                        breadToken) !=
                  migrated.objectiveState.completedIds.end());

    const bool foodsRemainRecoveryOnly = std::all_of(
        runtimeFoodRegistry().foods().begin(),
        runtimeFoodRegistry().foods().end(),
        [](const FoodDefinition &food) {
            return food.healthRestored > 0.f && food.cooldownTicks > 0;
        });
    check("P11C/food-remains-bounded-health-recovery-without-hunger",
          foodsRemainRecoveryOnly &&
              migrationData.playerState.health == 20.f &&
              migrationData.playerState.foodCooldownTicks == 0);
}

// ---------------------------------------------------------------------------
// P11D - structure-exclusive rewards that change later player decisions
// ---------------------------------------------------------------------------
void caseP11DExplorationRewards()
{
    StructurePlanSnapshot ruin;
    ruin.valid = true;
    ruin.hasChest = true;
    ruin.key = {StructureType::Ruin, CurrentTerrainGenerationVersion, 2, 3};
    ruin.chestPosition = {144, 72, 208};
    ruin.selectionHash = 0x13579bdf2468ace0ull;
    StructurePlanSnapshot camp = ruin;
    camp.key.type = StructureType::RaiderCamp;
    camp.key.cellX = 4;
    camp.key.cellZ = -2;
    camp.selectionHash = 0x2468ace013579bdfull;

    const StructureLootSnapshot legacyRuin = structureLootForPlan(
        ruin, ExplorationRewards::LegacyVersion);
    const StructureLootSnapshot currentRuin = structureLootForPlan(
        ruin, ExplorationRewards::CurrentVersion);
    const StructureLootSnapshot legacyCamp = structureLootForPlan(
        camp, ExplorationRewards::LegacyVersion);
    const StructureLootSnapshot currentCamp = structureLootForPlan(
        camp, ExplorationRewards::CurrentVersion);
    check("P11D/reward-version-preserves-old-loot-and-adds-unique-results",
          legacyRuin.valid && currentRuin.valid && legacyCamp.valid &&
              currentCamp.valid && legacyRuin.entries.size() == 3 &&
              currentRuin.entries.size() == 3 &&
              legacyCamp.entries.size() == 3 &&
              currentCamp.entries.size() == 3 &&
              legacyRuin.entries[0].materialId == Material::ID::IronIngot &&
              currentRuin.entries[0] == StructureLootEntry{
                  Material::ID::AncientCompass, 1} &&
              legacyCamp.entries[0].materialId == Material::ID::Bread &&
              currentCamp.entries[0] == StructureLootEntry{
                  Material::ID::RaiderWard, 1} &&
              currentRuin.entries[1] == legacyRuin.entries[1] &&
              currentRuin.entries[2] == legacyRuin.entries[2] &&
              currentCamp.entries[1] == legacyCamp.entries[1] &&
              currentCamp.entries[2] == legacyCamp.entries[2]);

    ClassicOverWorldGenerator legacyGenerator(
        20260830, CurrentTerrainGenerationVersion,
        ExplorationRewards::LegacyVersion);
    ClassicOverWorldGenerator currentGenerator(
        20260830, CurrentTerrainGenerationVersion,
        ExplorationRewards::CurrentVersion);
    check("P11D/generator-carries-reward-identity-without-changing-terrain",
          legacyGenerator.getGenerationVersion() ==
                  CurrentTerrainGenerationVersion &&
              currentGenerator.getGenerationVersion() ==
                  CurrentTerrainGenerationVersion &&
              legacyGenerator.getExplorationRewardVersion() ==
                  ExplorationRewards::LegacyVersion &&
              currentGenerator.getExplorationRewardVersion() ==
                  ExplorationRewards::CurrentVersion);

    const std::string saveDirectory =
        freshSaveDirectory("p11d_reward_v12");
    WorldSaveData current;
    current.worldId = "p11d-reward-v12";
    current.worldName = "P11D Reward V12";
    current.seed = 20260830;
    current.createdUtc = LegacyWorldTimestampUtc;
    current.lastPlayedUtc = LegacyWorldTimestampUtc;
    current.lastBuildIdentity = "p11d-test";
    current.hasPlayerState = true;
    current.playerState.inventory = {
        {Material::ID::AncientCompass, 1, 0},
        {Material::ID::RaiderWard, 1, 0}};
    WorldSave currentSave(saveDirectory);
    WorldSaveData currentRoundTrip;
    const bool currentSaved = currentSave.save(current) &&
        currentSave.load(currentRoundTrip);
    check("P11D/save-v12-roundtrips-reward-version-and-items",
          currentSaved && currentRoundTrip.version == 12 &&
              currentRoundTrip.explorationRewardVersion ==
                  ExplorationRewards::CurrentVersion &&
              currentRoundTrip.playerState.inventory ==
                  current.playerState.inventory);

    const std::string currentText = readTextFile(currentSave.metadataPath());
    const auto removeField = [](std::string text,
                                const std::string &field) {
        const std::size_t begin = text.find(field);
        if (begin != std::string::npos) {
            const std::size_t end = text.find('\n', begin);
            text.erase(begin, end == std::string::npos
                                  ? text.size() - begin : end - begin + 1);
        }
        return text;
    };
    const std::filesystem::path malformedRoot =
        freshSaveDirectory("p11d_reward_malformed");
    const std::filesystem::path missingPath = malformedRoot / "missing.meta";
    const std::filesystem::path invalidPath = malformedRoot / "invalid.meta";
    {
        std::ofstream missing(missingPath,
                              std::ios::binary | std::ios::trunc);
        missing << removeField(currentText, "exploration_reward_version ");
        std::string invalid = currentText;
        const std::size_t position = invalid.find(
            "exploration_reward_version 1");
        if (position != std::string::npos) {
            invalid.replace(position,
                            std::string("exploration_reward_version 1").size(),
                            "exploration_reward_version 2");
        }
        std::ofstream invalidFile(invalidPath,
                                  std::ios::binary | std::ios::trunc);
        invalidFile << invalid;
    }
    WorldSaveData malformed;
    std::string malformedError;
    check("P11D/current-save-requires-bounded-reward-version",
          !WorldSave::loadFromPath(
              missingPath.string(), malformed, &malformedError) &&
              !WorldSave::loadFromPath(
                  invalidPath.string(), malformed, &malformedError));

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("p11d_reward_v11_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "p11d-reward-v11";
    std::filesystem::create_directories(migratedWorld);
    std::string versionEleven = removeField(
        currentText, "exploration_reward_version ");
    const std::size_t versionPosition = versionEleven.find("version 12");
    if (versionPosition != std::string::npos) {
        versionEleven.replace(versionPosition, 10, "version 11");
    }
    const std::size_t idPosition = versionEleven.find(
        "world_id p11d-reward-v12");
    if (idPosition != std::string::npos) {
        versionEleven.replace(
            idPosition, std::string("world_id p11d-reward-v12").size(),
            "world_id p11d-reward-v11");
    }
    {
        std::ofstream legacy(migratedWorld / "world.meta",
                             std::ios::binary | std::ios::trunc);
        legacy << versionEleven;
    }
    WorldSaveData legacyLoaded;
    const bool readLegacy =
        WorldSave(migratedWorld.string()).load(legacyLoaded);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("p11d-reward-v11");
    WorldSaveData migratedData;
    const bool reopened = migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    check("P11D/v11-worlds-migrate-with-legacy-loot-identity",
          readLegacy && legacyLoaded.version == 11 &&
              legacyLoaded.explorationRewardVersion ==
                  ExplorationRewards::LegacyVersion && reopened &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.explorationRewardVersion ==
                  ExplorationRewards::LegacyVersion);

    setEnv("HELLOMINE3D_SEED", "20260830");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 100 8.5");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);
    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("p11d_compass"), false, 0);
        player.position = world.getPlayerSpawnPoint() +
            glm::vec3(10.f, 0.f, 10.f);
        player.addItem(Material::ANCIENT_COMPASS, 1);
        player.addItem(Material::RAIDER_WARD, 1);
        const ExplorationRewardSnapshot snapshot =
            world.getExplorationRewardSnapshot();
        check("P11D/compass-points-home-and-ward-reduces-guard-recovery",
              snapshot.version == ExplorationRewards::CurrentVersion &&
                  snapshot.ancientCompassHeld &&
                  snapshot.raiderWardCarried &&
                  snapshot.homeDirection == "NW" &&
                  std::abs(snapshot.homeDistance - 14.142136f) < 0.01f &&
                  snapshot.guardRecoverTicks ==
                      ExplorationRewards::RaiderWardGuardRecoverTicks &&
                  world.getPlayerGuardRecoverDurationTicks() ==
                      ExplorationRewards::RaiderWardGuardRecoverTicks);
    }
    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("p11d_guard"), false, 1);
        player.position = {8.5f, 100.f, 8.5f};
        for (int x = 6; x <= 10; ++x) {
            for (int z = 6; z <= 10; ++z) {
                world.setBlock(x, 99, z, {BlockId::Stone});
                world.setBlock(x, 100, z, {BlockId::Air});
                world.setBlock(x, 101, z, {BlockId::Air});
            }
        }
        player.addItem(Material::WOODEN_SWORD, 1);
        player.addItem(Material::RAIDER_WARD, 1);
        PlayerInputState input;
        input.hotbarSlot = 0;
        player.applyInput(input);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType,
            player.position + glm::vec3(0.f, 0.f, -1.1f));
        world.setPlayerGuarding(true);
        for (int tick = 1; tick <= 7; ++tick) {
            world.tick(tick);
        }
        const MobActor *mob = dynamic_cast<const MobActor *>(
            world.getActorManager().findActor(mobId));
        const WorldDebugStats stats = world.collectDebugStats();
        check("P11D/ward-applies-to-real-melee-guard-path",
              mob != nullptr && world.getPlayerHealth() == 20.f &&
                  stats.combatFeedback.kind ==
                      PlayerCombatFeedbackKind::Guard &&
                  stats.combatFeedback.guardRecoverTicksRemaining ==
                      ExplorationRewards::RaiderWardGuardRecoverTicks);
    }
    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "");
}

// ---------------------------------------------------------------------------
// P11-2 - terrain-v4 mountain relief and discoverable natural cave mouths
// ---------------------------------------------------------------------------
void caseP11TerrainContoursAndEntrances()
{
    check("P11-2/terrain-version-contract-is-append-only",
          LegacyTerrainGenerationVersion == 1 &&
              WaystoneTerrainGenerationVersion == 2 &&
              ExplorationSiteTerrainGenerationVersion == 3 &&
              MountainTerrainGenerationVersion == 4 &&
              CurrentTerrainGenerationVersion ==
                  MountainTerrainGenerationVersion);

    ClassicOverWorldGenerator terrainV3(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion);
    ClassicOverWorldGenerator terrainV4(
        kValidationSeed, MountainTerrainGenerationVersion);
    ClassicOverWorldGenerator repeatedV4(
        kValidationSeed, MountainTerrainGenerationVersion);
    ClassicOverWorldGenerator otherSeedV4(
        kValidationSeed + 1, MountainTerrainGenerationVersion);
    check("P11-2/terrain-v3-height-and-biome-output-remains-frozen",
          terrainV3.getSurfaceHeightAtWorld(18, 328) == 70 &&
              terrainV3.getSurfaceHeightAtWorld(-128, -128) ==
                  WATER_LEVEL - 1 &&
              terrainV3.getBiomeAtWorld(18, 328) !=
                  TerrainBiome::Mountain &&
              terrainV3.getBiomeAtWorld(-128, -128) !=
                  TerrainBiome::Mountain);

    int minimumHeight = 10000;
    int maximumHeight = -1;
    int mountainSamples = 0;
    int deterministicMismatches = 0;
    int seedDifferences = 0;
    for (int z = -2048; z <= 2048; z += 32) {
        for (int x = -2048; x <= 2048; x += 32) {
            const int height = terrainV4.getSurfaceHeightAtWorld(x, z);
            minimumHeight = std::min(minimumHeight, height);
            maximumHeight = std::max(maximumHeight, height);
            mountainSamples += terrainV4.getBiomeAtWorld(x, z) ==
                TerrainBiome::Mountain ? 1 : 0;
            deterministicMismatches += height !=
                repeatedV4.getSurfaceHeightAtWorld(x, z) ? 1 : 0;
            seedDifferences += height !=
                otherSeedV4.getSurfaceHeightAtWorld(x, z) ? 1 : 0;
        }
    }
    check("P11-2/v4-adds-deterministic-mountain-relief",
          deterministicMismatches == 0 && mountainSamples > 200 &&
              maximumHeight >= WATER_LEVEL + 56 &&
              maximumHeight - minimumHeight >= 64,
          "range=" + std::to_string(minimumHeight) + ".." +
              std::to_string(maximumHeight) + " mountain=" +
              std::to_string(mountainSamples));
    check("P11-2/v4-height-domain-remains-seed-sensitive",
          seedDifferences > 1000,
          "differences=" + std::to_string(seedDifferences));
    check("P11-2/mountain-reuses-bounded-ecology-and-pressure",
          TerrainAppearance::ecologyRow(TerrainBiome::Mountain) ==
                  TerrainAppearance::ecologyRow(
                      TerrainBiome::TemperateForest) &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::Mountain)) == World::BruteMobType);

    CaveGenerator caveV4(kValidationSeed,
                         MountainTerrainGenerationVersion);
    CaveGenerator repeatedCaveV4(kValidationSeed,
                                 MountainTerrainGenerationVersion);
    CaveGenerator caveV3(kValidationSeed,
                         ExplorationSiteTerrainGenerationVersion);
    const auto surfaceV4 = [&terrainV4](int x, int z) {
        return terrainV4.getSurfaceHeightAtWorld(x, z);
    };
    const auto biomeV4 = [&terrainV4](int x, int z) {
        return terrainV4.getBiomeAtWorld(x, z);
    };
    CaveGenerator::NaturalEntrance entrance;
    for (int radius = 0; radius <= 32 && !entrance.valid; ++radius) {
        for (int cellX = -radius; cellX <= radius && !entrance.valid;
             ++cellX) {
            for (int cellZ = -radius; cellZ <= radius; ++cellZ) {
                if (radius > 0 && std::abs(cellX) != radius &&
                    std::abs(cellZ) != radius) {
                    continue;
                }
                entrance = caveV4.getNaturalEntranceForCell(
                    cellX, cellZ, surfaceV4, biomeV4);
                if (entrance.valid) {
                    break;
                }
            }
        }
    }
    const CaveGenerator::NaturalEntrance repeatedEntrance =
        repeatedCaveV4.getNaturalEntranceForCell(
            entrance.cellX, entrance.cellZ, surfaceV4, biomeV4);
    const CaveGenerator::NaturalEntrance legacyEntrance =
        caveV3.getNaturalEntranceForCell(
            entrance.cellX, entrance.cellZ, surfaceV4, biomeV4);
    const bool sameEntrance = entrance.valid && repeatedEntrance.valid &&
        entrance.cellX == repeatedEntrance.cellX &&
        entrance.cellZ == repeatedEntrance.cellZ &&
        entrance.anchorX == repeatedEntrance.anchorX &&
        entrance.anchorY == repeatedEntrance.anchorY &&
        entrance.anchorZ == repeatedEntrance.anchorZ &&
        entrance.directionX == repeatedEntrance.directionX &&
        entrance.directionZ == repeatedEntrance.directionZ &&
        entrance.endY == repeatedEntrance.endY;
    check("P11-2/natural-entrance-plan-is-bounded-and-deterministic",
          sameEntrance && !legacyEntrance.valid &&
              biomeV4(entrance.anchorX, entrance.anchorZ) ==
                  TerrainBiome::Mountain &&
              entrance.anchorY >= WATER_LEVEL + 16 &&
              entrance.anchorY - entrance.endY >= 18 &&
              std::abs(entrance.directionX) +
                      std::abs(entrance.directionZ) == 1,
          "cell=" + std::to_string(entrance.cellX) + "," +
              std::to_string(entrance.cellZ) + " anchor=" +
              std::to_string(entrance.anchorX) + "," +
              std::to_string(entrance.anchorY) + "," +
              std::to_string(entrance.anchorZ));

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto sampleEntrance = [&](const std::string &name,
                                    bool reverseLoad) {
        Config config = makeConfig();
        Camera camera(config);
        Player player;
        World world(camera, config, player, freshSaveDirectory(name),
                    false, 0);
        const int endX = entrance.anchorX + entrance.directionX *
            CaveGenerator::EntranceTunnelLength;
        const int endZ = entrance.anchorZ + entrance.directionZ *
            CaveGenerator::EntranceTunnelLength;
        const int minimumChunkX = World::floorDiv(
            std::min(entrance.anchorX, endX) - 3, CHUNK_SIZE);
        const int maximumChunkX = World::floorDiv(
            std::max(entrance.anchorX, endX) + 3, CHUNK_SIZE);
        const int minimumChunkZ = World::floorDiv(
            std::min(entrance.anchorZ, endZ) - 3, CHUNK_SIZE);
        const int maximumChunkZ = World::floorDiv(
            std::max(entrance.anchorZ, endZ) + 3, CHUNK_SIZE);
        std::vector<glm::ivec2> chunks;
        for (int chunkX = minimumChunkX; chunkX <= maximumChunkX;
             ++chunkX) {
            for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ;
                 ++chunkZ) {
                chunks.push_back({chunkX, chunkZ});
            }
        }
        if (reverseLoad) {
            std::reverse(chunks.begin(), chunks.end());
        }
        for (const glm::ivec2 &chunk : chunks) {
            world.getChunkManager().loadChunk(chunk.x, chunk.y);
        }

        std::vector<Block_t> sample;
        for (int step = 0;
             step <= CaveGenerator::EntranceTunnelLength; ++step) {
            const int x = entrance.anchorX + entrance.directionX * step;
            const int z = entrance.anchorZ + entrance.directionZ * step;
            const int floorY = entrance.anchorY - step * 3 / 4;
            sample.push_back(world.getBlock(x, floorY, z).id);
            sample.push_back(world.getBlock(x, floorY + 1, z).id);
            sample.push_back(world.getBlock(x, floorY + 2, z).id);
        }
        sample.push_back(world.getBlock(
            entrance.anchorX, entrance.anchorY - 1,
            entrance.anchorZ).id);
        sample.push_back(world.getBlock(endX, entrance.endY, endZ).id);
        return sample;
    };
    std::vector<Block_t> forward;
    std::vector<Block_t> reverse;
    if (entrance.valid) {
        forward = sampleEntrance("p11_2_entrance_forward", false);
        reverse = sampleEntrance("p11_2_entrance_reverse", true);
    }
    const bool tunnelOpen = forward.size() ==
        (CaveGenerator::EntranceTunnelLength + 1) * 3 + 2 &&
        std::all_of(forward.begin(), forward.end() - 2,
                    [](Block_t block) {
                        return block == static_cast<Block_t>(BlockId::Air);
                    }) &&
        forward[forward.size() - 2] !=
            static_cast<Block_t>(BlockId::Air) &&
        forward.back() == static_cast<Block_t>(BlockId::Air);
    check("P11-2/generated-tunnel-opens-surface-and-underground-chamber",
          tunnelOpen,
          "samples=" + std::to_string(forward.size()));
    check("P11-2/entrance-output-ignores-chunk-load-order",
          !forward.empty() && forward == reverse);

    WorldSaveData current;
    current.worldId = "p11-2-terrain-v4";
    current.worldName = "P11-2 Terrain V4";
    current.seed = kValidationSeed;
    current.createdUtc = LegacyWorldTimestampUtc;
    current.lastPlayedUtc = LegacyWorldTimestampUtc;
    current.lastBuildIdentity = "p11-2-test";
    current.hasPlayerState = true;
    WorldSave currentSave(freshSaveDirectory("p11_2_save_v4"));
    WorldSaveData currentRoundTrip;
    const bool currentSaved = currentSave.save(current) &&
        currentSave.load(currentRoundTrip);
    WorldSaveData preservedV3 = current;
    preservedV3.worldId = "p11-2-terrain-v3";
    preservedV3.worldName = "P11-2 Terrain V3";
    preservedV3.terrainGenerationVersion =
        ExplorationSiteTerrainGenerationVersion;
    WorldSave v3Save(freshSaveDirectory("p11_2_save_v3"));
    WorldSaveData v3RoundTrip;
    const bool v3Saved = v3Save.save(preservedV3) &&
        v3Save.load(v3RoundTrip);
    check("P11-2/save-v12-preserves-created-terrain-identity",
          currentSaved && v3Saved &&
              currentRoundTrip.terrainGenerationVersion ==
                  MountainTerrainGenerationVersion &&
              v3RoundTrip.terrainGenerationVersion ==
                  ExplorationSiteTerrainGenerationVersion &&
              currentRoundTrip.version == WorldSaveFormatVersion &&
              v3RoundTrip.version == WorldSaveFormatVersion);

    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "");
}

// ---------------------------------------------------------------------------
// P11E - enemy silhouettes, combat poses, death presentation and resonance
// ---------------------------------------------------------------------------
void caseP11EEnemyPresentationAndResonance()
{
    const EnemyVisualProfile stalker =
        EnemyPresentation::profileForType(World::StalkerMobType);
    const EnemyVisualProfile brute =
        EnemyPresentation::profileForType(World::BruteMobType);
    const EnemyVisualProfile spitter =
        EnemyPresentation::profileForType(World::SpitterMobType);
    const EnemyVisualProfile guardian =
        EnemyPresentation::profileForType(
            WaystoneEncounter::StalkerType);
    check("P11E/enemy-silhouettes-are-distinct-and-bounded",
          stalker.archetype == EnemyVisualArchetype::Stalker &&
              brute.archetype == EnemyVisualArchetype::Brute &&
              spitter.archetype == EnemyVisualArchetype::Spitter &&
              stalker.partCount == 6 && brute.partCount == 6 &&
              spitter.partCount == 7 && guardian.partCount == 7 &&
              guardian.waystoneGuardian &&
              guardian.parts[guardian.partCount - 1].role ==
                  EnemyVisualPartRole::Crest &&
              EnemyVisualProfile::MaximumParts == 8);

    const auto partIndex = [](const EnemyVisualProfile &profile,
                              EnemyVisualPartRole role) {
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            if (profile.parts[index].role == role) {
                return index;
            }
        }
        return profile.partCount;
    };
    ActorSnapshot poseSnapshot;
    poseSnapshot.id = 7;
    poseSnapshot.type = World::StalkerMobType;
    poseSnapshot.position = {2.f, 3.f, 4.f};
    poseSnapshot.dimensions = {0.3f, 0.75f, 0.3f};
    poseSnapshot.combatant = true;
    poseSnapshot.combatState = MobCombatState::Windup;
    poseSnapshot.combatStateTicksRemaining = 3;
    poseSnapshot.combatStateTicksTotal = 6;
    const EnemyVisualPose windup =
        EnemyPresentation::poseFor(poseSnapshot, stalker);
    const std::size_t rightArm = partIndex(
        stalker, EnemyVisualPartRole::RightArm);
    poseSnapshot.combatState = MobCombatState::Recover;
    const EnemyVisualPose recover =
        EnemyPresentation::poseFor(poseSnapshot, stalker);
    poseSnapshot.combatState = MobCombatState::Chase;
    poseSnapshot.combatStateTicksRemaining = 0;
    poseSnapshot.combatStateTicksTotal = 0;
    const EnemyVisualPose chase =
        EnemyPresentation::poseFor(poseSnapshot, stalker);
    check("P11E/gameplay-state-drives-readable-combat-poses",
          rightArm < stalker.partCount &&
              windup.rotations[rightArm].x < -70.f &&
              recover.rotations[rightArm].x > 0.f &&
              std::abs(chase.rotations[rightArm].x) > 0.1f &&
              windup.rootPitch < 0.f);

    poseSnapshot.deathPresentation = true;
    poseSnapshot.deathPresentationTicksRemaining = 4;
    poseSnapshot.deathPresentationTicksTotal =
        EnemyPresentation::DeathPoseTicks;
    const EnemyVisualPose death =
        EnemyPresentation::poseFor(poseSnapshot, stalker);
    check("P11E/death-pose-is-bounded-and-presentation-only",
          EnemyPresentation::DeathPoseTicks == 8 &&
              EnemyPresentation::MaximumDeathPoses == 32 &&
              std::abs(death.rootRoll) >= 40.f &&
              death.rootYOffset < 0.f && death.rootScale > 0.7f &&
              poseSnapshot.position == glm::vec3(2.f, 3.f, 4.f));

    EnemyRegistry productionEnemies;
    bool productionLoaded = false;
    try {
        productionEnemies.freeze({{
            "media/enemies/Base.enemy",
            readTextFile(ResourcePaths::media("enemies/Base.enemy"))}});
        productionLoaded = true;
    }
    catch (const std::exception &) {
    }
    const auto identityLoot = [&productionEnemies](
                                  const std::string &type,
                                  Material::ID material) {
        const EnemyDefinition *definition =
            productionEnemies.find(type);
        return definition != nullptr && std::any_of(
            definition->loot.begin(), definition->loot.end(),
            [material](const EnemyLootDefinition &loot) {
                return loot.materialId == material;
            });
    };
    bool dirtFree = productionLoaded;
    for (const EnemyDefinition &definition : productionEnemies.enemies()) {
        dirtFree = dirtFree && std::none_of(
            definition.loot.begin(), definition.loot.end(),
            [](const EnemyLootDefinition &loot) {
                return loot.materialId == Material::ID::Dirt;
            });
    }
    check("P11E/enemy-loot-identities-use-existing-materials",
          productionLoaded && dirtFree &&
              identityLoot("hellomine:natural_mob",
                           Material::ID::PlantFiber) &&
              identityLoot(World::StalkerMobType,
                           Material::ID::Wheat) &&
              identityLoot(World::BruteMobType,
                           Material::ID::CoalOre) &&
              identityLoot(World::SpitterMobType,
                           Material::ID::WheatSeeds) &&
              identityLoot(WaystoneEncounter::StalkerType,
                           Material::ID::IronOre) &&
              identityLoot(WaystoneEncounter::BruteType,
                           Material::ID::IronIngot));

    LocalizedTextRegistry resonanceText;
    resonanceText.freeze({
        {"en-US.text", readTextFile(
             ResourcePaths::media("text/en-US.text"))},
        {"zh-CN.text", readTextFile(
             ResourcePaths::media("text/zh-CN.text"))}});
    check("P11E/resonance-feedback-is-bilingual-and-semantic",
          std::string(WaystoneEncounter::feedbackKey(
              WaystoneActionResult::ResonancePulse)) ==
                  "waystone.feedback.resonance_pulse" &&
              resonanceText.hasKey(
                  "en-US", "waystone.feedback.resonance_pulse") &&
              resonanceText.hasKey(
                  "zh-CN", "waystone.feedback.resonance_pulse") &&
              resonanceText.hasKey(
                  "en-US", "waystone.feedback.resonance_charging") &&
              resonanceText.hasKey(
                  "zh-CN", "waystone.feedback.resonance_no_target"));

    setEnv("HELLOMINE3D_SEED", "20260830");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8.5 100 8.5");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);
    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("p11e_death_pose"), false, 1);
        const ActorId id = world.spawnMob(
            World::StalkerMobType,
            player.position + glm::vec3(1.f, 0.f, 0.f));
        const bool killed = world.attackActor(id, 100.f);
        world.tick(1);
        const std::vector<ActorSnapshot> afterDeath =
            world.collectActorSnapshots();
        const auto deathSnapshot = std::find_if(
            afterDeath.begin(), afterDeath.end(),
            [id](const ActorSnapshot &snapshot) {
                return snapshot.id == id &&
                       snapshot.deathPresentation;
            });
        const std::vector<ActorSaveState> saveStates =
            world.getActorManager().collectSaveStates();
        const bool excludedFromSave = std::none_of(
            saveStates.begin(), saveStates.end(),
            [id](const ActorSaveState &state) {
                return state.id == id;
            });
        check("P11E/dead-mob-becomes-transient-render-snapshot",
              killed &&
                  world.getActorManager().findActor(id) == nullptr &&
                  deathSnapshot != afterDeath.end() &&
                  deathSnapshot->deathPresentationTicksRemaining ==
                      EnemyPresentation::DeathPoseTicks &&
                  excludedFromSave);
        for (int tick = 2;
             tick <= EnemyPresentation::DeathPoseTicks + 1; ++tick) {
            world.tick(tick);
        }
        const std::vector<ActorSnapshot> expired =
            world.collectActorSnapshots();
        check("P11E/death-presentation-expires-after-eight-ticks",
              std::none_of(
                  expired.begin(), expired.end(),
                  [id](const ActorSnapshot &snapshot) {
                      return snapshot.id == id;
                  }));
    }

    const std::string resonanceDirectory =
        freshSaveDirectory("p11e_waystone_resonance");
    const glm::ivec3 core{9, 100, 8};
    bool resonanceSaved = false;
    {
        Player player;
        World world(camera, config, player, resonanceDirectory, false, 1);
        for (int x = 1; x <= 15; ++x) {
            for (int z = 1; z <= 15; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
            }
        }
        world.setBlock(core.x, core.y, core.z, BlockId::WaystoneCore);
        const bool ready = world.initializeWaystone(core) &&
            player.addItem(Material::IRON_INGOT,
                           WaystoneEncounter::ActivationIronIngots) ==
                WaystoneEncounter::ActivationIronIngots &&
            world.useWaystone(core, player, true) ==
                WaystoneActionResult::Activated &&
            world.useWaystone(core, player, true) ==
                WaystoneActionResult::EncounterStarted;

        int tick = 1;
        std::vector<ActorId> windupIds;
        while (ready && tick <= 80 && windupIds.empty()) {
            world.tick(tick++);
            for (const ActorSnapshot &snapshot :
                 world.collectActorSnapshots()) {
                if (snapshot.type == WaystoneEncounter::StalkerType &&
                    snapshot.combatState == MobCombatState::Windup) {
                    windupIds.push_back(snapshot.id);
                }
            }
        }
        const std::size_t actorCountBefore =
            world.getActorManager().getActorCount();
        const float healthBefore = world.getPlayerHealth();
        const WaystoneActionResult pulsed =
            world.useWaystone(core, player, true);
        bool recovered = !windupIds.empty();
        for (ActorId id : windupIds) {
            const MobActor *mob = dynamic_cast<const MobActor *>(
                world.getActorManager().findActor(id));
            recovered = recovered && mob != nullptr &&
                mob->getCombatState() == MobCombatState::Recover &&
                mob->getLastCombatTransitionReason() ==
                    MobCombatTransitionReason::ResonanceInterrupted;
        }
        check("P11E/waystone-pulse-interrupts-only-windup-guardians",
              ready && pulsed == WaystoneActionResult::ResonancePulse &&
                  recovered && world.getPlayerHealth() == healthBefore &&
                  world.getActorManager().getActorCount() ==
                      actorCountBefore &&
                  world.getWaystoneEncounterSnapshot()
                          .resonanceCooldownTicks ==
                      WaystoneEncounter::ResonanceCooldownTicks &&
                  world.consumeWaystoneFeedbackKey() ==
                      "waystone.feedback.resonance_pulse");

        const WaystoneActionResult charging =
            world.useWaystone(core, player, true);
        player.position = {40.f, 100.f, 40.f};
        player.box.update(player.position);
        for (int elapsed = 0; elapsed <
             WaystoneEncounter::ResonanceCooldownTicks - 1; ++elapsed) {
            world.tick(tick++);
        }
        const int oneTickRemaining =
            world.getWaystoneEncounterSnapshot()
                .resonanceCooldownTicks;
        world.tick(tick++);
        player.position = glm::vec3(core) + glm::vec3(0.5f);
        player.box.update(player.position);
        const WaystoneActionResult noTarget =
            world.useWaystone(core, player, true);
        check("P11E/resonance-cooldown-is-exact-and-no-target-is-free",
              charging == WaystoneActionResult::ResonanceCharging &&
                  oneTickRemaining == 1 &&
                  noTarget == WaystoneActionResult::ResonanceNoTarget &&
                  world.getWaystoneEncounterSnapshot()
                          .resonanceCooldownTicks == 0 &&
                  world.consumeWaystoneFeedbackKey() ==
                      "waystone.feedback.resonance_no_target");

        bool secondWindup = false;
        for (int elapsed = 0; elapsed < 80 && !secondWindup; ++elapsed) {
            world.tick(tick++);
            for (const ActorSnapshot &snapshot :
                 world.collectActorSnapshots()) {
                if (snapshot.type == WaystoneEncounter::StalkerType &&
                    snapshot.combatState == MobCombatState::Windup) {
                    secondWindup = true;
                    break;
                }
            }
        }
        const WaystoneActionResult secondPulse = secondWindup
            ? world.useWaystone(core, player, true)
            : WaystoneActionResult::ResonanceNoTarget;
        resonanceSaved = world.save();
        const std::string metadata = readTextFile(
            WorldSave(resonanceDirectory).metadataPath());
        check("P11E/resonance-and-combat-pose-stay-out-of-save-v12",
              secondPulse == WaystoneActionResult::ResonancePulse &&
                  resonanceSaved &&
                  metadata.find("\nresonance_cooldown_ticks ") ==
                      std::string::npos &&
                  metadata.find("\ncombat_state ") ==
                      std::string::npos);
    }
    {
        Player player;
        World world(camera, config, player, resonanceDirectory, false, 1);
        bool resetCombatState = true;
        for (const ActorSnapshot &snapshot :
             world.collectActorSnapshots()) {
            if (snapshot.type == WaystoneEncounter::StalkerType) {
                resetCombatState = resetCombatState &&
                    snapshot.combatState == MobCombatState::Idle;
            }
        }
        check("P11E/reopen-resets-transient-resonance-and-combat-pose",
              resonanceSaved && resetCombatState &&
                  world.getWaystoneEncounterSnapshot()
                          .resonanceCooldownTicks == 0);
        ActorId guardianId = InvalidActorId;
        glm::vec3 guardianPosition{0.f};
        for (const ActorSnapshot &snapshot :
             world.collectActorSnapshots()) {
            if (!snapshot.deathPresentation &&
                snapshot.type == WaystoneEncounter::StalkerType) {
                guardianId = snapshot.id;
                guardianPosition = snapshot.position;
                break;
            }
        }
        player.position = guardianPosition + glm::vec3(0.f, 0.f, 2.f);
        player.box.update(player.position);
        const bool killed = guardianId != InvalidActorId &&
            world.attackActor(guardianId, 100.f);
        world.tick(1);
        const WaystoneEncounterSnapshot partial =
            world.getWaystoneEncounterSnapshot();
        const std::vector<ActorSnapshot> postKillSnapshots =
            world.collectActorSnapshots();
        const bool deathStillVisible = std::any_of(
            postKillSnapshots.begin(), postKillSnapshots.end(),
            [guardianId](const ActorSnapshot &snapshot) {
                return snapshot.id == guardianId &&
                       snapshot.deathPresentation;
            });
        check("P11E/death-presentation-never-counts-as-live-guardian",
              killed && deathStillVisible && partial.wave == 1 &&
                  partial.remainingGuardians == 1 &&
                  partial.loadedGuardians == 1);
    }
    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "");
}

// ---------------------------------------------------------------------------
// D3 - deterministic and bounded live-world mob population
// ---------------------------------------------------------------------------
void caseNaturalMobPopulation()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    std::vector<glm::ivec2> candidates;
    bool candidatesInRange = true;
    for (std::size_t attempt = 0; attempt < 8; ++attempt) {
        const glm::ivec2 candidate = World::naturalMobSpawnOffset(
            kValidationSeed, 17, attempt);
        candidatesInRange = candidatesInRange &&
                            std::abs(candidate.x) <= 22 &&
                            std::abs(candidate.y) <= 22 &&
                            (std::abs(candidate.x) >= 8 ||
                             std::abs(candidate.y) >= 8);
        candidates.push_back(candidate);
    }
    check("D3/deterministic-candidates",
          candidates.front() == World::naturalMobSpawnOffset(
                                    kValidationSeed, 17, 0) &&
              candidates.front() != World::naturalMobSpawnOffset(
                                        kValidationSeed + 1, 17, 0));
    check("D3/candidates-respect-spawn-ring", candidatesInRange);

    const auto directory = freshSaveDirectory("natural_mob_population");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    auto prepareSpawnPads = [](World &target, const glm::vec3 &center,
                               int spawnEpoch) {
        for (std::size_t attempt = 0;
             attempt < World::NaturalMobSpawnAttemptsPerCycle; ++attempt) {
            const glm::ivec2 offset = World::naturalMobSpawnOffset(
                target.collectDebugStats().terrainSeed, spawnEpoch, attempt);
            const int x = World::toBlockCoord(center.x) + offset.x;
            const int z = World::toBlockCoord(center.z) + offset.y;
            const VectorXZ chunkPosition = World::getChunkXZ(x, z);
            const VectorXZ localPosition = World::getBlockXZ(x, z);
            Chunk *chunk = target.getChunkManager().findChunk(
                chunkPosition.x, chunkPosition.z);
            if (chunk == nullptr) {
                continue;
            }
            const int groundY =
                chunk->getHeightAt(localPosition.x, localPosition.z);
            target.setBlock(x, groundY, z, BlockId::Stone);
            target.setBlock(x, groundY + 1, z, BlockId::Air);
            target.setBlock(x, groundY + 2, z, BlockId::Air);
        }
    };
    prepareSpawnPads(world, player.position, 1);

    auto naturalSnapshots = [&world]() {
        std::vector<ActorSnapshot> result;
        for (const ActorSnapshot &snapshot : world.collectActorSnapshots()) {
            if (World::isNaturalMobType(snapshot.type)) {
                result.push_back(snapshot);
            }
        }
        return result;
    };

    world.tick(World::NaturalMobSpawnIntervalTicks);
    const std::vector<ActorSnapshot> initial = naturalSnapshots();
    check("D3/live-world-spawns-mobs",
          !initial.empty() &&
              initial.size() <= World::NaturalMobLocalCap,
          "natural mobs=" + std::to_string(initial.size()));

    bool safePlacement = !initial.empty();
    for (const ActorSnapshot &snapshot : initial) {
        const int x = World::toBlockCoord(snapshot.position.x);
        const int y = World::toBlockCoord(snapshot.position.y);
        const int z = World::toBlockCoord(snapshot.position.z);
        const ChunkBlock ground = world.getBlock(x, y - 1, z);
        safePlacement = safePlacement &&
                        static_cast<BlockId>(ground.id) != BlockId::Air &&
                        ground.getData().isCollidable &&
                        static_cast<BlockId>(world.getBlock(x, y, z).id) ==
                            BlockId::Air &&
                        static_cast<BlockId>(
                            world.getBlock(x, y + 1, z).id) == BlockId::Air;
    }
    check("D3/safe-ground-and-headroom", safePlacement);

    world.tick(World::NaturalMobSpawnIntervalTicks * 2);
    const std::vector<ActorSnapshot> localSnapshots = naturalSnapshots();
    const std::size_t localCount = static_cast<std::size_t>(std::count_if(
        localSnapshots.begin(), localSnapshots.end(),
        [&player](const ActorSnapshot &snapshot) {
            const float x = snapshot.position.x - player.position.x;
            const float z = snapshot.position.z - player.position.z;
            return x * x + z * z <=
                World::NaturalMobLocalRadius *
                    World::NaturalMobLocalRadius;
        }));
    check("D3/local-cap-enforced",
          localCount == World::NaturalMobLocalCap);

    while (naturalSnapshots().size() < World::NaturalMobWorldCap) {
        const std::size_t index = naturalSnapshots().size();
        world.spawnMob(World::NaturalMobType,
                       {1000.f + static_cast<float>(index * 4),
                        90.f, 1000.f});
    }
    player.position = {512.f, 90.f, 512.f};
    player.box.update(player.position);
    world.tick(World::NaturalMobSpawnIntervalTicks * 3);
    const std::size_t cappedCount = naturalSnapshots().size();
    check("D3/world-cap-enforced",
          cappedCount == World::NaturalMobWorldCap,
          "natural mobs=" + std::to_string(cappedCount));

    const std::vector<ActorSnapshot> beforeUnload = naturalSnapshots();
    bool unloadRemoved = false;
    bool reloadStayedDespawned = false;
    if (!beforeUnload.empty()) {
        const ActorSnapshot &target = beforeUnload.front();
        const VectorXZ chunk = World::getChunkXZ(
            World::toBlockCoord(target.position.x),
            World::toBlockCoord(target.position.z));
        const std::size_t beforeCount = beforeUnload.size();
        world.getChunkManager().unloadChunk(chunk.x, chunk.z);
        const std::size_t afterUnload = naturalSnapshots().size();
        unloadRemoved = afterUnload < beforeCount;
        world.getChunkManager().loadChunk(chunk.x, chunk.z);
        reloadStayedDespawned = naturalSnapshots().size() == afterUnload;
    }
    check("D3/chunk-unload-despawns-owned-mobs", unloadRemoved);
    check("D3/chunk-reload-does-not-duplicate-mobs", reloadStayedDespawned);

    const WorldDebugStats stats = world.collectDebugStats();
    check("D3/debug-stats-expose-population",
          stats.naturalMobCount == naturalSnapshots().size() &&
              stats.naturalMobWorldCap == World::NaturalMobWorldCap &&
              stats.naturalMobLocalCap == World::NaturalMobLocalCap &&
              stats.naturalMobSpawnAttempts > 0 &&
              stats.naturalMobsSpawned >= initial.size() &&
              stats.naturalMobsDespawned > 0);

    const auto persistenceDirectory =
        freshSaveDirectory("natural_mob_persistence");
    std::size_t savedNaturalCount = 0;
    {
        Player savedPlayer;
        World savedWorld(camera, config, savedPlayer, persistenceDirectory,
                         false, 1);
        prepareSpawnPads(savedWorld, savedPlayer.position, 1);
        int tick = 0;
        while (savedNaturalCount < World::NaturalMobLocalCap && tick < 200) {
            tick += World::NaturalMobSpawnIntervalTicks;
            savedWorld.tick(tick);
            const std::vector<ActorSnapshot> current =
                savedWorld.collectActorSnapshots();
            savedNaturalCount = static_cast<std::size_t>(std::count_if(
                current.begin(), current.end(),
                [](const ActorSnapshot &snapshot) {
                    return World::isNaturalMobType(snapshot.type);
                }));
        }
        check("D3/save-has-live-natural-mobs",
              savedNaturalCount == World::NaturalMobLocalCap,
              "natural mobs=" + std::to_string(savedNaturalCount));
        savedWorld.save();
    }

    WorldSave saveFile(persistenceDirectory);
    WorldSaveData savedData;
    bool duplicateFixtureWritten = saveFile.load(savedData);
    auto naturalState = std::find_if(
        savedData.actors.begin(), savedData.actors.end(),
        [](const ActorSaveState &state) {
            return World::isNaturalMobType(state.type);
        });
    if (duplicateFixtureWritten && naturalState != savedData.actors.end()) {
        ActorSaveState duplicate = *naturalState;
        for (const ActorSaveState &state : savedData.actors) {
            duplicate.id = std::max(duplicate.id, state.id + 1);
        }
        savedData.actors.push_back(duplicate);
        duplicateFixtureWritten = saveFile.save(savedData);
    }
    else {
        duplicateFixtureWritten = false;
    }
    check("D3/duplicate-save-fixture-written", duplicateFixtureWritten);

    {
        Player restoredPlayer;
        World restoredWorld(camera, config, restoredPlayer,
                            persistenceDirectory, false, 1);
        const std::vector<ActorSnapshot> restored =
            restoredWorld.collectActorSnapshots();
        const std::size_t restoredCount = static_cast<std::size_t>(
            std::count_if(restored.begin(), restored.end(),
                          [](const ActorSnapshot &snapshot) {
                              return World::isNaturalMobType(snapshot.type);
                          }));
        check("D3/reload-restores-natural-mobs",
              restoredCount == savedNaturalCount,
              "saved=" + std::to_string(savedNaturalCount) +
                  " restored=" + std::to_string(restoredCount));

        bool duplicatePosition = false;
        for (std::size_t left = 0; left < restored.size(); ++left) {
            if (!World::isNaturalMobType(restored[left].type)) {
                continue;
            }
            for (std::size_t right = left + 1; right < restored.size();
                 ++right) {
                if (World::isNaturalMobType(restored[right].type) &&
                    glm::length(restored[left].position -
                                restored[right].position) < 1.5f) {
                    duplicatePosition = true;
                }
            }
        }
        check("D3/reload-rejects-spatial-duplicates",
              !duplicatePosition && restoredCount == savedNaturalCount);
    }
}

// ---------------------------------------------------------------------------
// D4 - actor targeting, combat, player death and respawn
// ---------------------------------------------------------------------------
void caseCombatAndRespawn()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    Config config = makeConfig();
    Camera camera(config);
    const auto combatDirectory = freshSaveDirectory("combat_targeting");
    Player player;
    World world(camera, config, player, combatDirectory, false, 1);

    const glm::vec3 rayOrigin{8.5f, 100.5f, 8.5f};
    world.setBlock(8, 100, 4, BlockId::Stone);
    const ActorId targetId = world.spawnMob(
        "hellomine:combat_target", {8.5f, 100.5f, 6.f});
    MobActor *target = dynamic_cast<MobActor *>(
        world.getActorManager().findActor(targetId));
    const PlayerTargetSelection actorFirst =
        PlayerTargetSelectionSystem::pick(
            world, rayOrigin, glm::vec3(0.f));
    check("D4/actor-occludes-farther-block",
          actorFirst.actor.has_value() &&
              actorFirst.actor->actorId == targetId &&
              !actorFirst.block.has_value());

    if (target != nullptr) {
        target->position.z = 3.5f;
        target->box.update(target->position);
    }
    const PlayerTargetSelection blockFirst =
        PlayerTargetSelectionSystem::pick(
            world, rayOrigin, glm::vec3(0.f));
    check("D4/block-occludes-farther-actor",
          blockFirst.block.has_value() &&
              blockFirst.block->blockPosition == glm::ivec3(8, 100, 4) &&
              !blockFirst.actor.has_value());

    std::vector<SandboxEventType> combatOrder;
    const auto damageSubscription = world.getEventBus().subscribe(
        SandboxEventType::EntityDamage,
        [&combatOrder](const SandboxEvent &) {
            combatOrder.push_back(SandboxEventType::EntityDamage);
        });
    const auto deathSubscription = world.getEventBus().subscribe(
        SandboxEventType::EntityDeath,
        [&combatOrder](const SandboxEvent &) {
            combatOrder.push_back(SandboxEventType::EntityDeath);
        });
    const auto spawnSubscription = world.getEventBus().subscribe(
        SandboxEventType::EntitySpawn,
        [&combatOrder](const SandboxEvent &) {
            combatOrder.push_back(SandboxEventType::EntitySpawn);
        });

    if (target != nullptr) {
        target->position = {8.5f, 100.5f, 6.f};
        target->box.update(target->position);
    }
    player.position = {8.5f, 100.5f, 8.f};
    player.box.update(player.position);

    const bool firstAttack = world.attackActor(targetId);
    const float healthAfterAttack =
        target != nullptr ? target->getHealth() : -1.f;
    check("D4/player-attack-uses-living-damage",
          firstAttack && std::abs(healthAfterAttack - 6.f) < 0.001f);
    check("D4/repeat-attack-is-suppressed",
          !world.attackActor(targetId) && target != nullptr &&
              std::abs(target->getHealth() - healthAfterAttack) < 0.001f);

    for (int tick = 1; tick <= 11; ++tick) {
        world.tick(tick);
    }
    combatOrder.clear();
    const bool lethalAttack = world.attackActor(targetId, 6.f);
    const bool orderedMobDeath =
        combatOrder == std::vector<SandboxEventType>{
                           SandboxEventType::EntityDamage,
                           SandboxEventType::EntityDeath,
                           SandboxEventType::EntitySpawn};
    check("D4/mob-death-event-order", lethalAttack && orderedMobDeath);

    const std::vector<ActorSaveState> drops =
        world.getActorManager().collectSaveStates();
    const bool lootSpawned = std::any_of(
        drops.begin(), drops.end(), [](const ActorSaveState &state) {
            return state.kind == ActorSaveKind::Item &&
                   state.materialId == static_cast<int>(Material::ID::Dirt) &&
                   state.amount == 1;
        });
    check("D4/lethal-hit-spawns-loot", lootSpawned);
    check("D4/dead-target-rejects-attacks",
          !world.attackActor(targetId));

    player.addItem(Material::IRON_SWORD, 1);
    int swordSlot = -1;
    for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
        if (player.getInventorySlot(slot).getMaterial().id ==
            Material::ID::IronSword) {
            swordSlot = slot;
            break;
        }
    }
    PlayerInputState swordInput;
    swordInput.hotbarSlot = swordSlot;
    player.applyInput(swordInput);
    const ActorId swordTargetId = world.spawnMob(
        "hellomine:iron_sword_target", {10.5f, 100.5f, 8.5f});
    MobActor *swordTarget = dynamic_cast<MobActor *>(
        world.getActorManager().findActor(swordTargetId));
    const bool swordAttack = swordSlot >= 0 &&
                             world.attackActor(swordTargetId);
    check("N2/iron-sword-applies-data-driven-damage-and-durability",
          swordAttack && swordTarget != nullptr &&
              std::abs(swordTarget->getHealth() - 3.f) < 0.001f &&
              player.getInventorySlot(swordSlot).getDurability() == 79);

    world.getEventBus().unsubscribe(damageSubscription);
    world.getEventBus().unsubscribe(deathSubscription);
    world.getEventBus().unsubscribe(spawnSubscription);

    const auto contactDirectory = freshSaveDirectory("combat_contact");
    Player contactPlayer;
    World contactWorld(camera, config, contactPlayer, contactDirectory,
                       false, 1);
    const ActorId contactMobId = contactWorld.spawnMob(
        "hellomine:contact_mob",
        contactPlayer.position + glm::vec3(2.f, 0.f, 0.f));
    MobActor *contactMob = dynamic_cast<MobActor *>(
        contactWorld.getActorManager().findActor(contactMobId));
    int playerDamageEvents = 0;
    ActorId lastPlayerDamageSource = InvalidActorId;
    float lastPlayerDamageHealth = -1.f;
    std::vector<SandboxEventType> playerOrder;
    const auto playerDamageSubscription = contactWorld.getEventBus().subscribe(
        SandboxEventType::EntityDamage,
        [&](const SandboxEvent &event) {
            const auto &damage =
                static_cast<const EntityDamageEvent &>(event);
            if (damage.id == DefaultPlayerActorId) {
                ++playerDamageEvents;
                lastPlayerDamageSource = damage.sourceId;
                lastPlayerDamageHealth = damage.healthAfter;
                playerOrder.push_back(SandboxEventType::EntityDamage);
            }
        });
    const auto playerDeathSubscription = contactWorld.getEventBus().subscribe(
        SandboxEventType::EntityDeath,
        [&](const SandboxEvent &event) {
            const auto &death = static_cast<const EntityDeathEvent &>(event);
            if (death.id == DefaultPlayerActorId) {
                playerOrder.push_back(SandboxEventType::EntityDeath);
            }
        });
    const auto playerSpawnSubscription = contactWorld.getEventBus().subscribe(
        SandboxEventType::PlayerSpawn,
        [&](const SandboxEvent &event) {
            const auto &spawn = static_cast<const PlayerSpawnEvent &>(event);
            if (spawn.playerId == DefaultPlayerActorId) {
                playerOrder.push_back(SandboxEventType::PlayerSpawn);
            }
        });

    int tick = 1;
    while (playerDamageEvents == 0 && tick <= 20) {
        contactWorld.tick(tick++);
    }
    check("D4/mob-contact-damages-player",
          playerDamageEvents == 1 &&
              lastPlayerDamageSource == contactMobId &&
              std::abs(lastPlayerDamageHealth - 18.f) < 0.001f &&
              std::abs(contactWorld.getPlayerHealth() - 18.f) < 0.001f);
    contactWorld.tick(tick++);
    check("D4/contact-damage-is-rate-limited",
          playerDamageEvents == 1 &&
              std::abs(contactWorld.getPlayerHealth() - 18.f) < 0.001f);

    const int secondDamageDeadline = tick + 40;
    while (playerDamageEvents < 2 && tick <= secondDamageDeadline) {
        contactWorld.tick(tick++);
    }
    check("D4/contact-damage-resumes-after-cooldown",
          playerDamageEvents == 2 &&
              std::abs(contactWorld.getPlayerHealth() - 16.f) < 0.001f);

    if (contactMob != nullptr) {
        contactMob->position += glm::vec3(100.f, 0.f, 100.f);
        contactMob->box.update(contactMob->position);
    }
    for (int cooldown = 0; cooldown < 11; ++cooldown) {
        contactWorld.tick(tick++);
    }

    contactPlayer.addItem(Material::STONE_BLOCK, 7);
    contactPlayer.addItem(Material::WOODEN_PICKAXE, 1, 5);
    contactPlayer.position = {20.f, 100.f, 20.f};
    contactPlayer.velocity = {1.f, 2.f, 3.f};
    contactPlayer.box.update(contactPlayer.position);
    contactPlayer.openContainer({8, 100, 8});
    playerOrder.clear();
    const bool lethalPlayerDamage =
        contactWorld.damagePlayer(100.f, contactMobId);
    check("D4/player-death-event-order",
          lethalPlayerDamage &&
              playerOrder == std::vector<SandboxEventType>{
                                 SandboxEventType::EntityDamage,
                                 SandboxEventType::EntityDeath} &&
              contactWorld.getPlayerHealth() == 0.f);
    contactWorld.tick(tick++);
    check("D4/respawn-returns-to-saved-spawn",
          playerOrder == std::vector<SandboxEventType>{
                             SandboxEventType::EntityDamage,
                             SandboxEventType::EntityDeath,
                             SandboxEventType::PlayerSpawn} &&
          glm::length(contactPlayer.position -
                      contactWorld.getPlayerSpawnPoint()) < 0.001f &&
              glm::length(contactPlayer.velocity) < 0.001f &&
              std::abs(contactWorld.getPlayerHealth() -
                       contactWorld.getPlayerMaxHealth()) < 0.001f);

    int retainedStone = 0;
    int retainedToolDurability = 0;
    for (const InventorySlotState &slot :
         contactPlayer.getSaveState().inventory) {
        if (slot.materialId == Material::ID::Stone) {
            retainedStone += slot.amount;
        }
        else if (slot.materialId == Material::ID::WoodenPickaxe) {
            retainedToolDurability = slot.durability;
        }
    }
    check("D4/death-retains-inventory-and-closes-ui",
          std::string(World::PlayerDeathInventoryPolicy) == "retain" &&
              retainedStone == 7 && retainedToolDurability == 5 &&
              !contactPlayer.hasOpenContainer());

    const WorldDebugStats combatStats = contactWorld.collectDebugStats();
    check("D4/health-is-exposed-to-hud",
          combatStats.playerHealth == contactWorld.getPlayerHealth() &&
              combatStats.playerMaxHealth ==
                  contactWorld.getPlayerMaxHealth() &&
              combatStats.playerMaxHealth == 20.f);

    contactWorld.getEventBus().unsubscribe(playerDamageSubscription);
    contactWorld.getEventBus().unsubscribe(playerDeathSubscription);
    contactWorld.getEventBus().unsubscribe(playerSpawnSubscription);
}

// ---------------------------------------------------------------------------
// N4 - readable enemies, bounded loot, weapon reach/cooldown and v7 saves
// ---------------------------------------------------------------------------
void caseCombatDepth()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    Config config = makeConfig();
    Camera camera(config);

    const auto directory = freshSaveDirectory("combat_depth");
    Player player;
    World world(camera, config, player, directory, false, 1);
    player.addItem(Material::WOODEN_SWORD, 1);
    int swordSlot = -1;
    for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
        if (player.getInventorySlot(slot).getMaterial().id ==
            Material::ID::WoodenSword) {
            swordSlot = slot;
            break;
        }
    }
    PlayerInputState swordInput;
    swordInput.hotbarSlot = swordSlot;
    player.applyInput(swordInput);

    const ActorId stalkerId = world.spawnMob(
        World::StalkerMobType, player.position + glm::vec3(10.f, 0.f, 0.f));
    MobActor *stalker = dynamic_cast<MobActor *>(
        world.getActorManager().findActor(stalkerId));
    check("N4/stalker-definition-is-applied-to-live-actor",
          stalker != nullptr && stalker->getMaxHealth() == 8.f &&
              stalker->getWanderSpeed() == 1.6f &&
              stalker->getChaseRadius() == 14.f &&
              stalker->getChaseSpeed() == 3.2f &&
              stalker->getContactDamage() == 1.f &&
              stalker->getLootTable().size() == 3 &&
              stalker->getLootTable()[0].materialId ==
                  Material::ID::PlantFiber &&
              stalker->getLootTable()[2].materialId ==
                  Material::ID::RawMeat);

    const int pristineDurability = swordSlot >= 0
        ? player.getInventorySlot(swordSlot).getDurability()
        : -1;
    const CombatAttackResult paused =
        world.tryAttackActor(stalkerId, false);
    player.openCrafting(CraftingSession::PlayerGridSize);
    const CombatAttackResult uiBusy = world.tryAttackActor(stalkerId);
    player.closeCrafting();
    const CombatAttackResult missing =
        world.tryAttackActor(InvalidActorId);
    const CombatAttackResult outOfReach = world.tryAttackActor(stalkerId);
    check("N4/rejected-attacks-are-atomic",
          paused == CombatAttackResult::SimulationPaused &&
              uiBusy == CombatAttackResult::UiBusy &&
              missing == CombatAttackResult::TargetMissing &&
              outOfReach == CombatAttackResult::OutOfReach &&
              world.getAttackCooldownTicksRemaining() == 0 &&
              swordSlot >= 0 &&
              player.getInventorySlot(swordSlot).getDurability() ==
                  pristineDurability && stalker != nullptr &&
              stalker->getHealth() == 8.f);

    if (stalker != nullptr) {
        stalker->position = player.position + glm::vec3(2.f, 0.f, 0.f);
        stalker->box.update(stalker->position);
    }
    const CombatAttackResult firstHit = world.tryAttackActor(stalkerId);
    check("N4/wooden-sword-hit-uses-data-and-durability-on-success",
          firstHit == CombatAttackResult::Hit && stalker != nullptr &&
              stalker->getHealth() == 4.f &&
              world.getAttackCooldownTicksRemaining() == 10 &&
              player.getInventorySlot(swordSlot).getDurability() == 31);

    const CombatAttackResult immediate = world.tryAttackActor(stalkerId);
    for (int tick = 0; tick < 9; ++tick) {
        world.tick(2000 + tick);
    }
    const CombatAttackResult early = world.tryAttackActor(stalkerId);
    check("N4/attack-cooldown-is-exact-fixed-tick-state",
          immediate == CombatAttackResult::CoolingDown &&
              early == CombatAttackResult::CoolingDown &&
              world.getAttackCooldownTicksRemaining() == 1 &&
              player.getInventorySlot(swordSlot).getDurability() == 31 &&
              stalker != nullptr && stalker->getHealth() == 4.f);

    world.tick(2009);
    if (stalker != nullptr) {
        stalker->setDamageInvulnerabilityRemaining(1.f);
    }
    const CombatAttackResult targetRejected =
        world.tryAttackActor(stalkerId);
    check("N4/target-rejection-does-not-consume-cooldown-or-durability",
          targetRejected == CombatAttackResult::TargetRejected &&
              world.getAttackCooldownTicksRemaining() == 0 &&
              player.getInventorySlot(swordSlot).getDurability() == 31 &&
              stalker != nullptr && stalker->getHealth() == 4.f);
    if (stalker != nullptr) {
        stalker->setDamageInvulnerabilityRemaining(0.f);
    }
    const CombatAttackResult lethal = world.tryAttackActor(stalkerId);
    const std::vector<ActorSaveState> stalkerDrops =
        world.getActorManager().collectSaveStates();
    int wheatAmount = 0;
    int rawMeatAmount = 0;
    int plantFiberAmount = 0;
    int stalkerItemCount = 0;
    for (const ActorSaveState &state : stalkerDrops) {
        if (state.kind != ActorSaveKind::Item) {
            continue;
        }
        ++stalkerItemCount;
        if (state.materialId == static_cast<int>(Material::ID::Wheat)) {
            wheatAmount += state.amount;
        }
        else if (state.materialId ==
                 static_cast<int>(Material::ID::RawMeat)) {
            rawMeatAmount += state.amount;
        }
        else if (state.materialId ==
                 static_cast<int>(Material::ID::PlantFiber)) {
            plantFiberAmount += state.amount;
        }
    }
    check("N4/stalker-death-drops-bounded-recovery-loot",
          lethal == CombatAttackResult::Hit && stalker != nullptr &&
              !stalker->isAlive() && wheatAmount >= 1 && wheatAmount <= 2 &&
              rawMeatAmount == 1 && plantFiberAmount == 1 &&
              stalkerItemCount == 3 &&
              player.getInventorySlot(swordSlot).getDurability() == 30);
    if (stalker != nullptr) {
        stalker->dropLoot(world);
    }
    check("N4/death-loot-is-emitted-exactly-once",
          world.getActorManager().collectSaveStates().size() ==
              stalkerDrops.size());

    const std::size_t beforeBruteDrops =
        world.getActorManager().collectSaveStates().size();
    const ActorId bruteId = world.spawnMob(
        World::BruteMobType, player.position + glm::vec3(2.f, 0.f, 0.f));
    MobActor *brute = dynamic_cast<MobActor *>(
        world.getActorManager().findActor(bruteId));
    check("N4/brute-definition-is-slower-heavier-and-stronger",
          brute != nullptr && brute->getMaxHealth() == 16.f &&
              brute->getWanderSpeed() == 0.8f &&
              brute->getChaseRadius() == 10.f &&
              brute->getChaseSpeed() == 1.6f &&
              brute->getContactDamage() == 4.f &&
              brute->getLootTable().size() == 4 &&
              brute->getLootTable()[0].materialId ==
                  Material::ID::PlantFiber &&
              brute->getLootTable()[3].materialId ==
                  Material::ID::RawMeat);
    const bool bruteKilled = world.attackActor(bruteId, 100.f);
    const std::vector<ActorSaveState> allDrops =
        world.getActorManager().collectSaveStates();
    int coalAmount = 0;
    int allRawMeatAmount = 0;
    for (const ActorSaveState &state : allDrops) {
        if (state.kind == ActorSaveKind::Item &&
            state.materialId == static_cast<int>(Material::ID::CoalOre)) {
            coalAmount += state.amount;
        }
        if (state.kind == ActorSaveKind::Item &&
            state.materialId == static_cast<int>(Material::ID::RawMeat)) {
            allRawMeatAmount += state.amount;
        }
    }
    check("N4/brute-death-drops-bounded-progression-loot",
          bruteKilled && brute != nullptr && !brute->isAlive() &&
              coalAmount == 1 &&
              allRawMeatAmount - rawMeatAmount >= 1 &&
              allRawMeatAmount - rawMeatAmount <= 2 &&
              allDrops.size() == beforeBruteDrops + 4);
    const WorldDebugStats enemyStats = world.collectDebugStats();
    check("N8B/all-combat-archetypes-are-natural-population-types",
          World::isNaturalMobType(World::StalkerMobType) &&
              World::isNaturalMobType(World::BruteMobType) &&
              World::isNaturalMobType(World::SpitterMobType) &&
              enemyStats.naturalMobWorldCap == World::NaturalMobWorldCap);

    const auto contactDamage = [&](const std::string &name,
                                   const std::string &type) {
        const auto contactDirectory = freshSaveDirectory(name);
        Player contactPlayer;
        World contactWorld(camera, config, contactPlayer,
                           contactDirectory, false, 1);
        contactWorld.spawnMob(type, contactPlayer.position);
        for (int tick = 1;
             tick <= 40 && contactWorld.getPlayerHealth() == 20.f;
             ++tick) {
            contactWorld.tick(tick);
        }
        return contactWorld.getPlayerHealth();
    };
    check("N4/contact-damage-distinguishes-stalker-and-brute",
          contactDamage("combat_stalker_contact", World::StalkerMobType) ==
                  19.f &&
              contactDamage("combat_brute_contact", World::BruteMobType) ==
                  16.f);

    const auto persistenceDirectory =
        freshSaveDirectory("combat_cooldown_persistence");
    bool persistencePrepared = false;
    {
        Player savedPlayer;
        World savedWorld(camera, config, savedPlayer, persistenceDirectory,
                         false, 1);
        savedPlayer.addItem(Material::WOODEN_SWORD, 1);
        PlayerInputState select;
        select.hotbarSlot = 0;
        savedPlayer.applyInput(select);
        const ActorId targetId = savedWorld.spawnMob(
            World::StalkerMobType,
            savedPlayer.position + glm::vec3(2.f, 0.f, 0.f));
        persistencePrepared =
            savedWorld.tryAttackActor(targetId) == CombatAttackResult::Hit &&
            savedWorld.getAttackCooldownTicksRemaining() == 10 &&
            savedWorld.save();
    }
    {
        Player restoredPlayer;
        World restoredWorld(camera, config, restoredPlayer,
                            persistenceDirectory, false, 1);
        check("N4/attack-cooldown-and-weapon-durability-resume-after-reload",
              persistencePrepared &&
                  restoredWorld.getAttackCooldownTicksRemaining() == 10 &&
                  restoredPlayer.getInventorySlot(0).getMaterial().id ==
                      Material::ID::WoodenSword &&
                  restoredPlayer.getInventorySlot(0).getDurability() == 31);
    }
    WorldSave combatSave(persistenceDirectory);
    WorldSaveData validCombatSave;
    const bool validCombatLoaded = combatSave.load(validCombatSave);
    WorldSaveData invalidCombatSave = validCombatSave;
    invalidCombatSave.playerState.attackCooldownTicks = 1201;
    WorldSaveData preservedCombatSave;
    check("N4/invalid-attack-cooldown-preserves-last-good-save",
          validCombatLoaded && !combatSave.save(invalidCombatSave) &&
              combatSave.load(preservedCombatSave) &&
              preservedCombatSave.playerState.attackCooldownTicks == 10);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("combat_v6_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n4-v6-combat";
    std::filesystem::create_directories(migratedWorld);
    std::filesystem::copy_file(
        ResourcePaths::join(
            ResourcePaths::projectRoot(),
            "tools/fixtures/combat/world-v6-recovery.meta"),
        migratedWorld / "world.meta",
        std::filesystem::copy_options::overwrite_existing);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("n4-v6-combat");
    WorldSaveData migratedData;
    const bool migratedLoaded = migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    const bool preservedUnknownObjective = std::find(
        migratedData.objectiveState.completedIds.begin(),
        migratedData.objectiveState.completedIds.end(),
        "future.optional") !=
        migratedData.objectiveState.completedIds.end();
    check("N4/version-six-migrates-with-recovery-and-objectives-intact",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.playerState.health == 13.f &&
              migratedData.playerState.foodCooldownTicks == 7 &&
              migratedData.playerState.attackCooldownTicks == 0 &&
              migratedData.playerState.inventory.size() == 1 &&
              migratedData.playerState.inventory.front().materialId ==
                  Material::ID::Bread &&
              migratedData.objectiveState.progress.size() == 1 &&
              preservedUnknownObjective);
}

// ---------------------------------------------------------------------------
// N8A - readable melee encounters, directional feedback and bounded work
// ---------------------------------------------------------------------------
void caseCombatReadability()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);

    const auto prepareArena = [](World &world) {
        for (int x = 5; x <= 11; ++x) {
            for (int z = 4; z <= 13; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
                world.setBlock(x, 102, z, BlockId::Air);
            }
        }
    };

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_fsm_telegraph"), false, 1);
        prepareArena(world);
        EventRecorder events(world.getEventBus());
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));

        world.tick(1);
        const ActorSnapshot windup = mob != nullptr
            ? mob->getSnapshot() : ActorSnapshot{};
        check("N8A/melee-enters-explicit-windup-with-one-telegraph",
              mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Windup &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::AttackRangeReached &&
                  windup.combatant &&
                  windup.combatTargetId == DefaultPlayerActorId &&
                  windup.combatStateTicksRemaining == 6 &&
                  events.count(SandboxEventType::CombatWindup) == 1 &&
                  world.getPlayerHealth() == 20.f);

        for (int tick = 2; tick <= 6; ++tick) {
            world.tick(tick);
        }
        check("N8A/windup-never-damages-or-republishes-warning-early",
              world.getPlayerHealth() == 20.f &&
                  events.count(SandboxEventType::CombatWindup) == 1 &&
                  events.count(SandboxEventType::EntityDamage) == 0 &&
                  mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Windup &&
                  mob->getCombatStateTicksRemaining() == 1);

        world.tick(7);
        const WorldDebugStats hitStats = world.collectDebugStats();
        check("N8A/windup-resolves-once-into-hit-recover-and-knockback",
              mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Recover &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::AttackHit &&
                  world.getPlayerHealth() == 19.f &&
                  events.count(SandboxEventType::EntityDamage) == 1 &&
                  player.velocity.z > 0.f && player.velocity.y > 0.f &&
                  hitStats.combatFeedback.kind ==
                      PlayerCombatFeedbackKind::Damage &&
                  hitStats.combatFeedback.direction == CombatDirection::Front &&
                  hitStats.combatFeedback.sourceId == mobId &&
                  hitStats.combatFeedback.epoch == 1);

        for (int tick = 8; tick <= 14; ++tick) {
            world.tick(tick);
        }
        check("N8A/recover-is-fixed-tick-and-cooldown-prevents-duplicate-hit",
              mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Chase &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::RecoveryComplete &&
                  mob->getCombatCooldownTicksRemaining() > 0 &&
                  world.getPlayerHealth() == 19.f &&
                  events.count(SandboxEventType::EntityDamage) == 1);
    }

    const auto damageDirection = [&](const std::string &name,
                                     const glm::vec3 &offset) {
        Player player;
        World world(camera, config, player, freshSaveDirectory(name), false, 1);
        prepareArena(world);
        const ActorId sourceId = world.spawnMob(
            World::StalkerMobType, player.position + offset);
        const bool damaged = world.damagePlayer(1.f, sourceId);
        const WorldDebugStats stats = world.collectDebugStats();
        return damaged ? stats.combatFeedback.direction
                       : CombatDirection::None;
    };
    check("N8A/damage-feedback-classifies-four-view-relative-directions",
          damageDirection("n8a_direction_front", {0.f, 0.f, -1.f}) ==
                  CombatDirection::Front &&
              damageDirection("n8a_direction_right", {1.f, 0.f, 0.f}) ==
                  CombatDirection::Right &&
              damageDirection("n8a_direction_back", {0.f, 0.f, 1.f}) ==
                  CombatDirection::Back &&
              damageDirection("n8a_direction_left", {-1.f, 0.f, 0.f}) ==
                  CombatDirection::Left);

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_guard"), false, 1);
        prepareArena(world);
        player.addItem(Material::WOODEN_SWORD, 1);
        PlayerInputState input;
        input.hotbarSlot = 0;
        player.applyInput(input);
        EventRecorder events(world.getEventBus());
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        world.setPlayerGuarding(true);
        for (int tick = 1; tick <= 7; ++tick) {
            world.tick(tick);
        }
        const WorldDebugStats guardStats = world.collectDebugStats();
        check("N8A/front-weapon-guard-is-atomic-readable-and-damages-tool-once",
              mob != nullptr && world.getPlayerHealth() == 20.f &&
                  player.getInventorySlot(0).getDurability() == 31 &&
                  events.count(SandboxEventType::CombatGuard) == 1 &&
                  events.count(SandboxEventType::EntityDamage) == 0 &&
                  mob->getCombatState() == MobCombatState::Recover &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::AttackGuarded &&
                  guardStats.combatFeedback.kind ==
                      PlayerCombatFeedbackKind::Guard &&
                  guardStats.combatFeedback.direction ==
                      CombatDirection::Front &&
                  guardStats.combatFeedback.guardRecoverTicksRemaining ==
                      World::PlayerGuardRecoverTicks);

        const MobMeleeAttackResult duringRecovery = mob != nullptr
            ? world.resolveMobMeleeAttack(*mob, DefaultPlayerActorId)
            : MobMeleeAttackResult::TargetMissing;
        check("N8A/guard-recovery-prevents-permanent-hold-blocking",
              duringRecovery == MobMeleeAttackResult::Hit &&
                  world.getPlayerHealth() == 19.f &&
                  player.getInventorySlot(0).getDurability() == 31 &&
                  events.count(SandboxEventType::CombatGuard) == 1);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_guard_back"), false, 1);
        prepareArena(world);
        player.addItem(Material::WOODEN_SWORD, 1);
        PlayerInputState input;
        input.hotbarSlot = 0;
        player.applyInput(input);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, 1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        world.setPlayerGuarding(true);
        const MobMeleeAttackResult result = mob != nullptr
            ? world.resolveMobMeleeAttack(*mob, DefaultPlayerActorId)
            : MobMeleeAttackResult::TargetMissing;
        check("N8A/rear-attacks-bypass-front-only-guard",
              result == MobMeleeAttackResult::Hit &&
                  world.getPlayerHealth() == 19.f &&
                  player.getInventorySlot(0).getDurability() == 32 &&
                  world.collectDebugStats().combatFeedback.direction ==
                      CombatDirection::Back);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_occlusion"), false, 1);
        prepareArena(world);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        world.setBlock(8, 100, 7, BlockId::Stone);
        world.setBlock(8, 101, 7, BlockId::Stone);
        const MobMeleeAttackResult result = mob != nullptr
            ? world.resolveMobMeleeAttack(*mob, DefaultPlayerActorId)
            : MobMeleeAttackResult::TargetMissing;
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8A/melee-line-of-sight-rejects-occluded-contact",
              result == MobMeleeAttackResult::Occluded &&
                  world.getPlayerHealth() == 20.f &&
                  stats.combat.raycastsUsed == 1 &&
                  stats.combat.raycastBudgetDenied == 0);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_target_escape"), false, 1);
        prepareArena(world);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        world.tick(1);
        player.position += glm::vec3(40.f, 0.f, 0.f);
        player.box.update(player.position);
        for (int tick = 2; tick <= 7; ++tick) {
            world.tick(tick);
        }
        check("N8A/committed-windup-resolves-target-escape-without-damage",
              mob != nullptr && world.getPlayerHealth() == 20.f &&
                  mob->getCombatState() == MobCombatState::Recover &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::TargetEscaped);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_hit_interrupt"), false, 1);
        prepareArena(world);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        world.tick(1);
        const glm::vec3 before = mob != nullptr ? mob->position : glm::vec3(0.f);
        const bool accepted = world.attackActor(mobId, 1.f);
        const ActorSnapshot snapshot = mob != nullptr
            ? mob->getSnapshot() : ActorSnapshot{};
        check("N8A/player-hit-interrupts-windup-with-stagger-and-feedback",
              accepted && mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Recover &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::HitInterrupted &&
                  mob->getCombatStateTicksRemaining() ==
                      World::MobPlayerHitRecoverTicks &&
                  glm::length(mob->position - before) > 0.5f &&
                  snapshot.hitFeedback > 0.f &&
                  world.getPlayerHealth() == 20.f);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_chase_budget"), false, 1);
        prepareArena(world);
        for (int index = 0; index < 34; ++index) {
            world.spawnMob(World::StalkerMobType,
                           player.position + glm::vec3(0.f, 0.f, 5.f));
        }
        world.tick(1);
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8A/chase-work-is-capped-and-reported-per-fixed-tick",
              stats.combat.combatantCount == 34 &&
                  stats.combat.chaseCount == 34 &&
                  stats.combat.chaseStepBudget ==
                      World::CombatChaseStepBudgetPerTick &&
                  stats.combat.chaseStepsUsed ==
                      World::CombatChaseStepBudgetPerTick &&
                  stats.combat.chaseStepBudgetDenied == 2);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_raycast_budget"), false, 1);
        prepareArena(world);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        MobMeleeAttackResult finalResult = MobMeleeAttackResult::TargetMissing;
        if (mob != nullptr) {
            for (std::size_t attempt = 0;
                 attempt <= World::CombatRaycastBudgetPerTick; ++attempt) {
                finalResult = world.resolveMobMeleeAttack(
                    *mob, DefaultPlayerActorId);
            }
        }
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8A/melee-rays-are-capped-and-budget-exhaustion-is-explicit",
              finalResult == MobMeleeAttackResult::RayBudgetExhausted &&
                  stats.combat.raycastBudget ==
                      World::CombatRaycastBudgetPerTick &&
                  stats.combat.raycastsUsed ==
                      World::CombatRaycastBudgetPerTick &&
                  stats.combat.raycastBudgetDenied == 1);
    }

    const auto persistenceDirectory =
        freshSaveDirectory("n8a_transient_fsm_reload");
    ActorId persistedMobId = InvalidActorId;
    bool prepared = false;
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 1);
        prepareArena(world);
        persistedMobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, -1.1f));
        world.tick(1);
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(persistedMobId));
        prepared = mob != nullptr &&
            mob->getCombatState() == MobCombatState::Windup && world.save();
    }
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 1);
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(persistedMobId));
        const bool reset = mob != nullptr &&
            mob->getCombatState() == MobCombatState::Idle &&
            mob->getLastCombatTransitionReason() ==
                MobCombatTransitionReason::Spawned &&
            mob->getCombatStateTicksRemaining() == 0 &&
            mob->getCombatCooldownTicksRemaining() == 0;
        world.tick(2);
        check("N8A/reload-clears-transient-fsm-and-never-resolves-stale-hit",
              prepared && reset && mob != nullptr &&
                  mob->getCombatState() == MobCombatState::Windup &&
                  mob->getCombatStateTicksRemaining() == 6 &&
                  world.getPlayerHealth() == 20.f);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8a_missing_target"), false, 1);
        prepareArena(world);
        const ActorId mobId = world.spawnMob(
            World::StalkerMobType, player.position + glm::vec3(0.f, 0.f, 5.f));
        MobActor *mob = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(mobId));
        if (mob != nullptr) {
            mob->setChaseTarget(nullptr);
        }
        world.tick(1);
        check("N8A/missing-target-fails-safe-to-idle",
              mob != nullptr &&
                  mob->getCombatTargetId() == InvalidActorId &&
                  mob->getCombatState() == MobCombatState::Idle &&
                  mob->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::TargetMissing &&
                  world.getPlayerHealth() == 20.f);
    }
}

// ---------------------------------------------------------------------------
// N8B - ranged combat profiles and bounded transient projectiles
// ---------------------------------------------------------------------------
void caseRangedCombatProjectiles()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);

    const auto prepareArena = [](World &world) {
        for (int x = 5; x <= 11; ++x) {
            for (int z = -4; z <= 14; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
                world.setBlock(x, 102, z, BlockId::Air);
            }
        }
    };
    const auto spawnSpitter = [](World &world, Player &player) {
        return world.spawnMob(
            World::SpitterMobType,
            player.position + glm::vec3(0.f, 0.f, -8.f));
    };

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_ranged_fsm_hit"), false, 1);
        prepareArena(world);
        EventRecorder events(world.getEventBus());
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        world.tick(1);
        const ActorSnapshot windup = spitter != nullptr
            ? spitter->getSnapshot() : ActorSnapshot{};
        check("N8B/ranged-enemy-telegraphs-explicit-windup-once",
              spitter != nullptr && windup.combatant &&
                  windup.combatMode == EnemyCombatMode::Ranged &&
                  windup.combatState == MobCombatState::Windup &&
                  windup.combatStateTicksRemaining == 12 &&
                  events.count(SandboxEventType::CombatWindup) == 1 &&
                  world.getPlayerHealth() == 20.f &&
                  world.collectCombatProjectileSnapshots().empty());

        for (int tick = 0; tick < 11; ++tick) {
            world.tick(1);
        }
        check("N8B/ranged-windup-never-launches-or-damages-early",
              spitter != nullptr &&
                  spitter->getCombatState() == MobCombatState::Windup &&
                  spitter->getCombatStateTicksRemaining() == 1 &&
                  events.count(SandboxEventType::CombatWindup) == 1 &&
                  world.getPlayerHealth() == 20.f &&
                  world.collectCombatProjectileSnapshots().empty());

        world.tick(1);
        const std::vector<CombatProjectileSnapshot> launched =
            world.collectCombatProjectileSnapshots();
        const WorldDebugStats launchStats = world.collectDebugStats();
        check("N8B/windup-launches-one-owned-transient-projectile",
              spitter != nullptr && launched.size() == 1 &&
                  launched.front().id != InvalidCombatProjectileId &&
                  launched.front().ownerId == spitterId &&
                  launched.front().ticksRemaining == 49 &&
                  launched.front().radius == 0.15f &&
                  spitter->getCombatState() == MobCombatState::Recover &&
                  spitter->getLastCombatTransitionReason() ==
                      MobCombatTransitionReason::ProjectileLaunched &&
                  launchStats.combat.projectilesLaunched == 1 &&
                  launchStats.combat.projectileCount == 1);
        if (spitter != nullptr) {
            spitter->setChaseTarget(nullptr);
        }
        for (int tick = 0;
             tick < 40 &&
                 !world.collectCombatProjectileSnapshots().empty();
             ++tick) {
            world.tick(1);
        }
        const WorldDebugStats hitStats = world.collectDebugStats();
        check("N8B/projectile-hits-once-with-damage-knockback-and-direction",
              world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == 18.f &&
                  player.velocity.z > 0.f && player.velocity.y > 0.f &&
                  events.count(SandboxEventType::EntityDamage) == 1 &&
                  hitStats.combat.projectileHits == 1 &&
                  hitStats.combatFeedback.kind ==
                      PlayerCombatFeedbackKind::Damage &&
                  hitStats.combatFeedback.direction ==
                      CombatDirection::Front);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_wall"), false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        const MobRangedAttackResult launched = spitter != nullptr
            ? world.launchMobProjectile(*spitter, DefaultPlayerActorId)
            : MobRangedAttackResult::TargetMissing;
        if (spitter != nullptr) {
            spitter->setChaseTarget(nullptr);
        }
        world.setBlock(8, 100, 4, BlockId::Stone);
        world.setBlock(8, 101, 4, BlockId::Stone);
        for (int tick = 0;
             tick < 20 &&
                 !world.collectCombatProjectileSnapshots().empty();
             ++tick) {
            world.tick(1);
        }
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8B/projectile-collision-stops-at-solid-wall",
              launched == MobRangedAttackResult::Launched &&
                  world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == 20.f &&
                  stats.combat.projectileBlocks == 1 &&
                  stats.combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::Blocked);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_guard"), false, 1);
        prepareArena(world);
        player.addItem(Material::WOODEN_SWORD, 1);
        PlayerInputState input;
        input.hotbarSlot = 0;
        player.applyInput(input);
        EventRecorder events(world.getEventBus());
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        const MobRangedAttackResult launched = spitter != nullptr
            ? world.launchMobProjectile(*spitter, DefaultPlayerActorId)
            : MobRangedAttackResult::TargetMissing;
        if (spitter != nullptr) {
            spitter->setChaseTarget(nullptr);
        }
        world.setPlayerGuarding(true);
        for (int tick = 0;
             tick < 40 &&
                 !world.collectCombatProjectileSnapshots().empty();
             ++tick) {
            world.tick(1);
        }
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8B/front-guard-consumes-projectile-and-durability-once",
              launched == MobRangedAttackResult::Launched &&
                  world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == 20.f &&
                  player.getInventorySlot(0).getDurability() == 31 &&
                  events.count(SandboxEventType::CombatGuard) == 1 &&
                  events.count(SandboxEventType::EntityDamage) == 0 &&
                  stats.combat.projectileGuards == 1 &&
                  stats.combatFeedback.kind ==
                      PlayerCombatFeedbackKind::Guard);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_capacity"), false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        bool launchedEight = spitter != nullptr;
        for (int index = 0; index < 8 && spitter != nullptr; ++index) {
            launchedEight = launchedEight &&
                world.launchMobProjectile(*spitter,
                    DefaultPlayerActorId) ==
                    MobRangedAttackResult::Launched;
        }
        const MobRangedAttackResult ninth = spitter != nullptr
            ? world.launchMobProjectile(*spitter, DefaultPlayerActorId)
            : MobRangedAttackResult::TargetMissing;
        const std::vector<CombatProjectileSnapshot> snapshots =
            world.collectCombatProjectileSnapshots();
        std::set<CombatProjectileId> ids;
        for (const CombatProjectileSnapshot &snapshot : snapshots) {
            ids.insert(snapshot.id);
        }
        world.tick(1);
        const WorldDebugStats budgetStats = world.collectDebugStats();
        check("N8B/local-capacity-and-step-budget-are-hard-bounded",
              launchedEight &&
                  ninth == MobRangedAttackResult::CapacityReached &&
                  snapshots.size() == 8 && ids.size() == snapshots.size() &&
                  budgetStats.combat.projectileCount == 8 &&
                  budgetStats.combat.projectileCapacityDenied == 1 &&
                  budgetStats.combat.projectileStepsUsed == 8 &&
                  budgetStats.combat.projectileStepBudget ==
                      World::CombatProjectileStepBudgetPerTick &&
                  budgetStats.combat.projectileStepBudgetDenied == 0 &&
                  budgetStats.combat.projectileWorldLimit ==
                      World::CombatProjectileWorldLimit);
        const bool killed = world.attackActor(spitterId, 100.f);
        const WorldDebugStats clearStats = world.collectDebugStats();
        check("N8B/owner-death-clears-all-owned-projectiles-immediately",
              killed && world.collectCombatProjectileSnapshots().empty() &&
                  clearStats.combat.projectileOwnerClears == 8 &&
                  clearStats.combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::OwnerMissing);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_launch_rejections"), false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        world.setBlock(8, 100, 4, BlockId::Stone);
        world.setBlock(8, 101, 4, BlockId::Stone);
        const MobRangedAttackResult occluded = spitter != nullptr
            ? world.launchMobProjectile(*spitter, DefaultPlayerActorId)
            : MobRangedAttackResult::TargetMissing;
        world.setBlock(8, 100, 4, BlockId::Air);
        world.setBlock(8, 101, 4, BlockId::Air);
        player.position += glm::vec3(40.f, 0.f, 0.f);
        player.box.update(player.position);
        const MobRangedAttackResult escaped = spitter != nullptr
            ? world.launchMobProjectile(*spitter, DefaultPlayerActorId)
            : MobRangedAttackResult::TargetMissing;
        const MobRangedAttackResult missing = spitter != nullptr
            ? world.launchMobProjectile(*spitter, InvalidActorId)
            : MobRangedAttackResult::TargetMissing;
        check("N8B/ranged-launch-rejects-occlusion-escape-and-missing-target",
              occluded == MobRangedAttackResult::Occluded &&
                  escaped == MobRangedAttackResult::OutOfRange &&
                  missing == MobRangedAttackResult::TargetMissing &&
                  world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == 20.f);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_lifecycle"), false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        const bool launched = spitter != nullptr &&
            world.launchMobProjectile(*spitter, DefaultPlayerActorId) ==
                MobRangedAttackResult::Launched;
        if (spitter != nullptr) {
            spitter->setChaseTarget(nullptr);
        }
        player.position += glm::vec3(10.f, 0.f, 0.f);
        player.box.update(player.position);
        for (int tick = 0;
             tick < 60 &&
                 !world.collectCombatProjectileSnapshots().empty();
             ++tick) {
            world.tick(1);
        }
        const WorldDebugStats distanceStats = world.collectDebugStats();
        check("N8B/projectile-max-distance-expires-without-ghost-hit",
              launched && world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == 20.f &&
                  distanceStats.combat.projectileExpirations == 1 &&
                  distanceStats.combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::MaximumDistance);

        player.position -= glm::vec3(10.f, 0.f, 0.f);
        player.box.update(player.position);
        const bool relaunched = spitter != nullptr &&
            world.launchMobProjectile(*spitter, DefaultPlayerActorId) ==
                MobRangedAttackResult::Launched;
        player.position += glm::vec3(40.f, 0.f, 0.f);
        player.box.update(player.position);
        world.tick(1);
        const WorldDebugStats activeStats = world.collectDebugStats();
        check("N8B/leaving-active-area-clears-projectile-on-next-tick",
              relaunched && world.collectCombatProjectileSnapshots().empty() &&
                  activeStats.combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::OutsideActiveArea);

        player.position -= glm::vec3(40.f, 0.f, 0.f);
        player.box.update(player.position);
        const EnemyDefinition *baseSpitter =
            runtimeEnemyRegistry().find(World::SpitterMobType);
        bool lifetimeLaunched = false;
        if (spitter != nullptr && baseSpitter != nullptr) {
            EnemyDefinition shortLived = *baseSpitter;
            shortLived.combat.projectileLifetimeTicks = 2;
            shortLived.combat.projectileMaxDistance = 64.f;
            shortLived.combat.projectileActiveRadius = 64.f;
            spitter->applyDefinition(shortLived);
            lifetimeLaunched = world.launchMobProjectile(
                *spitter, DefaultPlayerActorId) ==
                MobRangedAttackResult::Launched;
            spitter->setChaseTarget(nullptr);
        }
        player.position += glm::vec3(10.f, 0.f, 0.f);
        player.box.update(player.position);
        world.tick(1);
        world.tick(1);
        check("N8B/projectile-lifetime-is-an-independent-hard-bound",
              lifetimeLaunched &&
                  world.collectCombatProjectileSnapshots().empty() &&
                  world.collectDebugStats()
                          .combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::LifetimeExpired);
    }

    const auto persistenceDirectory =
        freshSaveDirectory("n8b_transient_projectile_reload");
    bool persisted = false;
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        persisted = spitter != nullptr &&
            world.launchMobProjectile(*spitter, DefaultPlayerActorId) ==
                MobRangedAttackResult::Launched &&
            world.collectCombatProjectileSnapshots().size() == 1 &&
            world.save();
    }
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 1);
        check("N8B/projectiles-are-never-serialized-or-restored",
              persisted && world.collectCombatProjectileSnapshots().empty() &&
                  world.getActorManager().countActorsByType(
                      World::SpitterMobType) == 1);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_chunk_unload"),
                    false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        const bool launched = spitter != nullptr &&
            world.launchMobProjectile(*spitter, DefaultPlayerActorId) ==
                MobRangedAttackResult::Launched;
        world.getChunkManager().unloadChunk(0, 0);
        const WorldDebugStats stats = world.collectDebugStats();
        check("N8B/chunk-unload-clears-projectile-and-natural-owner",
              launched && world.collectCombatProjectileSnapshots().empty() &&
                  world.getActorManager().findActor(spitterId) == nullptr &&
                  stats.combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::ChunkUnloaded);
    }

    {
        Player player;
        World world(camera, config, player,
                    freshSaveDirectory("n8b_projectile_player_death"),
                    false, 1);
        prepareArena(world);
        const ActorId spitterId = spawnSpitter(world, player);
        MobActor *spitter = dynamic_cast<MobActor *>(
            world.getActorManager().findActor(spitterId));
        bool launched = spitter != nullptr;
        for (int index = 0; index < 2 && spitter != nullptr; ++index) {
            launched = launched &&
                world.launchMobProjectile(*spitter,
                    DefaultPlayerActorId) ==
                    MobRangedAttackResult::Launched;
        }
        const bool killed = world.damagePlayer(100.f, spitterId);
        world.tick(1);
        check("N8B/player-death-respawn-clears-every-live-projectile",
              launched && killed &&
                  world.collectCombatProjectileSnapshots().empty() &&
                  world.getPlayerHealth() == world.getPlayerMaxHealth() &&
                  world.collectDebugStats()
                          .combat.lastProjectileRemovalReason ==
                      CombatProjectileRemovalReason::PlayerUnavailable);
    }
}

// ---------------------------------------------------------------------------
// D5 - seed acquisition, crop growth, harvest, replant and persistence
// ---------------------------------------------------------------------------
void caseWheatCropLoop()
{
    const auto &cropDefinition =
        BlockDatabase::get().getDefinition(BlockId::WheatCrop);
    const ChunkBlock planted(
        BlockId::WheatCrop, BlockMetadata::WheatCrop::Planted);
    const ChunkBlock growing(
        BlockId::WheatCrop, BlockMetadata::WheatCrop::Growing);
    const ChunkBlock ripening(
        BlockId::WheatCrop, BlockMetadata::WheatCrop::Ripening);
    const ChunkBlock mature(
        BlockId::WheatCrop, BlockMetadata::WheatCrop::Mature);
    check("D5/crop-definition-and-materials-registered",
          cropDefinition.stringId == "hellomine:wheatcrop" &&
              cropDefinition.render.meshType == BlockMeshType::Resource &&
              cropDefinition.render.shaderType == BlockShaderType::Flora &&
              cropDefinition.render.shape.name == "Cross" &&
              cropDefinition.render.texTopCoord == glm::ivec2(11, 0) &&
              Material::toMaterial(BlockId::WheatCrop).id ==
                  Material::ID::WheatSeeds &&
              Material::WHEAT_SEEDS.toBlockID() == BlockId::WheatCrop &&
              !Material::WHEAT.isBlock);
    check("D5/metadata-controls-visible-growth",
          std::abs(cropDefinition.behavior->verticalRenderScale(
                       cropDefinition, planted) - 0.25f) < 0.001f &&
              std::abs(cropDefinition.behavior->verticalRenderScale(
                           cropDefinition, mature) - 1.f) < 0.001f);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    const auto directory = freshSaveDirectory("wheat_crop_loop");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    const auto countPlayer = [&player](Material::ID materialId) {
        int total = 0;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            const ItemStack &stack = player.getInventorySlot(slot);
            if (stack.getMaterial().id == materialId) {
                total += stack.getNumInStack();
            }
        }
        return total;
    };
    const auto selectMaterial = [&player](Material::ID materialId) {
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            if (player.getInventorySlot(slot).getMaterial().id ==
                materialId) {
                PlayerInputState input;
                input.hotbarSlot = slot;
                player.applyInput(input);
                return true;
            }
        }
        return false;
    };

    constexpr int cropY = 101;
    const glm::ivec3 grassPosition{8, cropY, 8};
    world.setBlock(grassPosition.x, grassPosition.y, grassPosition.z,
                   ChunkBlock(BlockId::TallGrass,
                              BlockMetadata::TallGrass::Mature));
    const bool gatheredSeed = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(grassPosition) + glm::vec3(0.5f));
    check("D5/mature-grass-provides-seed",
          gatheredSeed && countPlayer(Material::ID::TallGrass) == 1 &&
              countPlayer(Material::ID::WheatSeeds) == 1);

    const glm::ivec3 invalidPosition{10, cropY, 10};
    world.setBlock(invalidPosition.x, invalidPosition.y - 1,
                   invalidPosition.z, BlockId::Stone);
    world.setBlock(invalidPosition.x, invalidPosition.y,
                   invalidPosition.z, BlockId::Air);
    const bool selectedInitialSeed =
        selectMaterial(Material::ID::WheatSeeds);
    const bool invalidPlant = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(invalidPosition) + glm::vec3(0.5f));
    check("D5/planting-rejects-invalid-support",
          selectedInitialSeed && !invalidPlant &&
              countPlayer(Material::ID::WheatSeeds) == 1 &&
              world.getBlock(invalidPosition.x, invalidPosition.y,
                             invalidPosition.z) == BlockId::Air);

    const glm::ivec3 cropPosition{11, cropY, 11};
    world.setBlock(cropPosition.x, cropPosition.y - 1, cropPosition.z,
                   BlockId::Dirt);
    world.setBlock(cropPosition.x, cropPosition.y, cropPosition.z,
                   BlockId::Air);
    const bool validPlant = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(cropPosition) + glm::vec3(0.5f));
    check("D5/seed-plants-on-dirt",
          validPlant && countPlayer(Material::ID::WheatSeeds) == 0 &&
              world.getBlock(cropPosition.x, cropPosition.y,
                             cropPosition.z) == planted);

    world.tick(200);
    const WorldDebugStats firstGrowth = world.collectDebugStats();
    check("D5/random-tick-advances-one-stage",
          world.getBlock(cropPosition.x, cropPosition.y,
                         cropPosition.z) == growing &&
              firstGrowth.randomTicksDispatched == 1 &&
              firstGrowth.randomTickBlocks == 1);

    const bool brokeImmature = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(cropPosition) + glm::vec3(0.5f));
    check("D5/immature-crop-returns-seed-only",
          brokeImmature && countPlayer(Material::ID::WheatSeeds) == 1 &&
              countPlayer(Material::ID::Wheat) == 0);

    const bool selectedReturnedSeed =
        selectMaterial(Material::ID::WheatSeeds);
    const bool replantedForGrowth = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(cropPosition) + glm::vec3(0.5f));
    world.tick(201);
    world.tick(202);
    world.tick(203);
    check("D5/bounded-random-ticks-reach-maturity",
          selectedReturnedSeed && replantedForGrowth &&
              world.getBlock(cropPosition.x, cropPosition.y,
                             cropPosition.z) == mature &&
              world.collectDebugStats().randomTickBlocks == 0);

    const bool harvested = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(cropPosition) + glm::vec3(0.5f));
    check("D5/mature-harvest-yields-wheat-and-seed",
          harvested && countPlayer(Material::ID::Wheat) == 1 &&
              countPlayer(Material::ID::WheatSeeds) == 1);
    const bool selectedHarvestSeed =
        selectMaterial(Material::ID::WheatSeeds);
    const bool replantedAfterHarvest = BlockInteractionSystem::placeBlock(
        world, player, glm::vec3(cropPosition) + glm::vec3(0.5f));
    check("D5/harvest-seed-replants-crop",
          selectedHarvestSeed && replantedAfterHarvest &&
              countPlayer(Material::ID::WheatSeeds) == 0 &&
              countPlayer(Material::ID::Wheat) == 1 &&
              world.getBlock(cropPosition.x, cropPosition.y,
                             cropPosition.z) == planted);

    world.setBlock(cropPosition.x, cropPosition.y, cropPosition.z,
                   BlockId::Air);
    constexpr int remoteChunkX = 4;
    const glm::ivec3 unloadedPosition{
        remoteChunkX * CHUNK_SIZE + 1, cropY, 1};
    world.getChunkManager().loadChunk(remoteChunkX, 0);
    world.setBlock(unloadedPosition.x, unloadedPosition.y - 1,
                   unloadedPosition.z, BlockId::Grass);
    world.setBlock(unloadedPosition.x, unloadedPosition.y,
                   unloadedPosition.z, growing);
    world.getChunkManager().unloadChunk(remoteChunkX, 0);
    world.tick(300);
    world.getChunkManager().loadChunk(remoteChunkX, 0);
    check("D5/unloaded-crop-does-not-tick",
          world.getBlock(unloadedPosition.x, unloadedPosition.y,
                         unloadedPosition.z) == growing);

    const auto persistenceDirectory =
        freshSaveDirectory("wheat_crop_persistence");
    const glm::ivec3 persistedPosition{12, cropY, 12};
    {
        Player savingPlayer;
        World savingWorld(camera, config, savingPlayer,
                          persistenceDirectory, false, 1);
        savingWorld.setBlock(persistedPosition.x,
                             persistedPosition.y - 1,
                             persistedPosition.z, BlockId::Dirt);
        savingWorld.setBlock(persistedPosition.x, persistedPosition.y,
                             persistedPosition.z, ripening);
        check("D5/crop-stage-save-succeeds", savingWorld.save());
    }
    {
        Player loadedPlayer;
        World loadedWorld(camera, config, loadedPlayer,
                          persistenceDirectory, false, 1);
        check("D5/save-reload-preserves-crop-stage",
              loadedWorld.getBlock(persistedPosition.x,
                                   persistedPosition.y,
                                   persistedPosition.z) == ripening);
    }
}

// ---------------------------------------------------------------------------
// D6 - one continuous playable loop across farming, storage and combat
// ---------------------------------------------------------------------------
void casePlayableVerticalSlice()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("playable_vertical_slice");
    Config config = makeConfig();
    Camera camera(config);

    const glm::ivec3 grassPosition{8, 101, 8};
    const glm::ivec3 cropPosition{11, 101, 11};
    const glm::ivec3 chestPosition{9, 101, 8};

    auto countPlayer = [](const Player &player, Material::ID materialId) {
        int total = 0;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            const ItemStack &stack = player.getInventorySlot(slot);
            if (stack.getMaterial().id == materialId) {
                total += stack.getNumInStack();
            }
        }
        return total;
    };
    auto findPlayerSlot = [](const Player &player, Material::ID materialId) {
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            if (player.getInventorySlot(slot).getMaterial().id ==
                materialId) {
                return slot;
            }
        }
        return -1;
    };
    auto selectPlayerSlot = [](Player &player, int slot) {
        if (slot < 0) {
            return false;
        }
        PlayerInputState input;
        input.hotbarSlot = slot;
        player.applyInput(input);
        return true;
    };
    auto prepareSpawnPads = [](World &world, const glm::vec3 &center,
                               int spawnEpoch) {
        for (std::size_t attempt = 0;
             attempt < World::NaturalMobSpawnAttemptsPerCycle; ++attempt) {
            const glm::ivec2 offset = World::naturalMobSpawnOffset(
                world.collectDebugStats().terrainSeed, spawnEpoch, attempt);
            const int x = World::toBlockCoord(center.x) + offset.x;
            const int z = World::toBlockCoord(center.z) + offset.y;
            const VectorXZ chunkPosition = World::getChunkXZ(x, z);
            Chunk *chunk = world.getChunkManager().findChunk(
                chunkPosition.x, chunkPosition.z);
            if (chunk == nullptr) {
                continue;
            }
            // Lift each candidate to the player's test plane. This is part of
            // the initial fixture and lets the normal chase/pickup systems
            // complete the encounter without teleporting the player later.
            const int groundY = World::toBlockCoord(center.y) - 1;
            world.setBlock(x, groundY, z, BlockId::Stone);
            world.setBlock(x, groundY + 1, z, BlockId::Air);
            world.setBlock(x, groundY + 2, z, BlockId::Air);
        }
    };

    ActorId defeatedMobId = InvalidActorId;
    Material::ID defeatedLootMaterial = Material::ID::Nothing;
    BlockMetadata_t cropStageBeforeSave = 0;
    int mobLootAfterPickup = 0;
    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        EventRecorder events(world.getEventBus());

        // The only direct world edits in this scenario are deterministic
        // initial fixtures. Every state transition below uses gameplay APIs.
        world.setBlock(grassPosition.x, grassPosition.y, grassPosition.z,
                       ChunkBlock(BlockId::TallGrass,
                                  BlockMetadata::TallGrass::Mature));
        world.setBlock(cropPosition.x, cropPosition.y - 1,
                       cropPosition.z, BlockId::Dirt);
        world.setBlock(cropPosition.x, cropPosition.y,
                       cropPosition.z, BlockId::Air);
        world.setBlock(chestPosition.x, chestPosition.y,
                       chestPosition.z, BlockId::Chest);
        const bool chestInitialized =
            ChestContainer::initialize(world, chestPosition);
        for (int x = 4; x <= 12; ++x) {
            for (int z = 4; z <= 12; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
            }
        }
        prepareSpawnPads(world, player.position, 1);

        const bool gatheredSeed = BlockInteractionSystem::breakBlock(
            world, player, glm::vec3(grassPosition) + glm::vec3(0.5f));
        const int seedSlot =
            findPlayerSlot(player, Material::ID::WheatSeeds);
        const bool planted =
            selectPlayerSlot(player, seedSlot) &&
            BlockInteractionSystem::placeBlock(
                world, player,
                glm::vec3(cropPosition) + glm::vec3(0.5f));
        check("D6/gather-and-plant-through-gameplay",
              chestInitialized && gatheredSeed && planted &&
                  countPlayer(player, Material::ID::WheatSeeds) == 0 &&
                  world.getBlock(cropPosition.x, cropPosition.y,
                                 cropPosition.z) ==
                      ChunkBlock(BlockId::WheatCrop,
                                 BlockMetadata::WheatCrop::Planted));

        world.tick(1);
        world.tick(2);
        world.tick(3);
        const bool harvested = BlockInteractionSystem::breakBlock(
            world, player,
            glm::vec3(cropPosition) + glm::vec3(0.5f));
        check("D6/grow-and-harvest-through-random-ticks",
              harvested &&
                  countPlayer(player, Material::ID::Wheat) == 1 &&
                  countPlayer(player, Material::ID::WheatSeeds) == 1);

        const bool opened = BlockInteractionSystem::useBlock(
            world, player,
            glm::vec3(chestPosition) + glm::vec3(0.5f));
        const int wheatSlot = findPlayerSlot(player, Material::ID::Wheat);
        const bool stored = opened && ChestContainer::transferFromPlayer(
                                           world, player, wheatSlot, 1);
        const auto storedView = ChestContainer::view(world, player);
        check("D6/store-harvest-through-container",
              stored && storedView.has_value() &&
                  storedView->inventory.count(Material::ID::Wheat) == 1 &&
                  countPlayer(player, Material::ID::Wheat) == 0);
        ChestContainer::close(player);

        world.tick(World::NaturalMobSpawnIntervalTicks);
        const std::vector<ActorSnapshot> encountered =
            world.collectActorSnapshots();
        const auto naturalMob = std::min_element(
            encountered.begin(), encountered.end(),
            [&player](const ActorSnapshot &left,
                      const ActorSnapshot &right) {
                const auto distanceSquared = [&player](
                                                 const ActorSnapshot &value) {
                    if (!World::isNaturalMobType(value.type)) {
                        return std::numeric_limits<float>::max();
                    }
                    const float x = value.position.x - player.position.x;
                    const float z = value.position.z - player.position.z;
                    return x * x + z * z;
                };
                return distanceSquared(left) < distanceSquared(right);
            });
        const bool foundNaturalMob =
            naturalMob != encountered.end() &&
            World::isNaturalMobType(naturalMob->type);
        if (foundNaturalMob) {
            defeatedMobId = naturalMob->id;
            const auto *mob = dynamic_cast<const MobActor *>(
                world.getActorManager().findActor(defeatedMobId));
            if (mob != nullptr && !mob->getLootTable().empty()) {
                defeatedLootMaterial =
                    mob->getLootTable().front().materialId;
            }
            player.position = naturalMob->position +
                glm::vec3(0.f, 0.f, 2.f);
            player.box.update(player.position);
            const int mobX = World::toBlockCoord(naturalMob->position.x);
            const int mobZ = World::toBlockCoord(naturalMob->position.z);
            const int playerZ = World::toBlockCoord(player.position.z);
            const int groundY =
                World::toBlockCoord(naturalMob->position.y) - 1;
            for (int x = mobX - 1; x <= mobX + 1; ++x) {
                for (int z = std::min(mobZ, playerZ) - 1;
                     z <= std::max(mobZ, playerZ) + 1; ++z) {
                    world.setBlock(x, groundY, z, BlockId::Stone);
                    world.setBlock(x, groundY + 1, z, BlockId::Air);
                    world.setBlock(x, groundY + 2, z, BlockId::Air);
                }
            }
        }
        const bool firstHit = foundNaturalMob &&
                              world.attackActor(defeatedMobId);
        int combatTick = World::NaturalMobSpawnIntervalTicks;
        bool mobReachedPlayer = false;
        while (firstHit && combatTick < 200 && !mobReachedPlayer) {
            world.tick(++combatTick);
            const Actor *target =
                world.getActorManager().findActor(defeatedMobId);
            const auto *combatMob = dynamic_cast<const MobActor *>(target);
            if (combatMob != nullptr) {
                const glm::vec3 separation = glm::max(
                    glm::abs(combatMob->position - player.position) -
                        combatMob->box.dimensions - player.box.dimensions,
                    glm::vec3(0.f));
                mobReachedPlayer = glm::length(separation) <=
                    combatMob->getCombatProfile().attackRange + 0.001f;
            }
        }
        const bool lethalHit =
            firstHit && mobReachedPlayer &&
            world.attackActor(defeatedMobId, 100.f);
        const auto *remainingMob = dynamic_cast<const MobActor *>(
            world.getActorManager().findActor(defeatedMobId));
        const std::vector<ActorSaveState> postCombat =
            world.getActorManager().collectSaveStates();
        const auto loot = std::find_if(
            postCombat.begin(), postCombat.end(),
            [defeatedLootMaterial](const ActorSaveState &state) {
                return state.kind == ActorSaveKind::Item &&
                       state.materialId ==
                           static_cast<int>(defeatedLootMaterial) &&
                       state.amount > 0;
            });
        check("D6/natural-encounter-produces-combat-loot",
              foundNaturalMob && firstHit && lethalHit &&
                  loot != postCombat.end(),
              "found=" + std::to_string(foundNaturalMob) +
                  " first=" + std::to_string(firstHit) +
                  " reached=" + std::to_string(mobReachedPlayer) +
                  " lethal=" + std::to_string(lethalHit) +
                  " state=" +
                  (remainingMob != nullptr
                       ? std::string(mobCombatStateName(
                             remainingMob->getCombatState())) + "/" +
                             mobCombatTransitionReasonName(
                                 remainingMob->getLastCombatTransitionReason())
                       : "gone"));

        const int expectedLootAmount =
            loot != postCombat.end() ? loot->amount : 0;
        const int mobLootBeforePickup =
            countPlayer(player, defeatedLootMaterial);
        for (int pickupTick = 0; pickupTick < 11; ++pickupTick) {
            world.tick(++combatTick);
        }
        mobLootAfterPickup =
            countPlayer(player, defeatedLootMaterial);
        const std::vector<ActorSaveState> postPickup =
            world.getActorManager().collectSaveStates();
        const bool lootStillExists = std::any_of(
            postPickup.begin(), postPickup.end(),
            [defeatedLootMaterial](const ActorSaveState &state) {
                return state.kind == ActorSaveKind::Item &&
                       state.materialId ==
                           static_cast<int>(defeatedLootMaterial);
            });
        check("D6/pick-up-defeated-mob-loot",
              expectedLootAmount > 0 &&
                  mobLootAfterPickup ==
                      mobLootBeforePickup + expectedLootAmount &&
                  !lootStillExists &&
                  events.count(SandboxEventType::ItemPickup) >= 2);

        const int harvestSeedSlot =
            findPlayerSlot(player, Material::ID::WheatSeeds);
        const bool replanted =
            selectPlayerSlot(player, harvestSeedSlot) &&
            BlockInteractionSystem::placeBlock(
                world, player,
                glm::vec3(cropPosition) + glm::vec3(0.5f));
        const ChunkBlock cropBeforeSave = world.getBlock(
            cropPosition.x, cropPosition.y, cropPosition.z);
        cropStageBeforeSave = cropBeforeSave.metadata;
        check("D6/replant-after-combat-without-state-injection",
              replanted &&
                  static_cast<BlockId>(cropBeforeSave.id) ==
                      BlockId::WheatCrop &&
                  cropStageBeforeSave ==
                      BlockMetadata::WheatCrop::Planted);
        check("D6/gameplay-events-cover-complete-loop",
              events.count(SandboxEventType::BlockBreak) == 2 &&
                  events.count(SandboxEventType::BlockPlace) == 2 &&
                  events.count(SandboxEventType::BlockUse) == 1 &&
                  events.count(SandboxEventType::EntityDamage) >= 2 &&
                  events.count(SandboxEventType::EntityDeath) >= 1);
        check("D6/save-complete-loop", world.save());
    }

    {
        Player restoredPlayer;
        World restoredWorld(camera, config, restoredPlayer, directory,
                            false, 1);
        const ChunkBlock restoredCrop = restoredWorld.getBlock(
            cropPosition.x, cropPosition.y, cropPosition.z);
        const bool opened = ChestContainer::open(
            restoredWorld, restoredPlayer, chestPosition);
        const auto restoredChest =
            ChestContainer::view(restoredWorld, restoredPlayer);
        const std::vector<ActorSaveState> restoredActors =
            restoredWorld.getActorManager().collectSaveStates();
        const bool defeatedMobRestored = std::any_of(
            restoredActors.begin(), restoredActors.end(),
            [defeatedMobId](const ActorSaveState &state) {
                return state.id == defeatedMobId;
            });
        check("D6/relaunch-preserves-loop-state",
              static_cast<BlockId>(restoredCrop.id) ==
                      BlockId::WheatCrop &&
                  restoredCrop.metadata == cropStageBeforeSave && opened &&
                  restoredChest.has_value() &&
                  restoredChest->inventory.count(Material::ID::Wheat) == 1 &&
                  countPlayer(restoredPlayer, defeatedLootMaterial) ==
                      mobLootAfterPickup &&
                  !defeatedMobRestored);
        ChestContainer::close(restoredPlayer);
    }
}

std::string validObjectiveTestDefinitions()
{
    return R"(# HelloMine3D objective registry v1
version 3
objective n1.break_dirt
type break_block
target hellomine:dirt
required 2
prerequisite none
visible 1
optional 0
title "Break Dirt"
instruction "Break two Dirt blocks."
feedback "Dirt broken"
end
objective n1.craft_workbench
type craft_item
target hellomine:workbench
required 1
prerequisite n1.break_dirt
visible 1
optional 0
title "Craft Workbench"
instruction "Craft one Workbench."
feedback "Workbench crafted"
end
objective n1.place_workbench
type place_block
target hellomine:workbench
required 1
prerequisite n1.craft_workbench
visible 1
optional 0
title "Place Workbench"
instruction "Place one Workbench."
feedback "Workbench placed"
end
objective n1.defeat_enemy
type defeat_enemy
required 1
prerequisite n1.place_workbench
visible 1
optional 0
title "Defeat Enemy"
instruction "Defeat one enemy."
feedback "Enemy defeated"
end
objective n1.pickup_dirt
type pickup_item
target hellomine:dirt
required 1
prerequisite n1.defeat_enemy
visible 1
optional 0
title "Pickup Dirt"
instruction "Pickup one Dirt item."
feedback "Dirt picked up"
end
objective n1.obtain_stone
type obtain_item
target hellomine:stone
required 1
prerequisite n1.pickup_dirt
visible 1
optional 0
title "Obtain Stone"
instruction "Hold one Stone."
feedback "Stone obtained"
end
objective n1.reach_marker
type reach_location
required 1
location 10 20 30 2
prerequisite n1.obtain_stone
visible 1
optional 0
title "Reach Marker"
instruction "Reach the marker."
feedback "Marker reached"
end
objective n1.reopen_world
type reopen_world
required 1
prerequisite n1.reach_marker
visible 1
optional 0
title "Reopen World"
instruction "Reopen the world."
feedback "Session complete"
end
)";
}

void caseDataDrivenObjectives()
{
    std::ifstream baseInput(ResourcePaths::media(
                                "objectives/Base.objective"),
                            std::ios::binary);
    const std::string baseText(
        (std::istreambuf_iterator<char>(baseInput)),
        std::istreambuf_iterator<char>());
    ObjectiveRegistry baseRegistry;
    bool baseLoaded = false;
    try {
        baseRegistry.freeze({{"Base.objective", baseText}});
        baseLoaded = true;
    }
    catch (const std::exception &) {
    }
    const ObjectiveDefinition *baseReach =
        baseLoaded
            ? baseRegistry.find("alpha.reach_spawn_marker")
            : nullptr;
    check("N1/base-objective-registry-is-versioned-and-complete",
          baseLoaded && baseRegistry.definitionVersion() == 3 &&
              baseRegistry.definitions().size() == 34 &&
              baseRegistry.find("alpha.gather_wood") != nullptr &&
              baseRegistry.find("alpha.reopen_world") != nullptr &&
              baseRegistry.find("progression.smelt_iron") != nullptr &&
              baseRegistry.find("progression.smelt_iron")->type ==
                  ObjectiveType::SmeltItem &&
              baseRegistry.find("survival.eat_bread") != nullptr &&
              baseRegistry.find("survival.eat_bread")->type ==
                  ObjectiveType::ConsumeItem &&
              baseRegistry.find("combat.defeat_enemies") != nullptr &&
              baseRegistry.find("combat.defeat_enemies")->type ==
                  ObjectiveType::DefeatEnemy &&
              baseRegistry.find("combat.collect_wheat") != nullptr &&
              baseRegistry.find("combat.collect_coal") != nullptr &&
              baseRegistry.find("exploration.recover_waystone") != nullptr &&
              baseRegistry.find("exploration.recover_waystone")->type ==
                  ObjectiveType::BreakBlock &&
              baseRegistry.find("exploration.restore_waystone") != nullptr &&
              baseRegistry.find("exploration.restore_waystone")->type ==
                  ObjectiveType::PlaceBlock &&
              baseReach != nullptr &&
              baseReach->type == ObjectiveType::ReachLocation &&
              !baseReach->visible && baseReach->optional);

    const std::string valid = validObjectiveTestDefinitions();
    const auto rejects = [](std::vector<ObjectiveSource> sources) {
        try {
            ObjectiveRegistry registry;
            registry.freeze(std::move(sources));
        }
        catch (const std::exception &) {
            return true;
        }
        return false;
    };
    std::string unknownType = valid;
    unknownType.replace(unknownType.find("type break_block"),
                        std::string("type break_block").size(),
                        "type unknown_type");
    std::string missingPrerequisite = valid;
    missingPrerequisite.replace(
        missingPrerequisite.find("prerequisite none"),
        std::string("prerequisite none").size(),
        "prerequisite n1.future");
    std::string hiddenRequired = valid;
    hiddenRequired.replace(hiddenRequired.find("visible 1"),
                           std::string("visible 1").size(),
                           "visible 0");
    std::string repeatedReach = valid;
    const std::size_t reachBegin =
        repeatedReach.find("objective n1.reach_marker");
    repeatedReach.replace(
        repeatedReach.find("required 1", reachBegin),
        std::string("required 1").size(), "required 2");
    check("N1/objective-registry-rejects-invalid-contracts",
          rejects({{"unknown.objective", unknownType}}) &&
              rejects({{"missing.objective", missingPrerequisite}}) &&
              rejects({{"hidden.objective", hiddenRequired}}) &&
              rejects({{"repeated-reach.objective", repeatedReach}}) &&
              rejects({{"first.objective", valid},
                       {"duplicate.objective", valid}}));

    ObjectiveRegistry registry;
    registry.freeze({{"test.objective", valid}});
    ObjectiveSaveState initial;
    initial.completedIds.push_back("future.optional");
    initial.progress.push_back({"future.progress", 7});
    ObjectiveSaveState beforeReopen;
    {
        Player player;
        SandboxEventBus eventBus;
        ObjectiveSystem objectives(registry, player, eventBus, initial,
                                   0u, false);
        const ObjectiveSnapshot first = objectives.snapshot();
        const ObjectiveSnapshot repeated = objectives.snapshot();
        check("N1/objective-query-is-read-only-and-starts-first-goal",
              first.currentId == "n1.break_dirt" &&
                  first.totalObjectives == 8 &&
                  first.completedObjectives == 0 && first.progress == 0 &&
                  repeated.currentId == first.currentId &&
                  repeated.progress == first.progress);

        eventBus.publish(BlockBreakEvent({0, 0, 0}, BlockId::Stone));
        eventBus.publish(BlockBreakEvent({0, 0, 0}, BlockId::Dirt));
        const ObjectiveSaveState partial = objectives.saveState();
        const auto partialProgress = std::find_if(
            partial.progress.begin(), partial.progress.end(),
            [](const ObjectiveProgressState &state) {
                return state.id == "n1.break_dirt" && state.value == 1;
            });
        check("N1/event-progress-is-filtered-and-persisted",
              objectives.progress("n1.break_dirt") == 1 &&
                  partialProgress != partial.progress.end());

        eventBus.publish(BlockBreakEvent({0, 0, 0}, BlockId::Dirt));
        eventBus.publish(CraftCompletedEvent(
            "test", Material::ID::Workbench, 1, 1, {}));
        eventBus.publish(BlockPlaceEvent({0, 0, 0}, BlockId::Workbench));
        eventBus.publish(EntityDeathEvent(2, DefaultPlayerActorId, {}));
        eventBus.publish(ItemPickupEvent(
            DefaultPlayerActorId, 3, Material::ID::Dirt, 1, {}));
        const bool stoneAdded =
            player.addItem(Material::STONE_BLOCK, 1) == 1;
        eventBus.publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, Material::ID::Stone, 1,
            "objective_test"));
        player.position = {10.f, 20.f, 30.f};
        objectives.update(0.05f);
        const ObjectiveSnapshot before = objectives.snapshot();
        beforeReopen = objectives.saveState();
        const bool unknownCompletedPreserved = std::find(
            beforeReopen.completedIds.begin(),
            beforeReopen.completedIds.end(),
            "future.optional") != beforeReopen.completedIds.end();
        const bool unknownProgressPreserved = std::any_of(
            beforeReopen.progress.begin(), beforeReopen.progress.end(),
            [](const ObjectiveProgressState &state) {
                return state.id == "future.progress" && state.value == 7;
            });
        check("N1/all-objective-event-types-advance-in-order",
              stoneAdded && before.currentId == "n1.reopen_world" &&
                  before.completedObjectives == 7 &&
                  before.completedTitles.size() == 7 &&
                  before.completedTitles.front() == "Break Dirt" &&
                  before.completedTitles.back() == "Reach Marker" &&
                  unknownCompletedPreserved && unknownProgressPreserved);
    }
    {
        Player player;
        SandboxEventBus eventBus;
        ObjectiveSystem objectives(registry, player, eventBus,
                                   beforeReopen, 0u, true);
        const ObjectiveSnapshot complete = objectives.snapshot();
        check("N1/reopen-completes-session-without-item-reward",
              complete.sessionComplete &&
                  complete.completedObjectives == 8 &&
                  complete.completedTitles.size() == 8 &&
                  complete.completedTitles.back() == "Reopen World" &&
                  player.getInventorySlot(0).getMaterial().id ==
                      Material::ID::Nothing);
    }

    const auto saveDirectory = freshSaveDirectory("objective_v5_contract");
    WorldSaveData validSave;
    validSave.worldId = "objective-v5-contract";
    validSave.worldName = "Objective V5 Contract";
    validSave.seed = kValidationSeed;
    validSave.createdUtc = LegacyWorldTimestampUtc;
    validSave.lastPlayedUtc = LegacyWorldTimestampUtc;
    validSave.lastBuildIdentity = "validation";
    validSave.alphaJourneyFlags = 1u;
    validSave.objectiveState.completedIds = {"alpha.gather_wood",
                                             "future.optional"};
    validSave.objectiveState.progress = {{"alpha.craft_workbench", 1}};
    WorldSave save(saveDirectory);
    WorldSaveData loaded;
    const bool saved = save.save(validSave) && save.load(loaded);
    WorldSaveData invalidDefinition = validSave;
    invalidDefinition.objectiveState.definitionVersion =
        ObjectiveSaveState::CurrentDefinitionVersion + 1;
    WorldSaveData duplicate = validSave;
    duplicate.objectiveState.completedIds.push_back("future.optional");
    WorldSaveData completedProgress = validSave;
    completedProgress.objectiveState.progress.push_back(
        {"alpha.gather_wood", 1});
    WorldSaveData mismatchedFlags = validSave;
    mismatchedFlags.alphaJourneyFlags = 0u;
    WorldSaveData preserved;
    check("N1/current-version-preserves-objective-state",
          saved && loaded.version == WorldSaveFormatVersion &&
              loaded.objectiveState.definitionVersion ==
                  ObjectiveSaveState::CurrentDefinitionVersion &&
              loaded.objectiveState.completedIds ==
                  validSave.objectiveState.completedIds &&
              loaded.objectiveState.progress.size() == 1);
    check("N1/invalid-objective-state-preserves-last-good-save",
          !save.save(invalidDefinition) && !save.save(duplicate) &&
              !save.save(completedProgress) &&
              !save.save(mismatchedFlags) && save.load(preserved) &&
              preserved.objectiveState.completedIds ==
                  validSave.objectiveState.completedIds);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("objective_v4_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "objective-v4-partial";
    std::filesystem::create_directories(migratedWorld);
    std::filesystem::copy_file(
        ResourcePaths::join(
            ResourcePaths::projectRoot(),
            "tools/fixtures/objectives/world-v4-partial-alpha.meta"),
        migratedWorld / "world.meta",
        std::filesystem::copy_options::overwrite_existing);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("objective-v4-partial");
    WorldSaveData migratedData;
    const bool migratedLoaded =
        migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    check("N1/version-four-flags-migrate-to-current-objectives",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.alphaJourneyFlags == 3u &&
              migratedData.objectiveState.definitionVersion ==
                  ObjectiveSaveState::CurrentDefinitionVersion &&
              migratedData.objectiveState.completedIds ==
                  ObjectiveState::completedFromLegacyFlags(3u) &&
              migratedData.objectiveState.progress.empty());
}

// ---------------------------------------------------------------------------
// G6 - clean-start Alpha journey through progression, combat and relaunch
// ---------------------------------------------------------------------------
void casePlayableAlphaJourney()
{
    setEnv("HELLOMINE3D_SEED", "");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 100 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto catalogueRoot =
        freshSaveDirectory("playable_alpha_catalogue");
    const WorldManagementService management(catalogueRoot);
    const WorldManagementResult created =
        management.createWorld("Playable Alpha", kValidationSeed);
    const WorldManagementResult opened =
        created.succeeded()
            ? management.prepareWorldForOpen(created.worldId)
            : WorldManagementResult{};
    check("G6/main-menu-service-creates-and-opens-alpha-world",
          created.succeeded() && opened.succeeded() &&
              opened.worldId == created.worldId &&
              std::filesystem::weakly_canonical(opened.directoryPath) ==
                  std::filesystem::weakly_canonical(created.directoryPath));
    if (!opened.succeeded()) {
        return;
    }

    RecipeRegistry recipes;
    std::ifstream recipeInput(ResourcePaths::media("recipes/Base.recipe"),
                              std::ios::binary);
    std::ostringstream recipeContent;
    recipeContent << recipeInput.rdbuf();
    recipes.freeze({{"base.recipe", recipeContent.str()}});

    Config config = makeConfig();
    Camera camera(config);
    const glm::ivec3 workbenchPosition{9, 100, 8};
    const glm::ivec3 ironPosition{8, 100, 10};
    const std::array<glm::ivec3, 3> stonePositions{{
        {5, 100, 10}, {6, 100, 10}, {7, 100, 10}}};
    std::vector<glm::ivec3> oakPositions;
    for (int x = 4; x <= 14; ++x) {
        oakPositions.push_back({x, 100, 6});
    }

    auto countPlayer = [](const Player &player, Material::ID materialId) {
        int total = 0;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            const ItemStack &stack = player.getInventorySlot(slot);
            if (stack.getMaterial().id == materialId) {
                total += stack.getNumInStack();
            }
        }
        return total;
    };
    auto findPlayerSlot = [](const Player &player,
                             Material::ID materialId) {
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            if (player.getInventorySlot(slot).getMaterial().id ==
                materialId) {
                return slot;
            }
        }
        return -1;
    };
    auto selectPlayerSlot = [](Player &player, int slot) {
        if (slot < 0) {
            return false;
        }
        PlayerInputState input;
        input.hotbarSlot = slot;
        player.applyInput(input);
        return true;
    };
    auto craft = [&recipes](
                     Player &player, int gridSize,
                     const std::vector<std::pair<int, Material::ID>> &cells,
                     Material::ID expectedOutput) {
        CraftingSession session(gridSize);
        bool cellsSet = true;
        for (const auto &cell : cells) {
            cellsSet = cellsSet &&
                       session.setCell(cell.first, cell.second);
        }
        const CraftingPreview preview =
            player.previewCrafting(session, recipes);
        const CraftingCommitResult result = player.commitCrafting(
            session, recipes, preview, 1);
        return cellsSet && preview.outputMaterialId == expectedOutput &&
               result.succeeded() && result.outputAdded == 1;
    };
    auto prepareSpawnPads = [](World &world, const glm::vec3 &center,
                               int spawnEpoch) {
        for (std::size_t attempt = 0;
             attempt < World::NaturalMobSpawnAttemptsPerCycle; ++attempt) {
            const glm::ivec2 offset = World::naturalMobSpawnOffset(
                world.collectDebugStats().terrainSeed, spawnEpoch, attempt);
            const int x = World::toBlockCoord(center.x) + offset.x;
            const int z = World::toBlockCoord(center.z) + offset.y;
            const VectorXZ chunkPosition = World::getChunkXZ(x, z);
            if (world.getChunkManager().findChunk(
                    chunkPosition.x, chunkPosition.z) == nullptr) {
                continue;
            }
            world.setBlock(x, 99, z, BlockId::Stone);
            world.setBlock(x, 100, z, BlockId::Air);
            world.setBlock(x, 101, z, BlockId::Air);
        }
    };

    std::uint32_t flagsBeforeRelaunch = 0;
    int stonePickaxeDurability = 0;
    int ironBeforeRelaunch = 0;
    int mobLootBeforeRelaunch = 0;
    int pickedMobLootAmount = 0;
    Material::ID defeatedLootMaterial = Material::ID::Nothing;
    ActorId defeatedMobId = InvalidActorId;
    {
        Player player;
        World world(camera, config, player, opened.directoryPath, false, 1);
        const AlphaJourneySnapshot initial =
            world.getAlphaJourneySnapshot();
        check("G6/clean-world-starts-with-minimal-wood-guidance",
              initial.step == AlphaJourneyStep::GatherWood &&
                  initial.completedSteps == 0 && initial.required ==
                      AlphaJourney::RequiredOakBark);

        for (int x = 2; x <= 15; ++x) {
            for (int z = 3; z <= 12; ++z) {
                world.setBlock(x, 99, z, BlockId::Stone);
                world.setBlock(x, 100, z, BlockId::Air);
                world.setBlock(x, 101, z, BlockId::Air);
            }
        }
        for (const glm::ivec3 &position : oakPositions) {
            world.setBlock(position.x, position.y, position.z,
                           BlockId::OakBark);
        }
        for (const glm::ivec3 &position : stonePositions) {
            world.setBlock(position.x, position.y, position.z,
                           BlockId::Stone);
        }
        world.setBlock(ironPosition.x, ironPosition.y, ironPosition.z,
                       BlockId::IronOre);
        prepareSpawnPads(world, player.position, 1);

        bool gatheredWood = true;
        for (const glm::ivec3 &position : oakPositions) {
            gatheredWood = gatheredWood &&
                BlockInteractionSystem::breakBlock(
                    world, player,
                    glm::vec3(position) + glm::vec3(0.5f));
        }
        check("G6/gather-eleven-wood-through-break-path",
              gatheredWood &&
                  countPlayer(player, Material::ID::OakBark) ==
                      AlphaJourney::RequiredOakBark &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CraftWorkbench);

        player.openCrafting(CraftingSession::PlayerGridSize);
        const bool workbenchCrafted = craft(
            player, CraftingSession::PlayerGridSize,
            {{0, Material::ID::OakBark}, {1, Material::ID::OakBark},
             {2, Material::ID::OakBark}, {3, Material::ID::OakBark}},
            Material::ID::Workbench);
        player.closeCrafting();
        check("G6/craft-workbench-through-player-grid",
              workbenchCrafted &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::PlaceWorkbench);

        const bool workbenchPlaced =
            selectPlayerSlot(
                player, findPlayerSlot(player, Material::ID::Workbench)) &&
            BlockInteractionSystem::placeBlock(
                world, player,
                glm::vec3(workbenchPosition) + glm::vec3(0.5f));
        check("G6/place-workbench-through-world-interaction",
              workbenchPlaced &&
                  static_cast<BlockId>(world.getBlock(
                      workbenchPosition.x, workbenchPosition.y,
                      workbenchPosition.z).id) == BlockId::Workbench &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CraftWoodenPickaxe);

        const bool workbenchOpened = BlockInteractionSystem::useBlock(
            world, player,
            glm::vec3(workbenchPosition) + glm::vec3(0.5f));
        const bool woodenPickaxeCrafted = workbenchOpened && craft(
            player, CraftingSession::WorkbenchGridSize,
            {{0, Material::ID::OakBark}, {1, Material::ID::OakBark},
             {2, Material::ID::OakBark}, {4, Material::ID::OakBark},
             {7, Material::ID::OakBark}},
            Material::ID::WoodenPickaxe);
        player.closeCrafting();
        check("G6/craft-wooden-pickaxe-at-placed-workbench",
              woodenPickaxeCrafted &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::GatherStone);

        const int woodenSlot =
            findPlayerSlot(player, Material::ID::WoodenPickaxe);
        bool gatheredStone = selectPlayerSlot(player, woodenSlot);
        for (const glm::ivec3 &position : stonePositions) {
            gatheredStone = gatheredStone &&
                BlockInteractionSystem::breakBlock(
                    world, player,
                    glm::vec3(position) + glm::vec3(0.5f));
        }
    check("G6/wooden-pickaxe-gathers-stone-and-loses-durability",
              gatheredStone &&
                  countPlayer(player, Material::ID::Cobblestone) ==
                      AlphaJourney::RequiredStone &&
                  player.getInventorySlot(woodenSlot).getDurability() == 13 &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CraftStonePickaxe);

        const bool reopenedWorkbench = BlockInteractionSystem::useBlock(
            world, player,
            glm::vec3(workbenchPosition) + glm::vec3(0.5f));
        const bool stonePickaxeCrafted = reopenedWorkbench && craft(
            player, CraftingSession::WorkbenchGridSize,
            {{0, Material::ID::Cobblestone},
             {1, Material::ID::Cobblestone},
             {2, Material::ID::Cobblestone},
             {4, Material::ID::OakBark},
             {7, Material::ID::OakBark}},
            Material::ID::StonePickaxe);
        player.closeCrafting();
        check("G6/craft-stone-pickaxe-through-progression-recipe",
              stonePickaxeCrafted &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::GatherIronOre);

        const int stoneSlot =
            findPlayerSlot(player, Material::ID::StonePickaxe);
        const bool ironGathered =
            selectPlayerSlot(player, stoneSlot) &&
            BlockInteractionSystem::breakBlock(
                world, player,
                glm::vec3(ironPosition) + glm::vec3(0.5f));
        stonePickaxeDurability =
            stoneSlot >= 0
                ? player.getInventorySlot(stoneSlot).getDurability()
                : 0;
        check("G6/stone-pickaxe-unlocks-iron-drop",
              ironGathered && stonePickaxeDurability == 31 &&
                  countPlayer(player, Material::ID::IronOre) == 1 &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::DefeatMob);

        world.tick(World::NaturalMobSpawnIntervalTicks);
        const std::vector<ActorSnapshot> encountered =
            world.collectActorSnapshots();
        const auto naturalMob = std::min_element(
            encountered.begin(), encountered.end(),
            [&player](const ActorSnapshot &left,
                      const ActorSnapshot &right) {
                const auto distanceSquared = [&player](
                                                 const ActorSnapshot &value) {
                    if (!World::isNaturalMobType(value.type)) {
                        return std::numeric_limits<float>::max();
                    }
                    const float x = value.position.x - player.position.x;
                    const float z = value.position.z - player.position.z;
                    return x * x + z * z;
                };
                return distanceSquared(left) < distanceSquared(right);
            });
        const bool foundNaturalMob =
            naturalMob != encountered.end() &&
            World::isNaturalMobType(naturalMob->type);
        if (foundNaturalMob) {
            defeatedMobId = naturalMob->id;
            const auto *mob = dynamic_cast<const MobActor *>(
                world.getActorManager().findActor(defeatedMobId));
            if (mob != nullptr && !mob->getLootTable().empty()) {
                defeatedLootMaterial =
                    mob->getLootTable().front().materialId;
            }
            player.position = naturalMob->position +
                glm::vec3(0.f, 0.f, 2.f);
            player.box.update(player.position);
            const int mobX = World::toBlockCoord(naturalMob->position.x);
            const int mobZ = World::toBlockCoord(naturalMob->position.z);
            const int playerZ = World::toBlockCoord(player.position.z);
            const int groundY =
                World::toBlockCoord(naturalMob->position.y) - 1;
            for (int x = mobX - 1; x <= mobX + 1; ++x) {
                for (int z = std::min(mobZ, playerZ) - 1;
                     z <= std::max(mobZ, playerZ) + 1; ++z) {
                    world.setBlock(x, groundY, z, BlockId::Stone);
                    world.setBlock(x, groundY + 1, z, BlockId::Air);
                    world.setBlock(x, groundY + 2, z, BlockId::Air);
                }
            }
        }
        const bool firstHit = foundNaturalMob &&
                              world.attackActor(
                                  defeatedMobId,
                                  World::PlayerAttackDamage);
        int combatTick = World::NaturalMobSpawnIntervalTicks;
        bool mobReachedPlayer = false;
        while (firstHit && combatTick < 200 && !mobReachedPlayer) {
            world.tick(++combatTick);
            const Actor *target =
                world.getActorManager().findActor(defeatedMobId);
            const auto *combatMob = dynamic_cast<const MobActor *>(target);
            if (combatMob != nullptr) {
                const glm::vec3 separation = glm::max(
                    glm::abs(combatMob->position - player.position) -
                        combatMob->box.dimensions - player.box.dimensions,
                    glm::vec3(0.f));
                mobReachedPlayer = glm::length(separation) <=
                    combatMob->getCombatProfile().attackRange + 0.001f;
            }
        }
        const bool lethalHit = firstHit && mobReachedPlayer &&
                               world.attackActor(defeatedMobId, 100.f);
        const auto *remainingMob = dynamic_cast<const MobActor *>(
            world.getActorManager().findActor(defeatedMobId));
        check("G6/natural-mob-is-defeated-through-combat-rules",
              foundNaturalMob && lethalHit &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CollectMobLoot,
              "found=" + std::to_string(foundNaturalMob) +
                  " first=" + std::to_string(firstHit) +
                  " reached=" + std::to_string(mobReachedPlayer) +
                  " lethal=" + std::to_string(lethalHit) +
                  " state=" +
                  (remainingMob != nullptr
                       ? std::string(mobCombatStateName(
                             remainingMob->getCombatState())) + "/" +
                             mobCombatTransitionReasonName(
                                 remainingMob->getLastCombatTransitionReason())
                       : "gone"));

        const int mobLootBeforePickup =
            countPlayer(player, defeatedLootMaterial);
        for (int pickupTick = 0; pickupTick < 11; ++pickupTick) {
            world.tick(++combatTick);
        }
        mobLootBeforeRelaunch =
            countPlayer(player, defeatedLootMaterial);
        pickedMobLootAmount =
            mobLootBeforeRelaunch - mobLootBeforePickup;
        check("G6/physical-mob-drop-is-picked-up",
              defeatedLootMaterial != Material::ID::Nothing &&
                  pickedMobLootAmount > 0 &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::ReopenWorld,
              "loot=" + std::to_string(mobLootBeforePickup) + "->" +
                  std::to_string(mobLootBeforeRelaunch) + " step=" +
                  std::to_string(static_cast<int>(
                      world.getAlphaJourneySnapshot().step)));

        flagsBeforeRelaunch =
            world.getAlphaJourneySnapshot().step ==
                    AlphaJourneyStep::ReopenWorld
                ? static_cast<std::uint32_t>(
                      world.getAlphaJourneySnapshot().completedSteps)
                : 0u;
        ironBeforeRelaunch =
            countPlayer(player, Material::ID::IronOre);
        const int totalBeforeSave =
            countPlayer(player, Material::ID::WoodenPickaxe) +
            countPlayer(player, Material::ID::StonePickaxe) +
            ironBeforeRelaunch + pickedMobLootAmount;
        check("G6/pre-save-state-conserves-tools-ore-and-loot",
              totalBeforeSave == 3 + pickedMobLootAmount &&
                  flagsBeforeRelaunch == 9u &&
                  world.getBlock(ironPosition.x, ironPosition.y,
                                 ironPosition.z).id == 0);

        const bool saved = world.save();
        WorldSaveData persisted;
        const bool metadataLoaded =
            WorldSave(opened.directoryPath).load(persisted);
        flagsBeforeRelaunch =
            metadataLoaded ? persisted.alphaJourneyFlags : 0u;
        check("G6/save-publishes-version-five-objective-progress",
              saved && metadataLoaded &&
                  persisted.version == WorldSaveFormatVersion &&
                  flagsBeforeRelaunch ==
                      (AlphaJourney::KnownFlags &
                       ~(1u << static_cast<unsigned>(
                           AlphaJourneyStep::ReopenWorld))));
    }

    const WorldManagementResult reopened =
        management.prepareWorldForOpen(created.worldId);
    Player restoredPlayer;
    World restoredWorld(camera, config, restoredPlayer,
                        reopened.succeeded() ? reopened.directoryPath
                                             : opened.directoryPath,
                        false, 1);
    const AlphaJourneySnapshot restoredJourney =
        restoredWorld.getAlphaJourneySnapshot();
    const int restoredStoneSlot =
        findPlayerSlot(restoredPlayer, Material::ID::StonePickaxe);
    const std::vector<ActorSaveState> restoredActors =
        restoredWorld.getActorManager().collectSaveStates();
    const bool defeatedMobRestored = std::any_of(
        restoredActors.begin(), restoredActors.end(),
        [defeatedMobId](const ActorSaveState &state) {
            return state.id == defeatedMobId;
        });
    check("G6/reopen-completes-journey-and-restores-conserved-state",
          reopened.succeeded() && restoredJourney.complete() &&
              restoredJourney.completedSteps == AlphaJourney::StepCount &&
              restoredStoneSlot >= 0 &&
              restoredPlayer.getInventorySlot(
                  restoredStoneSlot).getDurability() ==
                  stonePickaxeDurability &&
              countPlayer(restoredPlayer, Material::ID::IronOre) ==
                  ironBeforeRelaunch &&
              countPlayer(restoredPlayer, defeatedLootMaterial) ==
                  mobLootBeforeRelaunch &&
              static_cast<BlockId>(restoredWorld.getBlock(
                  workbenchPosition.x, workbenchPosition.y,
                  workbenchPosition.z).id) == BlockId::Workbench &&
              !defeatedMobRestored);

    const bool completedSaved = restoredWorld.save();
    WorldSave save(opened.directoryPath);
    WorldSaveData completeData;
    const bool completeLoaded = save.load(completeData);
    WorldSaveData invalidData = completeData;
    invalidData.alphaJourneyFlags |= (1u << 20u);
    const bool invalidRejected = !save.save(invalidData);
    WorldSaveData preservedData;
    check("G6/invalid-journey-bits-preserve-last-good-save",
          completedSaved && completeLoaded &&
              completeData.alphaJourneyFlags == AlphaJourney::KnownFlags &&
              invalidRejected && save.load(preservedData) &&
              preservedData.alphaJourneyFlags ==
                  AlphaJourney::KnownFlags);

    setEnv("HELLOMINE3D_SEED", "");
    clearDeterministicEnv();
    const auto randomDirectory =
        freshSaveDirectory("playable_alpha_random_seed");
    Player randomPlayer;
    World randomWorld(camera, config, randomPlayer, randomDirectory,
                      false, 0);
    WorldSaveData randomData;
    check("G6/random-seed-world-starts-with-valid-journey",
          randomWorld.getAlphaJourneySnapshot().step ==
                  AlphaJourneyStep::GatherWood &&
              WorldSave(randomDirectory).load(randomData) &&
              randomData.version == WorldSaveFormatVersion &&
              randomData.alphaJourneyFlags == 0u);
}

// ---------------------------------------------------------------------------
// S2.3 - versioned chunk format rejects corrupt files
// ---------------------------------------------------------------------------
void caseChunkFormatRejection()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("chunk_format");
    Config config = makeConfig();
    Camera camera(config);

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(8, 100, 8, BlockId::Stone);
        world.save();
    }

    // Corrupt the magic of the saved chunk file and confirm the loader falls
    // back to procedural generation instead of crashing or loading garbage.
    const ChunkStorage storage(
        (std::filesystem::path(directory) / "chunks").string());
    const std::filesystem::path chunkFile = storage.chunkPath(0, 0);
    const bool chunkFileExists = std::filesystem::exists(chunkFile);
    check("S2.2/chunk-file-path-deterministic", chunkFileExists,
          chunkFile.string());

    if (!chunkFileExists) {
        return;
    }

    {
        std::fstream file(chunkFile, std::ios::in | std::ios::out |
                                         std::ios::binary);
        if (file.is_open()) {
            file.seekp(0);
            const char garbage[4] = {'X', 'X', 'X', 'X'};
            file.write(garbage, sizeof(garbage));
        }
    }

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.preloadAround({8.f, 90.f, 8.f});
        // The corrupted chunk must not resurrect the saved edit; generation
        // takes over instead.
        check("S2.3/corrupt-chunk-rejected",
              world.getBlock(8, 100, 8).id !=
                  static_cast<Block_t>(BlockId::Stone),
              "block id=" +
                  std::to_string(
                      static_cast<int>(world.getBlock(8, 100, 8).id)));
    }
}

// ---------------------------------------------------------------------------
// S6.1 / S6.2 / S6.4 - deterministic terrain and ore decorators
// ---------------------------------------------------------------------------
struct TerrainSample {
    std::vector<Block_t> blocks;
    std::vector<std::size_t> caveAirIndices;
    int coalCount = 0;
    int ironCount = 0;
    int shallowAirCount = 0;
};

TerrainSample sampleTerrain(const std::string &directoryName)
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory(directoryName);
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    TerrainSample sample;
    sample.blocks.reserve(3 * 3 * CHUNK_SIZE * CHUNK_SIZE * 8);

    for (int chunkX = -1; chunkX <= 1; ++chunkX) {
        for (int chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            const Chunk *chunk =
                world.getChunkManager().findChunk(chunkX, chunkZ);
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const int surfaceY =
                        chunk != nullptr ? chunk->getHeightAt(x, z) : 0;
                    for (int y = 4; y < 80; ++y) {
                        const auto block =
                            world.getBlock(chunkX * CHUNK_SIZE + x, y,
                                           chunkZ * CHUNK_SIZE + z);
                        sample.blocks.push_back(block.id);
                        if (block.id ==
                                static_cast<Block_t>(BlockId::Air) &&
                            y >= 8 &&
                            y <= std::min(surfaceY - 5,
                                          WATER_LEVEL - 8)) {
                            sample.caveAirIndices.push_back(
                                sample.blocks.size() - 1);
                        }
                        if (block.id ==
                                static_cast<Block_t>(BlockId::Air) &&
                            y >= std::max(0, surfaceY - 4) &&
                            y <= surfaceY) {
                            ++sample.shallowAirCount;
                        }
                        if (block.id ==
                            static_cast<Block_t>(BlockId::CoalOre)) {
                            ++sample.coalCount;
                        }
                        if (block.id ==
                            static_cast<Block_t>(BlockId::IronOre)) {
                            ++sample.ironCount;
                        }
                    }
                }
            }
        }
    }

    return sample;
}

void caseTerrainDeterminism()
{
    const auto first = sampleTerrain("terrain_seed_a");
    const auto second = sampleTerrain("terrain_seed_b");

    check("S6.1/same-seed-same-terrain", first.blocks == second.blocks,
          "sampled " + std::to_string(first.blocks.size()) + " blocks");
    check("S6.4/ore-decorator-produces-ore",
          first.coalCount > 0 && first.ironCount > 0,
          "coal=" + std::to_string(first.coalCount) +
              " iron=" + std::to_string(first.ironCount));
    check("S6.4/ore-layout-deterministic",
          first.coalCount == second.coalCount &&
              first.ironCount == second.ironCount,
          "coal " + std::to_string(first.coalCount) + "/" +
              std::to_string(second.coalCount) + " iron " +
              std::to_string(first.ironCount) + "/" +
              std::to_string(second.ironCount));
    check("C4/cave-pass-produces-underground-air",
          !first.caveAirIndices.empty(),
          "air=" + std::to_string(first.caveAirIndices.size()));
    check("C4/cave-layout-deterministic",
          first.caveAirIndices == second.caveAirIndices,
          "air " + std::to_string(first.caveAirIndices.size()) + "/" +
              std::to_string(second.caveAirIndices.size()));
    check("C4/surface-buffer-remains-solid",
          first.shallowAirCount == 0 && second.shallowAirCount == 0,
          "shallow air " + std::to_string(first.shallowAirCount) + "/" +
              std::to_string(second.shallowAirCount));
}

// ---------------------------------------------------------------------------
// S6.2 / S6.3 / S6.5 - decorators, biome surfaces and structure stability
// ---------------------------------------------------------------------------
struct SurfaceSample {
    int treeBlocks = 0;
    int plantBlocks = 0;
    int crossChunkTreeLinks = 0;
    std::vector<int> topBlockIds;
};

// The sampled surface stops well below the marker blocks used to force chunks
// save-dirty, so the markers never change the measured surface composition.
constexpr int kSurfaceScanTop = 115;
constexpr int kSurfaceScanBottom = 40;
constexpr int kSaveMarkerY = 125;
constexpr int kStructureChunkRadius = 3;

// Sample the region the spawn search actually uses (chunk 100-200), not the
// world origin, which is open ocean/beach for most seeds.
constexpr int kStructureCenterChunk = 150;

void loadChunkArea(World &world, int centerChunk, int chunkRadius)
{
    auto &chunks = world.getChunkManager();
    for (int chunkX = centerChunk - chunkRadius;
         chunkX <= centerChunk + chunkRadius; ++chunkX) {
        for (int chunkZ = centerChunk - chunkRadius;
             chunkZ <= centerChunk + chunkRadius; ++chunkZ) {
            chunks.loadChunk(chunkX, chunkZ);
        }
    }
}

SurfaceSample sampleSurface(World &world, int centerChunk, int chunkRadius)
{
    SurfaceSample sample;
    std::array<bool, static_cast<int>(BlockId::NUM_TYPES)> seenTop{};
    const auto isTreeBlock = [](ChunkBlock block) {
        const auto id = static_cast<BlockId>(block.id);
        return id == BlockId::OakBark || id == BlockId::OakLeaf;
    };

    for (int chunkX = centerChunk - chunkRadius;
         chunkX <= centerChunk + chunkRadius; ++chunkX) {
        for (int chunkZ = centerChunk - chunkRadius;
             chunkZ <= centerChunk + chunkRadius; ++chunkZ) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    const int worldX = chunkX * CHUNK_SIZE + x;
                    const int worldZ = chunkZ * CHUNK_SIZE + z;

                    bool foundTop = false;
                    for (int y = kSurfaceScanTop; y >= kSurfaceScanBottom;
                         --y) {
                        const auto block = world.getBlock(worldX, y, worldZ);
                        const auto id = static_cast<BlockId>(block.id);
                        if (id == BlockId::OakBark || id == BlockId::OakLeaf) {
                            ++sample.treeBlocks;
                        }
                        if (id == BlockId::TallGrass || id == BlockId::Rose ||
                            id == BlockId::DeadShrub || id == BlockId::Cactus) {
                            ++sample.plantBlocks;
                        }

                        if (!foundTop && block.id != 0 &&
                            id != BlockId::Water) {
                            foundTop = true;
                            seenTop[block.id] = true;
                        }
                    }
                }
            }
        }
    }

    for (int id = 0; id < static_cast<int>(BlockId::NUM_TYPES); ++id) {
        if (seenTop[id]) {
            sample.topBlockIds.push_back(id);
        }
    }

    const int minimumChunk = centerChunk - chunkRadius;
    const int maximumChunk = centerChunk + chunkRadius;
    for (int chunkX = minimumChunk; chunkX < maximumChunk; ++chunkX) {
        const int boundaryX = (chunkX + 1) * CHUNK_SIZE;
        for (int chunkZ = minimumChunk; chunkZ <= maximumChunk; ++chunkZ) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int worldZ = chunkZ * CHUNK_SIZE + z;
                for (int y = kSurfaceScanBottom; y <= kSurfaceScanTop; ++y) {
                    if (isTreeBlock(world.getBlock(boundaryX - 1, y,
                                                   worldZ)) &&
                        isTreeBlock(world.getBlock(boundaryX, y, worldZ))) {
                        ++sample.crossChunkTreeLinks;
                    }
                }
            }
        }
    }
    for (int chunkZ = minimumChunk; chunkZ < maximumChunk; ++chunkZ) {
        const int boundaryZ = (chunkZ + 1) * CHUNK_SIZE;
        for (int chunkX = minimumChunk; chunkX <= maximumChunk; ++chunkX) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                const int worldX = chunkX * CHUNK_SIZE + x;
                for (int y = kSurfaceScanBottom; y <= kSurfaceScanTop; ++y) {
                    if (isTreeBlock(world.getBlock(worldX, y,
                                                   boundaryZ - 1)) &&
                        isTreeBlock(world.getBlock(worldX, y, boundaryZ))) {
                        ++sample.crossChunkTreeLinks;
                    }
                }
            }
        }
    }

    return sample;
}

std::string idListToString(const std::vector<int> &ids)
{
    std::string text;
    for (int id : ids) {
        text += std::to_string(id) + " ";
    }
    return text;
}

void caseTerrainStructures()
{
    const int centerBlock = kStructureCenterChunk * CHUNK_SIZE;
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION",
           std::to_string(centerBlock) + " 90 " + std::to_string(centerBlock));
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("terrain_structures");
    const bool terrainV3Prepared = initializeTerrainIdentity(
        directory, "terrain-structures-v3",
        ExplorationSiteTerrainGenerationVersion);
    Config config = makeConfig();
    Camera camera(config);

    SurfaceSample before;
    const int chunkSpan = kStructureChunkRadius * 2 + 1;
    const std::size_t chunkCount =
        static_cast<std::size_t>(chunkSpan) * chunkSpan;

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        check("S6.2/terrain-v3-fixture-prepared", terrainV3Prepared &&
                  world.collectDebugStats().terrainGenerationVersion ==
                      ExplorationSiteTerrainGenerationVersion);
        loadChunkArea(world, kStructureCenterChunk, kStructureChunkRadius);
        before = sampleSurface(world, kStructureCenterChunk,
                               kStructureChunkRadius);

        check("S6.2/decorators-produce-trees", before.treeBlocks > 0,
              "tree blocks=" + std::to_string(before.treeBlocks) + " over " +
                  std::to_string(chunkCount) + " chunks");
        check("S6.2/decorators-produce-plants", before.plantBlocks > 0,
              "plant blocks=" + std::to_string(before.plantBlocks));
        check("S6.3/biome-surface-variety", before.topBlockIds.size() >= 2,
              "distinct surface block ids: " +
                  idListToString(before.topBlockIds));
        check("C5/structures-cross-chunk-boundaries",
              before.crossChunkTreeLinks > 0,
              "links=" +
                  std::to_string(before.crossChunkTreeLinks));

        ClassicOverWorldGenerator forwardGenerator(
            kValidationSeed, ExplorationSiteTerrainGenerationVersion);
        ClassicOverWorldGenerator reverseGenerator(
            kValidationSeed, ExplorationSiteTerrainGenerationVersion);
        Chunk forwardWest(world, {kStructureCenterChunk,
                                  kStructureCenterChunk});
        Chunk forwardEast(world, {kStructureCenterChunk + 1,
                                  kStructureCenterChunk});
        Chunk reverseWest(world, {kStructureCenterChunk,
                                  kStructureCenterChunk});
        Chunk reverseEast(world, {kStructureCenterChunk + 1,
                                  kStructureCenterChunk});
        forwardWest.load(forwardGenerator);
        forwardEast.load(forwardGenerator);
        reverseEast.load(reverseGenerator);
        reverseWest.load(reverseGenerator);

        std::vector<Block_t> forwardWestBlocks;
        std::vector<Block_t> forwardEastBlocks;
        std::vector<Block_t> reverseWestBlocks;
        std::vector<Block_t> reverseEastBlocks;
        std::vector<BlockMetadata_t> metadata;
        forwardWest.collectBlockData(forwardWestBlocks, metadata);
        forwardEast.collectBlockData(forwardEastBlocks, metadata);
        reverseWest.collectBlockData(reverseWestBlocks, metadata);
        reverseEast.collectBlockData(reverseEastBlocks, metadata);
        check("C5/structure-output-ignores-load-order",
              forwardWestBlocks == reverseWestBlocks &&
                  forwardEastBlocks == reverseEastBlocks,
              "west/east blocks=" +
                  std::to_string(forwardWestBlocks.size()) + "/" +
                  std::to_string(forwardEastBlocks.size()));

        // Dirty every sampled chunk so the whole neighbourhood goes through
        // the save/load path rather than being regenerated on relaunch.
        for (int chunkX = kStructureCenterChunk - kStructureChunkRadius;
             chunkX <= kStructureCenterChunk + kStructureChunkRadius;
             ++chunkX) {
            for (int chunkZ = kStructureCenterChunk - kStructureChunkRadius;
                 chunkZ <= kStructureCenterChunk + kStructureChunkRadius;
                 ++chunkZ) {
                world.setBlock(chunkX * CHUNK_SIZE + 1, kSaveMarkerY,
                               chunkZ * CHUNK_SIZE + 1, BlockId::Stone);
            }
        }

        check("S6.5/all-sampled-chunks-dirty",
              world.collectDebugStats().chunks.saveDirtyChunks == chunkCount,
              "save dirty chunks=" +
                  std::to_string(
                      world.collectDebugStats().chunks.saveDirtyChunks) +
                  "/" + std::to_string(chunkCount));
        check("S6.5/save-before-reload", world.save());
    }

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        loadChunkArea(world, kStructureCenterChunk, kStructureChunkRadius);
        const SurfaceSample after = sampleSurface(world, kStructureCenterChunk,
                                                  kStructureChunkRadius);

        check("S6.5/structures-survive-reload",
              after.treeBlocks == before.treeBlocks &&
                  after.plantBlocks == before.plantBlocks &&
                  after.crossChunkTreeLinks ==
                      before.crossChunkTreeLinks,
              "trees " + std::to_string(before.treeBlocks) + " -> " +
                  std::to_string(after.treeBlocks) + ", plants " +
                  std::to_string(before.plantBlocks) + " -> " +
                  std::to_string(after.plantBlocks) + ", links " +
                  std::to_string(before.crossChunkTreeLinks) + " -> " +
                  std::to_string(after.crossChunkTreeLinks));
        check("S6.5/surface-composition-stable",
              after.topBlockIds == before.topBlockIds,
              idListToString(before.topBlockIds) + "-> " +
                  idListToString(after.topBlockIds));
    }
}

// ---------------------------------------------------------------------------
// N5 - versioned ecology, biome danger and seed-stable Waystone landmarks
// ---------------------------------------------------------------------------
struct GeneratedChunkSample {
    int x = 0;
    int z = 0;
    std::vector<Block_t> blocks;
};

std::vector<GeneratedChunkSample> sampleLandmarkChunks(
    World &world, ClassicOverWorldGenerator &generator,
    const ClassicOverWorldGenerator::LandmarkPlacement &landmark,
    bool reverseOrder)
{
    const int minimumChunkX = World::floorDiv(
        landmark.x - ClassicOverWorldGenerator::LandmarkRadius,
        CHUNK_SIZE);
    const int maximumChunkX = World::floorDiv(
        landmark.x + ClassicOverWorldGenerator::LandmarkRadius,
        CHUNK_SIZE);
    const int minimumChunkZ = World::floorDiv(
        landmark.z - ClassicOverWorldGenerator::LandmarkRadius,
        CHUNK_SIZE);
    const int maximumChunkZ = World::floorDiv(
        landmark.z + ClassicOverWorldGenerator::LandmarkRadius,
        CHUNK_SIZE);
    std::vector<glm::ivec2> positions;
    for (int chunkX = minimumChunkX; chunkX <= maximumChunkX; ++chunkX) {
        for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ; ++chunkZ) {
            positions.push_back({chunkX, chunkZ});
        }
    }
    if (reverseOrder) {
        std::reverse(positions.begin(), positions.end());
    }

    std::vector<GeneratedChunkSample> samples;
    for (const glm::ivec2 &position : positions) {
        Chunk chunk(world, position);
        chunk.load(generator);
        GeneratedChunkSample sample;
        sample.x = position.x;
        sample.z = position.y;
        std::vector<BlockMetadata_t> metadata;
        chunk.collectBlockData(sample.blocks, metadata);
        samples.push_back(std::move(sample));
    }
    std::sort(samples.begin(), samples.end(),
              [](const GeneratedChunkSample &left,
                 const GeneratedChunkSample &right) {
                  return left.x != right.x ? left.x < right.x
                                           : left.z < right.z;
              });
    return samples;
}

bool sameStructurePlan(const StructurePlanSnapshot &left,
                       const StructurePlanSnapshot &right)
{
    return left.key == right.key && left.valid == right.valid &&
           left.anchor == right.anchor &&
           left.footprint.minimumX == right.footprint.minimumX &&
           left.footprint.maximumX == right.footprint.maximumX &&
           left.footprint.minimumY == right.footprint.minimumY &&
           left.footprint.maximumY == right.footprint.maximumY &&
           left.footprint.minimumZ == right.footprint.minimumZ &&
           left.footprint.maximumZ == right.footprint.maximumZ &&
           left.biome == right.biome &&
           left.projectionPriority == right.projectionPriority &&
           left.selectedCandidate == right.selectedCandidate &&
           left.plannedBlockCount == right.plannedBlockCount &&
           left.selectionHash == right.selectionHash &&
           left.hasChest == right.hasChest &&
           left.chestPosition == right.chestPosition;
}

bool sameStructurePlans(const std::vector<StructurePlanSnapshot> &left,
                        const std::vector<StructurePlanSnapshot> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!sameStructurePlan(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

void caseEcologyAndExploration()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    Config config = makeConfig();
    Camera camera(config);
    const auto directory = freshSaveDirectory("n5_ecology");
    Player player;
    World world(camera, config, player, directory, false, 1);

    ClassicOverWorldGenerator currentGenerator(
        kValidationSeed, WaystoneTerrainGenerationVersion);
    ClassicOverWorldGenerator legacyGenerator(
        kValidationSeed, LegacyTerrainGenerationVersion);
    ClassicOverWorldGenerator otherSeedGenerator(
        kValidationSeed + 1, WaystoneTerrainGenerationVersion);
    ClassicOverWorldGenerator::LandmarkPlacement landmark;
    int landmarkCellX = 0;
    int landmarkCellZ = 0;
    for (int cellX = -8; cellX <= 8 && !landmark.valid; ++cellX) {
        for (int cellZ = -8; cellZ <= 8 && !landmark.valid; ++cellZ) {
            const auto candidate =
                currentGenerator.getLandmarkForCell(cellX, cellZ);
            if (candidate.valid) {
                landmark = candidate;
                landmarkCellX = cellX;
                landmarkCellZ = cellZ;
            }
        }
    }
    const auto otherLandmark = otherSeedGenerator.getLandmarkForCell(
        landmarkCellX, landmarkCellZ);
    check("N5/landmark-anchor-is-seed-stable-and-seed-sensitive",
          landmark.valid && otherLandmark.valid &&
              (landmark.x != otherLandmark.x ||
               landmark.z != otherLandmark.z ||
               landmark.y != otherLandmark.y),
          "anchor=" + std::to_string(landmark.x) + "," +
              std::to_string(landmark.y) + "," +
              std::to_string(landmark.z));

    const auto forward = sampleLandmarkChunks(
        world, currentGenerator, landmark, false);
    const auto reverse = sampleLandmarkChunks(
        world, currentGenerator, landmark, true);
    const auto legacy = sampleLandmarkChunks(
        world, legacyGenerator, landmark, false);
    bool sameLoadOrderOutput = forward.size() == reverse.size();
    int currentCoreCount = 0;
    int legacyCoreCount = 0;
    for (std::size_t index = 0;
         index < forward.size() && index < reverse.size(); ++index) {
        sameLoadOrderOutput = sameLoadOrderOutput &&
            forward[index].x == reverse[index].x &&
            forward[index].z == reverse[index].z &&
            forward[index].blocks == reverse[index].blocks;
        currentCoreCount += static_cast<int>(std::count(
            forward[index].blocks.begin(), forward[index].blocks.end(),
            static_cast<Block_t>(BlockId::WaystoneCore)));
        legacyCoreCount += static_cast<int>(std::count(
            legacy[index].blocks.begin(), legacy[index].blocks.end(),
            static_cast<Block_t>(BlockId::WaystoneCore)));
    }
    check("N5/landmark-output-ignores-chunk-load-order",
          sameLoadOrderOutput,
          "chunks=" + std::to_string(forward.size()));
    check("N5/new-generation-adds-one-bounded-waystone-core",
          currentCoreCount == 1,
          "cores=" + std::to_string(currentCoreCount));
    check("N5/legacy-generation-does-not-backfill-landmarks",
          legacyCoreCount == 0,
          "legacy cores=" + std::to_string(legacyCoreCount));

    std::array<bool, 5> observedBiomes{};
    for (int x = -4096; x <= 4096; x += 64) {
        for (int z = -4096; z <= 4096; z += 64) {
            const TerrainBiome biome =
                currentGenerator.getBiomeAtWorld(x, z);
            observedBiomes[static_cast<std::size_t>(biome)] = true;
        }
    }
    check("N5/five-biome-identities-are-queryable",
          std::all_of(observedBiomes.begin(), observedBiomes.end(),
                      [](bool observed) { return observed; }));
    check("N5/biomes-select-readable-enemy-pressure",
          std::string(World::naturalMobTypeForBiome(
              TerrainBiome::Desert)) == World::BruteMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::Grassland)) == World::StalkerMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::LightForest)) == World::StalkerMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::TemperateForest)) == World::SpitterMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::Ocean)) == World::StalkerMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::Mountain)) == World::BruteMobType);

    const BlockDefinition &coreDefinition =
        BlockDatabase::get().getDefinition(BlockId::WaystoneCore);
    check("N5/waystone-core-is-a-tier-three-placeable-trophy",
          coreDefinition.requiredToolTier == 3 &&
              coreDefinition.miningClass == MiningClass::Pickaxe &&
              coreDefinition.light == 12 &&
              coreDefinition.defaultDrop == Material::ID::WaystoneCore &&
              Material::WAYSTONE_CORE.toBlockID() ==
                  BlockId::WaystoneCore);

    WorldSave currentSave(directory);
    WorldSaveData currentData;
    const bool currentLoaded = currentSave.load(currentData);
    WorldSaveData invalidGeneration = currentData;
    invalidGeneration.terrainGenerationVersion =
        CurrentTerrainGenerationVersion + 1;
    WorldSaveData preservedCurrent;
    check("P11-2/new-world-persists-terrain-v4-and-rejects-v5",
          currentLoaded &&
              currentData.version == WorldSaveFormatVersion &&
              currentData.terrainGenerationVersion ==
                  CurrentTerrainGenerationVersion &&
              !currentSave.save(invalidGeneration) &&
              currentSave.load(preservedCurrent) &&
              preservedCurrent.terrainGenerationVersion ==
                  CurrentTerrainGenerationVersion);

    const std::filesystem::path migrationRoot =
        freshSaveDirectory("n5_generation_migration");
    const std::filesystem::path migratedWorld =
        migrationRoot / "n5-v7-generation";
    std::filesystem::create_directories(migratedWorld);
    std::filesystem::copy_file(
        ResourcePaths::join(
            ResourcePaths::projectRoot(),
            "tools/fixtures/exploration/world-v7-generation.meta"),
        migratedWorld / "world.meta",
        std::filesystem::copy_options::overwrite_existing);
    const WorldManagementService management(migrationRoot.string());
    const WorldManagementResult migrated =
        management.prepareWorldForOpen("n5-v7-generation");
    WorldSaveData migratedData;
    const bool migratedLoaded = migrated.succeeded() &&
        WorldSave(migratedWorld.string()).load(migratedData);
    std::ifstream migratedInput(migratedWorld / "world.meta",
                                std::ios::binary);
    const std::string migratedText(
        (std::istreambuf_iterator<char>(migratedInput)),
        std::istreambuf_iterator<char>());
    check("N5/v7-world-migrates-with-legacy-generation-identity",
          migratedLoaded &&
              migratedData.version == WorldSaveFormatVersion &&
              migratedData.terrainGenerationVersion ==
                  LegacyTerrainGenerationVersion &&
              migratedText.find("terrain_generation_version 1") !=
                  std::string::npos);

    if (migratedLoaded && landmark.valid) {
        Player legacyPlayer;
        World legacyWorld(camera, config, legacyPlayer,
                          migratedWorld.string(), false, 1);
        const VectorXZ landmarkChunk = World::getChunkXZ(
            landmark.x, landmark.z);
        legacyWorld.getChunkManager().loadChunk(
            landmarkChunk.x, landmarkChunk.z);
        check("N5/migrated-world-keeps-old-unexplored-chunk-output",
              legacyWorld.collectDebugStats().terrainGenerationVersion ==
                      LegacyTerrainGenerationVersion &&
                  static_cast<BlockId>(legacyWorld.getBlock(
                      landmark.x, landmark.y + 3, landmark.z).id) !=
                      BlockId::WaystoneCore);
    }
    else {
        check("N5/migrated-world-keeps-old-unexplored-chunk-output", false);
    }
}

// ---------------------------------------------------------------------------
// N9A - deterministic structure ownership, planning and chunk projection
// ---------------------------------------------------------------------------
void caseDeterministicStructurePlanning()
{
    check("N9A/structure-type-and-cap-contract",
          std::string(structureTypeName(StructureType::Waystone)) ==
                  "waystone" &&
              std::string(structureTypeName(
                  static_cast<StructureType>(255))) == "unknown" &&
              DeterministicStructurePlanner::WaystoneCellChunks == 4 &&
              DeterministicStructurePlanner::WaystoneCandidateCount == 8 &&
              DeterministicStructurePlanner::MaximumPlansPerChunk == 4);

    ClassicOverWorldGenerator currentGenerator(
        kValidationSeed, WaystoneTerrainGenerationVersion);
    ClassicOverWorldGenerator otherSeedGenerator(
        kValidationSeed + 1, WaystoneTerrainGenerationVersion);
    ClassicOverWorldGenerator legacyGenerator(
        kValidationSeed, LegacyTerrainGenerationVersion);
    const StructurePlanSnapshot frozen =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    check("N9A/terrain-v2-waystone-plan-keeps-frozen-identity",
          frozen.valid && frozen.key.type == StructureType::Waystone &&
              frozen.key.terrainGenerationVersion ==
                  WaystoneTerrainGenerationVersion &&
              frozen.key.cellX == 0 && frozen.key.cellZ == 5 &&
              frozen.anchor == glm::ivec3(18, 70, 328),
          "anchor=" + std::to_string(frozen.anchor.x) + "," +
              std::to_string(frozen.anchor.y) + "," +
              std::to_string(frozen.anchor.z));

    const StructurePlanSnapshot repeated =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    const StructurePlanSnapshot otherSeed =
        otherSeedGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    check("N9A/plan-is-seed-stable-and-seed-sensitive",
          sameStructurePlan(frozen, repeated) &&
              (frozen.anchor != otherSeed.anchor ||
               frozen.selectionHash != otherSeed.selectionHash));

    const StructurePlanSnapshot legacy =
        legacyGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    const StructurePlanSnapshot unknown =
        currentGenerator.getStructurePlanForCell(
            static_cast<StructureType>(255), 0, 5);
    check("N9A/legacy-and-unknown-structure-plans-are-rejected",
          !legacy.valid && !legacy.footprint.valid() &&
              legacy.key.terrainGenerationVersion ==
                  LegacyTerrainGenerationVersion &&
              !unknown.valid && !unknown.footprint.valid() &&
              unknown.key.type == static_cast<StructureType>(255));

    const StructurePlanSnapshot negative =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, -2, -3);
    const int cellSize =
        DeterministicStructurePlanner::WaystoneCellChunks * CHUNK_SIZE;
    const int inset = DeterministicStructurePlanner::WaystoneRadius + 1;
    check("N9A/negative-cell-ownership-uses-floor-coordinates",
          negative.anchor.x >= -2 * cellSize + inset &&
              negative.anchor.x < -1 * cellSize - inset &&
              negative.anchor.z >= -3 * cellSize + inset &&
              negative.anchor.z < -2 * cellSize - inset,
          "anchor=" + std::to_string(negative.anchor.x) + "," +
              std::to_string(negative.anchor.z));

    check("N9A/footprint-and-plan-size-are-bounded",
          frozen.footprint.width() == 5 &&
              frozen.footprint.height() == 6 &&
              frozen.footprint.depth() == 5 &&
              frozen.selectedCandidate >= 0 &&
              frozen.selectedCandidate <
                  DeterministicStructurePlanner::WaystoneCandidateCount &&
              frozen.plannedBlockCount == 195 &&
              frozen.selectionHash != 0);

    const StructurePlanSnapshot adjacentX =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 1, 5);
    const StructurePlanSnapshot adjacentZ =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 6);
    check("N9A/adjacent-cells-respect-minimum-anchor-spacing",
          std::abs(adjacentX.anchor.x - frozen.anchor.x) >=
                  DeterministicStructurePlanner::
                      WaystoneMinimumAnchorSpacing &&
              std::abs(adjacentZ.anchor.z - frozen.anchor.z) >=
                  DeterministicStructurePlanner::
                      WaystoneMinimumAnchorSpacing);

    std::srand(0x4e3941);
    const int expectedFirstRandom = std::rand();
    const int expectedSecondRandom = std::rand();
    std::srand(0x4e3941);
    const int observedFirstRandom = std::rand();
    const StructurePlanSnapshot randomIsolated =
        currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    const int observedSecondRandom = std::rand();
    check("N9A/planning-does-not-consume-global-rand",
          expectedFirstRandom == observedFirstRandom &&
              expectedSecondRandom == observedSecondRandom &&
              sameStructurePlan(frozen, randomIsolated));

    const int frozenChunkX = World::floorDiv(frozen.anchor.x, CHUNK_SIZE);
    const int frozenChunkZ = World::floorDiv(frozen.anchor.z, CHUNK_SIZE);
    const std::vector<StructurePlanSnapshot> firstQuery =
        currentGenerator.getStructurePlansForChunk(
            frozenChunkX, frozenChunkZ);
    const std::vector<StructurePlanSnapshot> secondQuery =
        currentGenerator.getStructurePlansForChunk(
            frozenChunkX, frozenChunkZ);
    check("N9A/chunk-plan-query-is-repeatable-and-capped",
          sameStructurePlans(firstQuery, secondQuery) &&
              !firstQuery.empty() &&
              firstQuery.size() <=
                  DeterministicStructurePlanner::MaximumPlansPerChunk,
          "plans=" + std::to_string(firstQuery.size()));

    bool ownedOverlapsOnly = true;
    for (const StructurePlanSnapshot &plan : firstQuery) {
        const int ownerMinimumX = plan.key.cellX * cellSize + inset;
        const int ownerMinimumZ = plan.key.cellZ * cellSize + inset;
        ownedOverlapsOnly = ownedOverlapsOnly &&
            plan.footprint.overlapsChunk(frozenChunkX, frozenChunkZ) &&
            plan.anchor.x >= ownerMinimumX &&
            plan.anchor.x < ownerMinimumX + cellSize - inset * 2 &&
            plan.anchor.z >= ownerMinimumZ &&
            plan.anchor.z < ownerMinimumZ + cellSize - inset * 2;
    }
    check("N9A/chunk-query-returns-only-owned-overlaps",
          ownedOverlapsOnly && !firstQuery.empty());

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player,
                freshSaveDirectory("n9a_structure_planning"), false, 1);
    const ChunkDebugStats beforeQuery = world.collectDebugStats().chunks;
    const auto isolatedQuery = currentGenerator.getStructurePlansForChunk(
        frozenChunkX, frozenChunkZ);
    const ChunkDebugStats afterQuery = world.collectDebugStats().chunks;
    check("N9A/plan-query-does-not-load-neighbor-chunks",
          !isolatedQuery.empty() &&
              beforeQuery.existingChunks == afterQuery.existingChunks &&
              beforeQuery.loadedChunks == afterQuery.loadedChunks,
          "chunks=" + std::to_string(beforeQuery.existingChunks) +
              "->" + std::to_string(afterQuery.existingChunks));

    StructurePlanSnapshot lowPriority = frozen;
    lowPriority.key.cellX = 10;
    lowPriority.key.cellZ = 10;
    lowPriority.projectionPriority = 10;
    lowPriority.footprint.minimumY += 100;
    lowPriority.footprint.maximumY += 100;
    StructurePlanSnapshot highPriority = lowPriority;
    highPriority.key.cellX = 11;
    highPriority.projectionPriority = 20;
    StructurePlanSnapshot farPlan = lowPriority;
    farPlan.key.cellX = 12;
    farPlan.footprint.minimumX += 100;
    farPlan.footprint.maximumX += 100;
    const auto priorityResolved =
        DeterministicStructurePlanner::resolveOverlaps(
            {lowPriority, farPlan, highPriority});
    check("N9A/higher-priority-horizontal-overlap-wins",
          priorityResolved.size() == 2 &&
              std::any_of(priorityResolved.begin(), priorityResolved.end(),
                          [&highPriority](const StructurePlanSnapshot &plan) {
                              return plan.key == highPriority.key;
                          }) &&
              std::any_of(priorityResolved.begin(), priorityResolved.end(),
                          [&farPlan](const StructurePlanSnapshot &plan) {
                              return plan.key == farPlan.key;
                          }));

    StructurePlanSnapshot stableFirst = frozen;
    stableFirst.key.cellX = 2;
    stableFirst.key.cellZ = 3;
    StructurePlanSnapshot stableSecond = stableFirst;
    stableSecond.key.cellX = 3;
    const auto stableResolved =
        DeterministicStructurePlanner::resolveOverlaps(
            {stableSecond, stableFirst});
    check("N9A/equal-priority-overlap-has-stable-key-tiebreak",
          stableResolved.size() == 1 &&
              stableResolved.front().key == stableFirst.key);

    StructurePlanSnapshot threadedLeft;
    StructurePlanSnapshot threadedRight;
    std::thread leftThread([&currentGenerator, &threadedLeft]() {
        threadedLeft = currentGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    });
    std::thread rightThread([&otherSeedGenerator, &threadedRight]() {
        threadedRight = otherSeedGenerator.getStructurePlanForCell(
            StructureType::Waystone, 0, 5);
    });
    leftThread.join();
    rightThread.join();
    check("N9A/thread-scheduling-does-not-change-plan",
          sameStructurePlan(frozen, threadedLeft) &&
              sameStructurePlan(otherSeed, threadedRight));

    StructurePlanSnapshot crossing;
    for (int cellX = -16; cellX <= 16 && !crossing.valid; ++cellX) {
        for (int cellZ = -16; cellZ <= 16 && !crossing.valid; ++cellZ) {
            const StructurePlanSnapshot candidate =
                currentGenerator.getStructurePlanForCell(
                    StructureType::Waystone, cellX, cellZ);
            const int localX = World::floorMod(candidate.anchor.x, CHUNK_SIZE);
            const int localZ = World::floorMod(candidate.anchor.z, CHUNK_SIZE);
            const bool crossesBoundary =
                localX < DeterministicStructurePlanner::WaystoneRadius ||
                localX + DeterministicStructurePlanner::WaystoneRadius >=
                    CHUNK_SIZE ||
                localZ < DeterministicStructurePlanner::WaystoneRadius ||
                localZ + DeterministicStructurePlanner::WaystoneRadius >=
                    CHUNK_SIZE;
            if (candidate.valid && crossesBoundary) {
                crossing = candidate;
            }
        }
    }
    bool crossingDiscoverable = crossing.valid;
    int crossingChunkCount = 0;
    if (crossing.valid) {
        const int minimumChunkX = World::floorDiv(
            crossing.footprint.minimumX, CHUNK_SIZE);
        const int maximumChunkX = World::floorDiv(
            crossing.footprint.maximumX, CHUNK_SIZE);
        const int minimumChunkZ = World::floorDiv(
            crossing.footprint.minimumZ, CHUNK_SIZE);
        const int maximumChunkZ = World::floorDiv(
            crossing.footprint.maximumZ, CHUNK_SIZE);
        for (int chunkX = minimumChunkX; chunkX <= maximumChunkX; ++chunkX) {
            for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ;
                 ++chunkZ) {
                ++crossingChunkCount;
                const auto plans = currentGenerator.getStructurePlansForChunk(
                    chunkX, chunkZ);
                crossingDiscoverable = crossingDiscoverable &&
                    std::any_of(plans.begin(), plans.end(),
                                [&crossing](const StructurePlanSnapshot &plan) {
                                    return plan.key == crossing.key;
                                });
            }
        }
    }
    check("N9A/cross-chunk-plan-is-discoverable-and-bounded",
          crossingDiscoverable && crossingChunkCount >= 2 &&
              crossingChunkCount <= 4,
          "chunks=" + std::to_string(crossingChunkCount));

    ClassicOverWorldGenerator::LandmarkPlacement crossingLandmark{
        crossing.valid, crossing.anchor.x, crossing.anchor.y,
        crossing.anchor.z};
    const auto forward = sampleLandmarkChunks(
        world, currentGenerator, crossingLandmark, false);
    const auto reverse = sampleLandmarkChunks(
        world, currentGenerator, crossingLandmark, true);
    bool sameProjection = crossing.valid && forward.size() == reverse.size();
    int coreCount = 0;
    for (std::size_t index = 0;
         index < forward.size() && index < reverse.size(); ++index) {
        sameProjection = sameProjection &&
            forward[index].x == reverse[index].x &&
            forward[index].z == reverse[index].z &&
            forward[index].blocks == reverse[index].blocks;
        coreCount += static_cast<int>(std::count(
            forward[index].blocks.begin(), forward[index].blocks.end(),
            static_cast<Block_t>(BlockId::WaystoneCore)));
    }
    check("N9A/cross-chunk-projection-ignores-load-order",
          sameProjection && forward.size() >= 2 && coreCount == 1,
          "chunks=" + std::to_string(forward.size()) +
              ", cores=" + std::to_string(coreCount));
}

// ---------------------------------------------------------------------------
// N9B - terrain-v3 ruins, raider camps and persistent structure loot
// ---------------------------------------------------------------------------
struct GeneratedStructureChunkSample {
    int x = 0;
    int z = 0;
    std::vector<Block_t> blocks;
    std::vector<BlockEntityRecord> blockEntities;
};

bool sameBlockEntityRecord(const BlockEntityRecord &left,
                           const BlockEntityRecord &right)
{
    return left.position == right.position && left.type == right.type &&
           left.payload == right.payload;
}

bool sameGeneratedStructureSamples(
    const std::vector<GeneratedStructureChunkSample> &left,
    const std::vector<GeneratedStructureChunkSample> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].x != right[index].x ||
            left[index].z != right[index].z ||
            left[index].blocks != right[index].blocks ||
            left[index].blockEntities.size() !=
                right[index].blockEntities.size()) {
            return false;
        }
        for (std::size_t entity = 0;
             entity < left[index].blockEntities.size(); ++entity) {
            if (!sameBlockEntityRecord(
                    left[index].blockEntities[entity],
                    right[index].blockEntities[entity])) {
                return false;
            }
        }
    }
    return true;
}

std::vector<GeneratedStructureChunkSample> sampleStructureChunks(
    World &world, ClassicOverWorldGenerator &generator,
    const StructurePlanSnapshot &plan, bool reverseOrder)
{
    std::vector<glm::ivec2> positions;
    const int minimumChunkX = World::floorDiv(
        plan.footprint.minimumX, CHUNK_SIZE);
    const int maximumChunkX = World::floorDiv(
        plan.footprint.maximumX, CHUNK_SIZE);
    const int minimumChunkZ = World::floorDiv(
        plan.footprint.minimumZ, CHUNK_SIZE);
    const int maximumChunkZ = World::floorDiv(
        plan.footprint.maximumZ, CHUNK_SIZE);
    for (int chunkX = minimumChunkX; chunkX <= maximumChunkX; ++chunkX) {
        for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ;
             ++chunkZ) {
            positions.push_back({chunkX, chunkZ});
        }
    }
    if (reverseOrder) {
        std::reverse(positions.begin(), positions.end());
    }

    std::vector<GeneratedStructureChunkSample> samples;
    for (const glm::ivec2 &position : positions) {
        Chunk chunk(world, position);
        chunk.load(generator);
        GeneratedStructureChunkSample sample;
        sample.x = position.x;
        sample.z = position.y;
        std::vector<BlockMetadata_t> metadata;
        chunk.collectBlockData(sample.blocks, metadata);
        sample.blockEntities = chunk.getBlockEntities();
        samples.push_back(std::move(sample));
    }
    std::sort(samples.begin(), samples.end(),
              [](const GeneratedStructureChunkSample &left,
                 const GeneratedStructureChunkSample &right) {
                  return left.x != right.x ? left.x < right.x
                                           : left.z < right.z;
              });
    return samples;
}

StructurePlanSnapshot findStructurePlan(
    const ClassicOverWorldGenerator &generator, StructureType type,
    bool requireCrossChunk)
{
    for (int radius = 0; radius <= 32; ++radius) {
        for (int cellX = -radius; cellX <= radius; ++cellX) {
            for (int cellZ = -radius; cellZ <= radius; ++cellZ) {
                if (radius > 0 && std::abs(cellX) != radius &&
                    std::abs(cellZ) != radius) {
                    continue;
                }
                const StructurePlanSnapshot plan =
                    generator.getStructurePlanForCell(type, cellX, cellZ);
                if (!plan.valid) {
                    continue;
                }
                const bool crosses =
                    World::floorDiv(plan.footprint.minimumX, CHUNK_SIZE) !=
                        World::floorDiv(plan.footprint.maximumX,
                                        CHUNK_SIZE) ||
                    World::floorDiv(plan.footprint.minimumZ, CHUNK_SIZE) !=
                        World::floorDiv(plan.footprint.maximumZ,
                                        CHUNK_SIZE);
                if (!requireCrossChunk || crosses) {
                    return plan;
                }
            }
        }
    }
    return {};
}

bool inventoryMatchesLoot(const ContainerInventory &inventory,
                          const StructureLootSnapshot &loot)
{
    int nonEmptySlots = 0;
    for (int slot = 0; slot < inventory.getSlotCount(); ++slot) {
        if (inventory.getSlot(slot).amount > 0) {
            ++nonEmptySlots;
        }
    }
    if (!loot.valid || nonEmptySlots !=
            static_cast<int>(loot.entries.size())) {
        return false;
    }
    return std::all_of(
        loot.entries.begin(), loot.entries.end(),
        [&inventory](const StructureLootEntry &entry) {
            return inventory.count(entry.materialId) == entry.amount;
        });
}

bool generatedChestMatchesLoot(
    const std::vector<GeneratedStructureChunkSample> &samples,
    const StructurePlanSnapshot &plan,
    const StructureLootSnapshot &loot)
{
    int matchingChestBlocks = 0;
    int matchingBlockEntities = 0;
    bool payloadMatches = false;
    for (const GeneratedStructureChunkSample &sample : samples) {
        const int chunkMinimumX = sample.x * CHUNK_SIZE;
        const int chunkMinimumZ = sample.z * CHUNK_SIZE;
        const int localX = plan.chestPosition.x - chunkMinimumX;
        const int localZ = plan.chestPosition.z - chunkMinimumZ;
        if (localX >= 0 && localX < CHUNK_SIZE &&
            localZ >= 0 && localZ < CHUNK_SIZE) {
            const std::size_t section = static_cast<std::size_t>(
                plan.chestPosition.y / CHUNK_SIZE);
            const std::size_t sectionOffset = static_cast<std::size_t>(
                plan.chestPosition.y % CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE;
            const std::size_t localOffset = static_cast<std::size_t>(
                localZ) * CHUNK_SIZE + static_cast<std::size_t>(localX);
            const std::size_t blockIndex =
                section * CHUNK_VOLUME + sectionOffset + localOffset;
            if (blockIndex < sample.blocks.size() &&
                static_cast<BlockId>(sample.blocks[blockIndex]) ==
                    BlockId::Chest) {
                ++matchingChestBlocks;
            }
        }
        for (const BlockEntityRecord &record : sample.blockEntities) {
            const glm::ivec3 worldPosition{
                chunkMinimumX + record.position.x,
                record.position.y,
                chunkMinimumZ + record.position.z};
            if (worldPosition != plan.chestPosition ||
                record.type != ChestContainer::BlockEntityType) {
                continue;
            }
            ++matchingBlockEntities;
            ContainerInventory inventory(ChestContainer::SlotCount);
            payloadMatches =
                ContainerInventory::deserialize(record.payload, inventory) &&
                inventoryMatchesLoot(inventory, loot);
        }
    }
    return matchingChestBlocks == 1 && matchingBlockEntities == 1 &&
           payloadMatches;
}

void loadStructureFootprint(World &world,
                            const StructurePlanSnapshot &plan)
{
    const int minimumChunkX = World::floorDiv(
        plan.footprint.minimumX, CHUNK_SIZE);
    const int maximumChunkX = World::floorDiv(
        plan.footprint.maximumX, CHUNK_SIZE);
    const int minimumChunkZ = World::floorDiv(
        plan.footprint.minimumZ, CHUNK_SIZE);
    const int maximumChunkZ = World::floorDiv(
        plan.footprint.maximumZ, CHUNK_SIZE);
    for (int chunkX = minimumChunkX; chunkX <= maximumChunkX; ++chunkX) {
        for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ;
             ++chunkZ) {
            world.getChunkManager().loadChunk(chunkX, chunkZ);
        }
    }
}

void caseExplorationStructuresAndLoot()
{
    check("N9B/terrain-v3-and-structure-type-contract",
          WaystoneTerrainGenerationVersion == 2 &&
              ExplorationSiteTerrainGenerationVersion == 3 &&
              std::string(structureTypeName(StructureType::Ruin)) ==
                  "ruin" &&
              std::string(structureTypeName(
                  StructureType::RaiderCamp)) == "raider_camp" &&
              DeterministicStructurePlanner::SiteCellChunks == 4 &&
              DeterministicStructurePlanner::SiteCandidateCount == 8 &&
              DeterministicStructurePlanner::MaximumPlansPerChunk == 4);

    ClassicOverWorldGenerator currentGenerator(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion);
    ClassicOverWorldGenerator repeatedGenerator(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion);
    ClassicOverWorldGenerator otherSeedGenerator(
        kValidationSeed + 1,
        ExplorationSiteTerrainGenerationVersion);
    ClassicOverWorldGenerator v2Generator(
        kValidationSeed, WaystoneTerrainGenerationVersion);
    ClassicOverWorldGenerator legacyGenerator(
        kValidationSeed, LegacyTerrainGenerationVersion);
    const DeterministicStructurePlanner selector(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion,
        [&currentGenerator](int x, int z) {
            return currentGenerator.getSurfaceHeightAtWorld(x, z);
        },
        [&currentGenerator](int x, int z) {
            return currentGenerator.getBiomeAtWorld(x, z);
        });

    std::array<bool, 3> selectedTypes{};
    bool oneTypePerCell = true;
    for (int cellX = -8; cellX <= 8; ++cellX) {
        for (int cellZ = -8; cellZ <= 8; ++cellZ) {
            const StructureType selectedType =
                selector.selectedStructureTypeForCell(cellX, cellZ);
            selectedTypes[static_cast<std::size_t>(selectedType)] = true;
            int validPlans = 0;
            for (StructureType type : {
                     StructureType::Waystone, StructureType::Ruin,
                     StructureType::RaiderCamp}) {
                const StructurePlanSnapshot plan =
                    currentGenerator.getStructurePlanForCell(
                        type, cellX, cellZ);
                validPlans += plan.valid ? 1 : 0;
                if (type != selectedType && plan.valid) {
                    oneTypePerCell = false;
                }
            }
            oneTypePerCell = oneTypePerCell && validPlans <= 1;
        }
    }
    check("N9B/cell-selector-covers-three-types-and-keeps-one-plan",
          std::all_of(selectedTypes.begin(), selectedTypes.end(),
                      [](bool selected) { return selected; }) &&
              oneTypePerCell);

    const StructurePlanSnapshot ruin = findStructurePlan(
        currentGenerator, StructureType::Ruin, true);
    const StructurePlanSnapshot camp = findStructurePlan(
        currentGenerator, StructureType::RaiderCamp, true);
    const auto flatSurface = [](int, int) { return 70; };
    const DeterministicStructurePlanner negativeRuinPlanner(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion,
        flatSurface,
        [](int, int) { return TerrainBiome::LightForest; });
    const DeterministicStructurePlanner negativeCampPlanner(
        kValidationSeed, ExplorationSiteTerrainGenerationVersion,
        flatSurface,
        [](int, int) { return TerrainBiome::Desert; });
    StructurePlanSnapshot negativeRuin;
    StructurePlanSnapshot negativeCamp;
    for (int cellX = -1; cellX >= -8; --cellX) {
        for (int cellZ = -1; cellZ >= -8; --cellZ) {
            if (!negativeRuin.valid) {
                negativeRuin = negativeRuinPlanner.planForCell(
                    StructureType::Ruin, cellX, cellZ);
            }
            if (!negativeCamp.valid) {
                negativeCamp = negativeCampPlanner.planForCell(
                    StructureType::RaiderCamp, cellX, cellZ);
            }
        }
    }
    check("N9B/ruin-and-camp-are-discoverable-in-eligible-biomes",
          ruin.valid && camp.valid &&
              (ruin.biome == TerrainBiome::LightForest ||
               ruin.biome == TerrainBiome::TemperateForest) &&
              (camp.biome == TerrainBiome::Desert ||
               camp.biome == TerrainBiome::Grassland),
          "ruin cell=" + std::to_string(ruin.key.cellX) + "," +
              std::to_string(ruin.key.cellZ) + " anchor=" +
              std::to_string(ruin.anchor.x) + "," +
              std::to_string(ruin.anchor.y) + "," +
              std::to_string(ruin.anchor.z) + "; camp cell=" +
              std::to_string(camp.key.cellX) + "," +
              std::to_string(camp.key.cellZ) + " anchor=" +
              std::to_string(camp.anchor.x) + "," +
              std::to_string(camp.anchor.y) + "," +
              std::to_string(camp.anchor.z));

    check("N9B/site-footprints-priorities-and-write-budgets-are-frozen",
          ruin.footprint.width() == 9 &&
              ruin.footprint.height() == 9 &&
              ruin.footprint.depth() == 9 && ruin.hasChest &&
              ruin.projectionPriority == 80 &&
              ruin.plannedBlockCount == 790 &&
              camp.footprint.width() == 11 &&
              camp.footprint.height() == 8 &&
              camp.footprint.depth() == 9 && camp.hasChest &&
              camp.projectionPriority == 60 &&
              camp.plannedBlockCount == 905);

    const int siteCellSize =
        DeterministicStructurePlanner::SiteCellChunks * CHUNK_SIZE;
    const auto insideOwnedCell = [siteCellSize](
        const StructurePlanSnapshot &plan) {
        const int minimumX = plan.key.cellX * siteCellSize +
            DeterministicStructurePlanner::SiteEdgeInset;
        const int minimumZ = plan.key.cellZ * siteCellSize +
            DeterministicStructurePlanner::SiteEdgeInset;
        const int maximumExclusiveX =
            (plan.key.cellX + 1) * siteCellSize -
            DeterministicStructurePlanner::SiteEdgeInset;
        const int maximumExclusiveZ =
            (plan.key.cellZ + 1) * siteCellSize -
            DeterministicStructurePlanner::SiteEdgeInset;
        return plan.anchor.x >= minimumX &&
               plan.anchor.x < maximumExclusiveX &&
               plan.anchor.z >= minimumZ &&
               plan.anchor.z < maximumExclusiveZ;
    };
    check("N9B/site-ownership-is-bounded",
          insideOwnedCell(ruin) && insideOwnedCell(camp) &&
              negativeRuin.valid && insideOwnedCell(negativeRuin) &&
              negativeCamp.valid && insideOwnedCell(negativeCamp),
          "negative ruin=" + std::to_string(negativeRuin.key.cellX) +
              "," + std::to_string(negativeRuin.key.cellZ) +
              " camp=" + std::to_string(negativeCamp.key.cellX) +
              "," + std::to_string(negativeCamp.key.cellZ));

    const StructurePlanSnapshot repeatedRuin =
        repeatedGenerator.getStructurePlanForCell(
            StructureType::Ruin, ruin.key.cellX, ruin.key.cellZ);
    const StructurePlanSnapshot repeatedCamp =
        repeatedGenerator.getStructurePlanForCell(
            StructureType::RaiderCamp, camp.key.cellX, camp.key.cellZ);
    const StructurePlanSnapshot otherSeedRuin =
        otherSeedGenerator.getStructurePlanForCell(
            StructureType::Ruin, ruin.key.cellX, ruin.key.cellZ);
    check("N9B/site-plans-are-seed-stable-and-seed-sensitive",
          sameStructurePlan(ruin, repeatedRuin) &&
              sameStructurePlan(camp, repeatedCamp) &&
              (!sameStructurePlan(ruin, otherSeedRuin) ||
               currentGenerator.getStructurePlanForCell(
                   StructureType::RaiderCamp,
                   camp.key.cellX, camp.key.cellZ).selectionHash !=
                   otherSeedGenerator.getStructurePlanForCell(
                       StructureType::RaiderCamp,
                       camp.key.cellX, camp.key.cellZ).selectionHash));

    check("N9B/terrain-v1-v2-never-plan-new-sites",
          !v2Generator.getStructurePlanForCell(
               StructureType::Ruin, ruin.key.cellX,
               ruin.key.cellZ).valid &&
              !v2Generator.getStructurePlanForCell(
                  StructureType::RaiderCamp, camp.key.cellX,
                  camp.key.cellZ).valid &&
              !legacyGenerator.getStructurePlanForCell(
                  StructureType::Ruin, ruin.key.cellX,
                  ruin.key.cellZ).valid &&
              !legacyGenerator.getStructurePlanForCell(
                  StructureType::RaiderCamp, camp.key.cellX,
                  camp.key.cellZ).valid &&
              v2Generator.getStructurePlanForCell(
                  StructureType::Waystone, 0, 5).anchor ==
                  glm::ivec3(18, 70, 328));

    bool queryBounded = true;
    for (const StructurePlanSnapshot *site : {&ruin, &camp}) {
        const int minimumChunkX = World::floorDiv(
            site->footprint.minimumX, CHUNK_SIZE);
        const int maximumChunkX = World::floorDiv(
            site->footprint.maximumX, CHUNK_SIZE);
        const int minimumChunkZ = World::floorDiv(
            site->footprint.minimumZ, CHUNK_SIZE);
        const int maximumChunkZ = World::floorDiv(
            site->footprint.maximumZ, CHUNK_SIZE);
        for (int chunkX = minimumChunkX; chunkX <= maximumChunkX;
             ++chunkX) {
            for (int chunkZ = minimumChunkZ; chunkZ <= maximumChunkZ;
                 ++chunkZ) {
                const auto plans =
                    currentGenerator.getStructurePlansForChunk(
                        chunkX, chunkZ);
                queryBounded = queryBounded &&
                    plans.size() <=
                        DeterministicStructurePlanner::MaximumPlansPerChunk &&
                    std::count_if(
                        plans.begin(), plans.end(),
                        [site](const StructurePlanSnapshot &plan) {
                            return plan.key == site->key;
                        }) == 1;
            }
        }
    }
    check("N9B/cross-chunk-queries-are-complete-and-capped",
          queryBounded);

    StructurePlanSnapshot threadedRuin;
    StructurePlanSnapshot threadedCamp;
    std::thread ruinThread([&currentGenerator, &ruin, &threadedRuin]() {
        threadedRuin = currentGenerator.getStructurePlanForCell(
            StructureType::Ruin, ruin.key.cellX, ruin.key.cellZ);
    });
    std::thread campThread([&currentGenerator, &camp, &threadedCamp]() {
        threadedCamp = currentGenerator.getStructurePlanForCell(
            StructureType::RaiderCamp, camp.key.cellX, camp.key.cellZ);
    });
    ruinThread.join();
    campThread.join();
    check("N9B/thread-scheduling-does-not-change-sites",
          sameStructurePlan(ruin, threadedRuin) &&
              sameStructurePlan(camp, threadedCamp));

    const StructureLootSnapshot ruinLoot = structureLootForPlan(
        ruin, ExplorationRewards::CurrentVersion);
    const StructureLootSnapshot repeatedRuinLoot =
        structureLootForPlan(repeatedRuin,
                             ExplorationRewards::CurrentVersion);
    const StructureLootSnapshot campLoot = structureLootForPlan(
        camp, ExplorationRewards::CurrentVersion);
    const auto validLoot = [](const StructureLootSnapshot &loot,
                              StructureType expectedType) {
        if (!loot.valid || loot.key.type != expectedType ||
            loot.entries.size() != 3 || loot.selectionHash == 0) {
            return false;
        }
        std::set<Material::ID> materials;
        for (const StructureLootEntry &entry : loot.entries) {
            const Material &material = Material::toMaterial(
                entry.materialId);
            if (entry.materialId == Material::ID::Nothing ||
                entry.amount <= 0 || entry.amount > material.maxStackSize ||
                material.isTool || !materials.insert(entry.materialId).second) {
                return false;
            }
        }
        return true;
    };
    check("N9B/type-specific-loot-snapshots-are-valid-and-stable",
          validLoot(ruinLoot, StructureType::Ruin) &&
              validLoot(campLoot, StructureType::RaiderCamp) &&
              ruinLoot.entries == repeatedRuinLoot.entries &&
              ruinLoot.selectionHash == repeatedRuinLoot.selectionHash &&
              ruinLoot.chestPosition == ruin.chestPosition &&
              campLoot.chestPosition == camp.chestPosition &&
              ruinLoot.entries != campLoot.entries,
          "ruin=" + std::to_string(ruinLoot.entries[0].amount) + "," +
              std::to_string(ruinLoot.entries[1].amount) + "," +
              std::to_string(ruinLoot.entries[2].amount) +
              "; camp=" + std::to_string(campLoot.entries[0].amount) +
              "," + std::to_string(campLoot.entries[1].amount) + "," +
              std::to_string(campLoot.entries[2].amount));
    check("N9B/loot-ranges-and-non-site-rejection-are-frozen",
          ruinLoot.entries[0].materialId ==
                  Material::ID::AncientCompass &&
              ruinLoot.entries[0].amount == 1 &&
              ruinLoot.entries[1].materialId == Material::ID::Glass &&
              ruinLoot.entries[1].amount >= 2 &&
              ruinLoot.entries[1].amount <= 5 &&
              ruinLoot.entries[2].materialId == Material::ID::WheatSeeds &&
              ruinLoot.entries[2].amount >= 2 &&
              ruinLoot.entries[2].amount <= 6 &&
              campLoot.entries[0].materialId ==
                  Material::ID::RaiderWard &&
              campLoot.entries[0].amount == 1 &&
              campLoot.entries[1].materialId == Material::ID::CoalOre &&
              campLoot.entries[1].amount >= 4 &&
              campLoot.entries[1].amount <= 8 &&
              campLoot.entries[2].materialId == Material::ID::IronOre &&
              campLoot.entries[2].amount >= 1 &&
              campLoot.entries[2].amount <= 4 &&
              !structureLootForPlan(
                  v2Generator.getStructurePlanForCell(
                      StructureType::Waystone, 0, 5),
                  ExplorationRewards::CurrentVersion).valid);
    check("N9B/fixed-seed-structure-and-loot-snapshot",
          ruin.key.cellX == 3 && ruin.key.cellZ == 6 &&
              ruin.anchor == glm::ivec3(222, 70, 400) &&
              ruin.chestPosition == glm::ivec3(222, 72, 400) &&
              ruinLoot.entries[0].amount == 1 &&
              ruinLoot.entries[1].amount == 4 &&
              ruinLoot.entries[2].amount == 4 &&
              camp.key.cellX == 6 && camp.key.cellZ == 0 &&
              camp.anchor == glm::ivec3(436, 102, 37) &&
              camp.chestPosition == glm::ivec3(436, 104, 38) &&
              campLoot.entries[0].amount == 1 &&
              campLoot.entries[1].amount == 6 &&
              campLoot.entries[2].amount == 2);

    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");
    Config config = makeConfig();
    Camera camera(config);
    Player samplePlayer;
    World sampleWorld(camera, config, samplePlayer,
                      freshSaveDirectory("n9b_projection"), false, 0);
    const auto ruinForward = sampleStructureChunks(
        sampleWorld, currentGenerator, ruin, false);
    const auto ruinReverse = sampleStructureChunks(
        sampleWorld, repeatedGenerator, ruin, true);
    const auto campForward = sampleStructureChunks(
        sampleWorld, currentGenerator, camp, false);
    const auto campReverse = sampleStructureChunks(
        sampleWorld, repeatedGenerator, camp, true);
    check("N9B/ruin-and-camp-projection-ignore-load-order",
          sameGeneratedStructureSamples(ruinForward, ruinReverse) &&
              sameGeneratedStructureSamples(campForward, campReverse));
    check("N9B/each-site-projects-one-chest-with-exact-loot",
          generatedChestMatchesLoot(ruinForward, ruin, ruinLoot) &&
              generatedChestMatchesLoot(campForward, camp, campLoot));

    const auto v2RuinArea = sampleStructureChunks(
        sampleWorld, v2Generator, ruin, false);
    const auto v2CampArea = sampleStructureChunks(
        sampleWorld, v2Generator, camp, false);
    auto containsNewSiteChest = [](const auto &samples,
                                   const glm::ivec3 &position) {
        for (const auto &sample : samples) {
            for (const BlockEntityRecord &record : sample.blockEntities) {
                const glm::ivec3 worldPosition{
                    sample.x * CHUNK_SIZE + record.position.x,
                    record.position.y,
                    sample.z * CHUNK_SIZE + record.position.z};
                if (worldPosition == position &&
                    record.type == ChestContainer::BlockEntityType) {
                    return true;
                }
            }
        }
        return false;
    };
    check("N9B/terrain-v2-output-does-not-backfill-site-chests",
          !containsNewSiteChest(v2RuinArea, ruin.chestPosition) &&
              !containsNewSiteChest(v2CampArea, camp.chestPosition));

    const auto persistenceDirectory =
        freshSaveDirectory("n9b_loot_persistence");
    const bool persistenceIdentityPrepared = initializeTerrainIdentity(
        persistenceDirectory, "n9b-loot-terrain-v3",
        ExplorationSiteTerrainGenerationVersion);
    const glm::ivec3 damagedBlock{
        ruin.footprint.minimumX, ruin.anchor.y + 2,
        ruin.footprint.minimumZ};
    bool emptiedAndSaved = false;
    {
        setEnv("HELLOMINE3D_PLAYER_POSITION",
               std::to_string(ruin.chestPosition.x) + " " +
                   std::to_string(ruin.chestPosition.y + 2) + " " +
                   std::to_string(ruin.chestPosition.z));
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 0);
        loadStructureFootprint(world, ruin);
        const auto record = world.getBlockEntity(ruin.chestPosition);
        ContainerInventory initial(ChestContainer::SlotCount);
        const bool initialMatches = record &&
            record->type == ChestContainer::BlockEntityType &&
            ContainerInventory::deserialize(record->payload, initial) &&
            inventoryMatchesLoot(initial, ruinLoot) &&
            ChestContainer::open(world, player, ruin.chestPosition);
        bool transferred = initialMatches;
        for (int slot = 0;
             slot < ChestContainer::SlotCount && transferred; ++slot) {
            const auto view = ChestContainer::view(world, player);
            if (!view) {
                transferred = false;
                break;
            }
            const InventorySlotState entry = view->inventory.getSlot(slot);
            if (entry.amount > 0) {
                transferred = ChestContainer::transferToPlayer(
                    world, player, slot, entry.amount);
            }
        }
        const auto emptied = ChestContainer::view(world, player);
        bool empty = emptied.has_value();
        for (int slot = 0;
             slot < ChestContainer::SlotCount && empty; ++slot) {
            empty = emptied->inventory.getSlot(slot).amount == 0;
        }
        ChestContainer::close(player);
        world.setBlock(damagedBlock.x, damagedBlock.y, damagedBlock.z,
                       BlockId::Air);
        emptiedAndSaved = persistenceIdentityPrepared && transferred &&
            empty && world.save();
    }

    bool emptyAndDamageRestored = false;
    bool depositSaved = false;
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 0);
        loadStructureFootprint(world, ruin);
        const auto record = world.getBlockEntity(ruin.chestPosition);
        ContainerInventory inventory(ChestContainer::SlotCount);
        bool empty = record &&
            ContainerInventory::deserialize(record->payload, inventory);
        for (int slot = 0;
             slot < ChestContainer::SlotCount && empty; ++slot) {
            empty = inventory.getSlot(slot).amount == 0;
        }
        emptyAndDamageRestored = empty &&
            static_cast<BlockId>(world.getBlock(
                damagedBlock.x, damagedBlock.y, damagedBlock.z).id) ==
                BlockId::Air;

        const int added = player.addItem(Material::DIRT_BLOCK, 7);
        int dirtSlot = -1;
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot) {
            if (player.getInventorySlot(slot).getMaterial().id ==
                Material::ID::Dirt) {
                dirtSlot = slot;
                break;
            }
        }
        const bool opened = ChestContainer::open(
            world, player, ruin.chestPosition);
        const bool deposited = opened && dirtSlot >= 0 &&
            ChestContainer::transferFromPlayer(
                world, player, dirtSlot, 7);
        ChestContainer::close(player);
        depositSaved = added == 7 && deposited && world.save();
    }

    bool depositRestored = false;
    {
        Player player;
        World world(camera, config, player, persistenceDirectory, false, 0);
        loadStructureFootprint(world, ruin);
        const auto record = world.getBlockEntity(ruin.chestPosition);
        ContainerInventory inventory(ChestContainer::SlotCount);
        depositRestored = record &&
            ContainerInventory::deserialize(record->payload, inventory) &&
            inventory.count(Material::ID::Dirt) == 7 &&
            std::all_of(
                ruinLoot.entries.begin(), ruinLoot.entries.end(),
                [&inventory](const StructureLootEntry &entry) {
                    return inventory.count(entry.materialId) == 0;
                }) &&
            static_cast<BlockId>(world.getBlock(
                damagedBlock.x, damagedBlock.y, damagedBlock.z).id) ==
                BlockId::Air;
    }
    check("N9B/empty-loot-and-structure-damage-survive-reload",
          emptiedAndSaved && emptyAndDamageRestored);
    check("N9B/player-deposit-survives-without-loot-reinitialization",
          depositSaved && depositRestored);

    const auto v2IdentityDirectory =
        freshSaveDirectory("n9b_v2_identity");
    {
        Player player;
        World world(camera, config, player, v2IdentityDirectory, false, 0);
    }
    WorldSave v2Save(v2IdentityDirectory);
    WorldSaveData v2Data;
    bool v2IdentityPrepared = v2Save.load(v2Data);
    if (v2IdentityPrepared) {
        v2Data.terrainGenerationVersion = WaystoneTerrainGenerationVersion;
        v2IdentityPrepared = v2Save.save(v2Data);
    }
    bool v2WorldStayedStructureFree = false;
    if (v2IdentityPrepared) {
        Player player;
        World world(camera, config, player, v2IdentityDirectory, false, 0);
        loadStructureFootprint(world, ruin);
        v2WorldStayedStructureFree =
            world.collectDebugStats().terrainGenerationVersion ==
                WaystoneTerrainGenerationVersion &&
            static_cast<BlockId>(world.getBlock(
                ruin.chestPosition.x, ruin.chestPosition.y,
                ruin.chestPosition.z).id) != BlockId::Chest &&
            !world.getBlockEntity(ruin.chestPosition).has_value() &&
            world.save();
    }
    WorldSaveData preservedV2;
    check("N9B/existing-terrain-v2-world-never-upgrades-or-backfills",
          v2WorldStayedStructureFree && v2Save.load(preservedV2) &&
              preservedV2.terrainGenerationVersion ==
                  WaystoneTerrainGenerationVersion);
}

// ---------------------------------------------------------------------------
// S3.4 / S3.5 / S4.2 / S4.5 - interaction, drops and block/player events
// ---------------------------------------------------------------------------
void caseInteractionAndEvents()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("interaction");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    EventRecorder events(world.getEventBus());

    const int y = 100;
    player.addItem(Material::WOODEN_PICKAXE, 1);
    world.setBlock(8, y, 8, BlockId::Stone);

    const glm::vec3 target(8.5f, static_cast<float>(y) + 0.5f, 8.5f);
    const bool broke = BlockInteractionSystem::breakBlock(world, player, target);
    check("S3.4/break-through-interaction-system", broke);
    check("S3.4/break-clears-block", world.getBlock(8, y, 8).id == 0);
    check("S3.5/break-adds-configured-drop",
          player.getSaveState().inventory.size() >= 2 &&
              player.getInventorySlot(1).getMaterial().id ==
                  Material::ID::Cobblestone &&
              player.getInventorySlot(1).getNumInStack() == 1,
          "stone=" + std::to_string(
              player.getInventorySlot(1).getNumInStack()));
    check("S4.2/break-publishes-events",
          events.count(SandboxEventType::BlockBreak) == 1 &&
              events.count(SandboxEventType::BlockChanged) == 1,
          "break=" + std::to_string(events.count(SandboxEventType::BlockBreak)) +
              " changed=" +
              std::to_string(events.count(SandboxEventType::BlockChanged)));
    check("S4.5/break-publishes-inventory-event",
          events.count(SandboxEventType::PlayerInventoryChanged) == 2);

    events.reset();
    PlayerSaveState placementState = player.getSaveState();
    placementState.heldItem = 1;
    player.applySaveState(placementState);
    const bool placed =
        BlockInteractionSystem::placeBlock(world, player, target);
    check("S3.4/place-through-interaction-system", placed);
    check("S3.4/place-sets-block",
          world.getBlock(8, y, 8).id ==
              static_cast<Block_t>(BlockId::Cobblestone));
    check("S3.4/place-consumes-item",
          player.getHeldItems().getNumInStack() == 0,
          "held x" + std::to_string(player.getHeldItems().getNumInStack()));
    check("S4.2/place-publishes-events",
          events.count(SandboxEventType::BlockPlace) == 1 &&
              events.count(SandboxEventType::BlockChanged) == 1);

    glm::ivec3 usedPosition{0};
    BlockId usedBlock = BlockId::Air;
    const auto useSubscription = world.getEventBus().subscribe(
        SandboxEventType::BlockUse,
        [&](const SandboxEvent &event) {
            const auto &useEvent = static_cast<const BlockUseEvent &>(event);
            usedPosition = useEvent.position;
            usedBlock = useEvent.blockId;
        });

    world.setBlock(8, y, 8, BlockId::Workbench);
    events.reset();
    const bool used =
        BlockInteractionSystem::useBlock(world, player, target);
    check("P5/use-through-interaction-system", used);
    check("P5/use-publishes-event",
          events.count(SandboxEventType::BlockUse) == 1);
    check("P5/use-event-identifies-target",
          usedPosition == glm::ivec3(8, y, 8) &&
              usedBlock == BlockId::Workbench);
    player.closeCrafting();

    world.setBlock(8, y, 8, BlockId::Stone);
    events.reset();
    const bool usedPlainBlock =
        BlockInteractionSystem::useBlock(world, player, target);
    check("P11A/plain-block-does-not-consume-use",
          !usedPlainBlock &&
              events.count(SandboxEventType::BlockUse) == 0);

    events.reset();
    const bool usedAir = BlockInteractionSystem::useBlock(
        world, player,
        glm::vec3(8.5f, static_cast<float>(y) + 5.5f, 8.5f));
    check("P5/use-air-is-noop",
          !usedAir && events.count(SandboxEventType::BlockUse) == 0);
    world.getEventBus().unsubscribe(useSubscription);

    // Breaking air must be a no-op and must not publish events.
    events.reset();
    const bool brokeAir = BlockInteractionSystem::breakBlock(
        world, player, glm::vec3(8.5f, static_cast<float>(y) + 5.5f, 8.5f));
    check("S3.4/break-air-is-noop",
          !brokeAir && events.count(SandboxEventType::BlockBreak) == 0);

    // S3.3 - block metadata survives a set/get roundtrip.
    world.setBlock(10, y, 10, ChunkBlock(BlockId::Stone, 5));
    const auto metadataBlock = world.getBlock(10, y, 10);
    check("S3.3/block-metadata-roundtrip",
          metadataBlock.id == static_cast<Block_t>(BlockId::Stone) &&
              metadataBlock.metadata == 5,
          "id=" + std::to_string(static_cast<int>(metadataBlock.id)) +
              " metadata=" +
              std::to_string(static_cast<int>(metadataBlock.metadata)));

    // S3.1 / S3.2 - block definitions expose the data the mesh builder needs.
    const auto &stoneDefinition =
        BlockDatabase::get().getDefinition(BlockId::Stone);
    const auto &waterDefinition =
        BlockDatabase::get().getDefinition(BlockId::Water);
    check("S3.1/block-definition-populated",
          !stoneDefinition.stringId.empty() && stoneDefinition.solid &&
              stoneDefinition.defaultDrop == Material::ID::Stone,
          "stringId=" + stoneDefinition.stringId);
    check("S3.1/liquid-definition-flagged",
          waterDefinition.liquid && !waterDefinition.solid);
}

// ---------------------------------------------------------------------------
// S4.3 - chunk lifecycle events
// ---------------------------------------------------------------------------
void caseChunkEvents()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("chunk_events");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    EventRecorder events(world.getEventBus());

    // Load a region that is far from the preloaded spawn area.
    world.preloadAround({640.f, 90.f, 640.f});
    check("S4.3/chunk-load-events",
          events.count(SandboxEventType::ChunkLoaded) > 0 &&
              events.count(SandboxEventType::ChunkGenerated) > 0,
          "loaded=" + std::to_string(events.count(SandboxEventType::ChunkLoaded)) +
              " generated=" +
              std::to_string(events.count(SandboxEventType::ChunkGenerated)));

    events.reset();
    world.setBlock(640, 100, 640, BlockId::Stone);
    world.save();
    check("S4.3/chunk-save-event",
          events.count(SandboxEventType::ChunkSaved) > 0,
          "saved=" + std::to_string(events.count(SandboxEventType::ChunkSaved)));

    events.reset();
    world.getChunkManager().unloadChunk(40, 40);
    check("S4.3/chunk-unload-event",
          events.count(SandboxEventType::ChunkUnloaded) > 0);
}

// ---------------------------------------------------------------------------
// S5.1 - S5.6 - actors, mobs and item entities inside a real world
// ---------------------------------------------------------------------------
void caseActors()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("actors");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 1);

    EventRecorder events(world.getEventBus());

    const glm::vec3 mobPosition(8.5f, 100.f, 8.5f);
    const ActorId mobId = world.spawnMob("validation_mob", mobPosition);
    check("S5.6/mob-spawns", mobId != InvalidActorId);
    check("S5.1/actor-count-tracked",
          world.getActorManager().getActorCount() == 1,
          "count=" +
              std::to_string(world.getActorManager().getActorCount()));
    check("S4.4/entity-spawn-event",
          events.count(SandboxEventType::EntitySpawn) == 1);

    auto actorSnapshots = world.collectActorSnapshots();
    check("P1/mob-render-snapshot",
          actorSnapshots.size() == 1 &&
              actorSnapshots.front().id == mobId &&
              actorSnapshots.front().type == "validation_mob" &&
              actorSnapshots.front().dimensions ==
                  glm::vec3(0.35f, 0.9f, 0.35f));

    auto *mob = dynamic_cast<MobActor *>(
        world.getActorManager().findActor(mobId));
    if (mob == nullptr) {
        check("S5.2/mob-is-living-actor", false);
        return;
    }
    check("S5.2/mob-is-living-actor", true);

    player.position = mobPosition + glm::vec3(4.f, 0.f, 0.f);
    const float chaseDistanceBefore =
        glm::length(glm::vec2(player.position.x - mob->position.x,
                              player.position.z - mob->position.z));
    world.tick(0);
    const float chaseDistanceAfter =
        glm::length(glm::vec2(player.position.x - mob->position.x,
                              player.position.z - mob->position.z));
    check("C6/mob-chases-nearby-player",
          chaseDistanceAfter < chaseDistanceBefore,
          "distance " + std::to_string(chaseDistanceBefore) + " -> " +
              std::to_string(chaseDistanceAfter));

    player.position = mobPosition + glm::vec3(100.f, 0.f, 100.f);
    const glm::vec3 startPosition = mob->position;
    for (int i = 1; i <= 20; ++i) {
        world.tick(i);
    }
    const float moved = glm::length(mob->position - startPosition);
    check("S5.6/mob-wanders-on-tick", moved > 0.f,
          "moved " + std::to_string(moved));
    actorSnapshots = world.collectActorSnapshots();
    check("P1/mob-render-transform-updates",
          actorSnapshots.size() == 1 &&
              actorSnapshots.front().position == mob->position);

    events.reset();
    mob->damage(world, 5.f);
    check("S5.2/mob-takes-damage",
          mob->getHealth() < mob->getMaxHealth() && mob->isAlive(),
          "health=" + std::to_string(mob->getHealth()));
    check("S4.4/entity-damage-event",
          events.count(SandboxEventType::EntityDamage) == 1);
    const float healthAfterFirstHit = mob->getHealth();
    const bool repeatedDamageAccepted = mob->damage(world, 1.f);
    check("C6/damage-immunity-rejects-repeat",
          !repeatedDamageAccepted &&
              mob->getHealth() == healthAfterFirstHit &&
              mob->getDamageInvulnerabilityRemaining() > 0.f &&
              events.count(SandboxEventType::EntityDamage) == 1,
          "health=" + std::to_string(mob->getHealth()) +
              " events=" +
              std::to_string(events.count(SandboxEventType::EntityDamage)));

    events.reset();
    for (int i = 21; i <= 31; ++i) {
        world.tick(i);
    }
    const bool lethalDamageAccepted = mob->damage(world, 1000.f);
    check("C6/damage-immunity-expires", lethalDamageAccepted,
          "remaining=" + std::to_string(
              mob->getDamageInvulnerabilityRemaining()));
    check("S5.2/mob-dies", !mob->isAlive());
    check("S4.4/entity-death-event",
          lethalDamageAccepted &&
              events.count(SandboxEventType::EntityDeath) == 1);
    actorSnapshots = world.collectActorSnapshots();
    check("P1/dead-mob-is-not-rendered",
          std::none_of(actorSnapshots.begin(), actorSnapshots.end(),
                       [mobId](const ActorSnapshot &snapshot) {
                           return snapshot.id == mobId;
                       }));

    world.tick(100);
    check("S5.1/dead-actors-removed",
          world.getActorManager().findActor(mobId) == nullptr);

    // S5.5 / P11B - bounded attraction, lifetime and pickup feedback.
    events.reset();
    player.position = glm::vec3(8.5f, 100.f, 8.5f);
    const ActorId attractedItemId = world.spawnItemEntity(
        Material::ID::Dirt, 1, player.position + glm::vec3(2.5f, 0.f, 0.f));
    auto *attractedItem = dynamic_cast<ItemEntity *>(
        world.getActorManager().findActor(attractedItemId));
    if (attractedItem != nullptr)
    {
        attractedItem->setPickupDelay(0.f);
    }
    world.tick(190);
    attractedItem = dynamic_cast<ItemEntity *>(
        world.getActorManager().findActor(attractedItemId));
    check("P11B/item-attraction-is-bounded-and-points-toward-player",
          attractedItem != nullptr && attractedItem->velocity.x < 0.f &&
              glm::length(attractedItem->velocity) <=
                  ItemEntity::MaxPickupAttractionSpeed + 0.0001f);
    if (attractedItem != nullptr)
    {
        attractedItem->kill();
    }
    world.tick(191);

    const ActorId expiringItemId = world.spawnItemEntity(
        Material::ID::Dirt, 1, player.position + glm::vec3(4.f, 0.f, 0.f));
    auto *expiringItem = dynamic_cast<ItemEntity *>(
        world.getActorManager().findActor(expiringItemId));
    if (expiringItem != nullptr)
    {
        ActorSaveState state = expiringItem->getSaveState();
        state.wanderTime = ItemEntity::MaxLifetimeSeconds - 0.01f;
        expiringItem->applySaveState(state);
    }
    world.tick(192);
    check("P11B/item-lifetime-persists-and-expires-at-bounded-age",
          world.getActorManager().findActor(expiringItemId) == nullptr &&
              ItemEntity::MaxGroundBounces == 2 &&
              ItemEntity::PickupAttractionRadius == 3.f);

    events.reset();
    const ActorId itemId = world.spawnItemEntity(Material::ID::Stone, 3,
                                                 player.position);
    check("S5.5/item-entity-spawns", itemId != InvalidActorId);
    actorSnapshots = world.collectActorSnapshots();
    const auto itemSnapshot = std::find_if(
        actorSnapshots.begin(), actorSnapshots.end(),
        [itemId](const ActorSnapshot &snapshot) {
            return snapshot.id == itemId;
        });
    check("P1/item-render-snapshot",
          itemSnapshot != actorSnapshots.end() &&
              itemSnapshot->type == "item" &&
              itemSnapshot->dimensions == glm::vec3(0.25f));

    auto *item =
        dynamic_cast<ItemEntity *>(world.getActorManager().findActor(itemId));
    if (item == nullptr) {
        check("S5.5/item-entity-found", false);
        return;
    }
    check("S5.5/item-entity-found", true);
    item->setPickupDelay(0.f);

    const int heldBefore = player.getHeldItems().getNumInStack();
    world.tick(200);
    check("S5.5/item-entity-picked-up",
          player.getHeldItems().getNumInStack() == heldBefore + 3 &&
              world.getActorManager().findActor(itemId) == nullptr,
          "held " + std::to_string(heldBefore) + " -> " +
              std::to_string(player.getHeldItems().getNumInStack()));
    check("S4.4/item-pickup-event",
          events.count(SandboxEventType::ItemPickup) == 1);
    check("S4.5/pickup-publishes-inventory-event",
          events.count(SandboxEventType::PlayerInventoryChanged) == 1);
    actorSnapshots = world.collectActorSnapshots();
    check("P1/picked-item-is-not-rendered",
          std::none_of(actorSnapshots.begin(), actorSnapshots.end(),
                       [itemId](const ActorSnapshot &snapshot) {
                           return snapshot.id == itemId;
                       }));
}

// ---------------------------------------------------------------------------
// S1.2 / S1.4 / S1.5 - world manager boundary
// ---------------------------------------------------------------------------
void caseWorldManager()
{
    setEnv("HELLOMINE3D_SEED", std::to_string(kValidationSeed));
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 90 8");
    setEnv("HELLOMINE3D_PLAYER_ROTATION", "0 0 0");

    const auto directory = freshSaveDirectory("world_manager");
    setEnv("HELLOMINE3D_SAVE_DIR", directory);
    setEnv("HELLOMINE3D_WORLD_TIME", "18000");

    Config config = makeConfig();
    Camera camera(config);
    Player player;

    {
        WorldManager manager(config, camera, player);
        World &world = manager.createWorld();

        const WorldDebugStats initialStats = world.collectDebugStats();
        check("W1/world-time-automation-override",
              manager.getWorldTime() == 18000 &&
                  std::abs(initialStats.worldTime - 18000.f) < 0.01f);
        check("W1/world-stats-expose-environment",
              std::abs(initialStats.environment.cycle - 0.75f) < 0.00001f &&
                  initialStats.environment.daylight < 0.2f);

        check("S1.2/active-world-is-created-world",
              manager.getActiveWorld() == &world &&
                  manager.getWorld(WorldManager::MainWorldId) == &world);
        check("S1.2/active-world-id",
              manager.getActiveWorldId() == WorldManager::MainWorldId);

        EventRecorder events(world.getEventBus());

        const glm::vec3 destination(320.5f, 100.f, 320.5f);
        const bool teleported = manager.teleportPlayer(player, destination);
        check("S1.4/same-world-teleport", teleported);
        check("S1.4/teleport-moves-player",
              std::abs(player.position.x - destination.x) < 0.01f &&
                  std::abs(player.position.z - destination.z) < 0.01f,
              vecToString(player.position));
        check("S4.5/teleport-publishes-event",
              events.count(SandboxEventType::PlayerTeleport) == 1);
        check("S1.4/teleport-preloads-destination",
              world.getChunkManager().chunkLoadedAt(20, 20),
              "chunk (20,20) loaded");
        const ChunkDemandDebugStats teleportDemand =
            world.collectDebugStats().streamingDemand;
        check("B2/successful-teleport-elevates-destination",
              teleportDemand.teleportDemands == 1 &&
                  teleportDemand.activeDemands <=
                      ChunkDemandReasonCount);

        const std::uint64_t demandRevisionBeforeRejectedTeleport =
            teleportDemand.revision;
        const bool crossWorld = manager.teleportPlayer(player, 7, destination);
        check("S1.4/cross-world-teleport-rejected", !crossWorld);
        const ChunkDemandDebugStats rejectedTeleportDemand =
            world.collectDebugStats().streamingDemand;
        check("B2/rejected-teleport-does-not-mutate-demand",
              rejectedTeleportDemand.revision ==
                      demandRevisionBeforeRejectedTeleport &&
                  rejectedTeleportDemand.teleportDemands == 1);

        const int timeBefore = manager.getWorldTime();
        for (int i = 0; i < 20; ++i) {
            manager.tick();
        }
        check("S1.5/fixed-tick-advances-world-time",
              manager.getWorldTime() == timeBefore + 20,
              "time " + std::to_string(timeBefore) + " -> " +
                  std::to_string(manager.getWorldTime()));

        check("S1.2/save-world", manager.saveWorld());
        check("S1.2/save-unknown-world-fails", !manager.saveWorld(9));
    }

    setEnv("HELLOMINE3D_WORLD_TIME", "");
    {
        WorldManager manager(config, camera, player);
        check("S1.2/load-world", manager.loadWorld());
        check("S1.2/load-world-activates",
              manager.getActiveWorld() != nullptr &&
                  manager.closeAllWorlds() &&
                  manager.getActiveWorld() == nullptr);
    }

    setEnv("HELLOMINE3D_SAVE_DIR", "");
}

} // namespace

int main()
{
    struct FreeImageScope {
        FreeImageScope() { FreeImage_Initialise(FALSE); }
        ~FreeImageScope() { FreeImage_DeInitialise(); }
    } freeImageScope;

    try {
        std::cout << "[VALIDATION] world runtime smoke starting\n";
        runtimeToolRegistry().freeze(
            {{"runtime.tool", validToolDefinitions()}});
        runtimeEnemyRegistry().freeze(
            {{"runtime.enemy", validEnemyDefinitions()}});
        runtimeSmeltingRegistry().freeze(
            {{"runtime.smelting", validSmeltingDefinitions()}});
        runtimeFoodRegistry().freeze(
            {{"runtime.food", validFoodDefinitions()}});

        const char *focus = std::getenv("HELLOMINE3D_WORLD_SMOKE_FOCUS");
        if (focus != nullptr && std::string(focus) == "V10A") {
            caseGreedyMeshing();
            caseVertexLighting();
            caseSunlightStorage();
            caseBlockLightStorage();
        }
        else if (focus != nullptr && std::string(focus) == "V10B1") {
            caseBlockTextureCoordinates();
        }
        else if (focus != nullptr && std::string(focus) == "V10B3") {
            caseTerrainAppearance();
            caseGreedyMeshing();
        }
        else if (focus != nullptr && std::string(focus) == "V10C") {
            caseWorldEnvironment();
        }
        else if (focus != nullptr && std::string(focus) == "V10D") {
            caseRuntimeConfigOwnership();
        }
        else if (focus != nullptr && std::string(focus) == "V10E") {
            caseRuntimeConfigOwnership();
        }
        else if (focus != nullptr && std::string(focus) == "P11A") {
            caseWorldOutcomeAndLocalizedText();
            caseRuntimeConfigOwnership();
            caseP11ACoreInput();
            casePausedApplicationFlow();
            caseInteractionAndEvents();
        }
        else if (focus != nullptr && std::string(focus) == "P11B") {
            caseWorldOutcomeAndLocalizedText();
            caseRuntimeConfigOwnership();
            caseP11BActionFeedback();
            caseActors();
        }
        else if (focus != nullptr && std::string(focus) == "P11-0") {
            caseOreTextures();
            caseBlockTextureCoordinates();
            caseBackgroundLoaderStress();
            caseBlockLightStorage();
            caseLocalRelightAfterEdits();
            caseFurnaceProgression();
        }
        else if (focus != nullptr && std::string(focus) == "P11-1") {
            caseWorldOutcomeAndLocalizedText();
            caseOreTextures();
            caseP11MinimumBuildingAndTools();
        }
        else if (focus != nullptr && std::string(focus) == "P11C") {
            caseWorldOutcomeAndLocalizedText();
            caseDataDrivenObjectives();
            caseP11CFirstThirtyMinutes();
        }
        else if (focus != nullptr && std::string(focus) == "P11D") {
            caseWorldOutcomeAndLocalizedText();
            caseP11DExplorationRewards();
        }
        else if (focus != nullptr && std::string(focus) == "P11-2") {
            caseP11TerrainContoursAndEntrances();
        }
        else if (focus != nullptr && std::string(focus) == "P11E") {
            caseWorldOutcomeAndLocalizedText();
            caseP11EEnemyPresentationAndResonance();
        }
        else if (focus != nullptr && std::string(focus) == "AL-A3") {
            caseFixedTickScheduler();
            casePausedApplicationFlow();
            caseWorldSimulationRuntime();
        }
        else if (focus != nullptr && std::string(focus) == "AL-A4") {
            caseEventCommandQueryBoundary();
            caseInteractionAndEvents();
            caseDataDrivenObjectives();
        }
        else if (focus != nullptr && std::string(focus) == "AL-A5") {
            caseWorldSimulationRuntime();
            caseSimulationPhaseMetrics();
        }
        else if (focus != nullptr && std::string(focus) == "B1") {
            caseLocalRelightAfterEdits();
            caseChunkResidencyStateMachine();
            caseSectionMeshUploadSnapshot();
            caseUnloadPersistence();
            caseTerrainStructures();
        }
        else if (focus != nullptr && std::string(focus) == "B2") {
            caseStreamingDemandModel();
            caseWorldManager();
        }
        else {
        caseWorldOutcomeAndLocalizedText();
        caseWaystoneVictoryLoop();
        caseDebugPanelStartupOption();
        caseFixedTickScheduler();
        caseWorldSimulationRuntime();
        caseSimulationPhaseMetrics();
        caseWorldEnvironment();
        caseBlockTextureCoordinates();
        caseRuntimeConfigOwnership();
        caseP11ACoreInput();
        caseP11BActionFeedback();
        caseEventCommandQueryBoundary();
        casePausedApplicationFlow();
        caseAudioFeedback();
        caseStreamedMusic();
        caseBlockDataDiagnostics();
        caseOreTextures();
        caseConfiguredWorldSeed();
        caseBlockSelection();
        casePlayerControllerInput();
        casePlayerSweptCollision();
        caseHeightMapEdits();
        caseBackgroundLoaderStress();
        caseSpawnPreload();
        caseManagedWorldFirstSpawn();
        caseNegativeCoordinates();
        caseNoImplicitChunkCreation();
        caseMeshDirtyPropagation();
        casePersistence();
        caseSectionMeshInput();
        caseGreedyMeshing();
        caseTerrainAppearance();
        caseVertexLighting();
        caseTerrainBufferMetrics();
        caseTransparentBlockRules();
        caseBlockBehaviorDispatch();
        caseMetadataBackedBehavior();
        caseRandomTickScheduling();
        caseResourceDrivenBlockShapes();
        caseSunlightStorage();
        caseBlockLightStorage();
        caseLocalRelightAfterEdits();
        caseEnclosedSectionSkip();
        caseFrustumMeshPriority();
        caseStreamingDemandModel();
        caseChunkResidencyStateMachine();
        caseSectionMeshUploadSnapshot();
        caseUnloadPersistence();
        caseBlockEntityLifecycle();
        caseChestContainer();
        caseFurnaceProgression();
        caseFoodRecovery();
        caseExpandedResourceEconomy();
        caseDifficultyProfiles();
        casePostVictoryEvents();
        caseWorkbenchCrafting();
        caseToolMiningProgression();
        caseP11MinimumBuildingAndTools();
        caseP11CFirstThirtyMinutes();
        caseP11DExplorationRewards();
        caseP11TerrainContoursAndEntrances();
        caseP11EEnemyPresentationAndResonance();
        caseNaturalMobPopulation();
        caseCombatAndRespawn();
        caseCombatDepth();
        caseCombatReadability();
        caseRangedCombatProjectiles();
        caseWheatCropLoop();
        casePlayableVerticalSlice();
        caseDataDrivenObjectives();
        casePlayableAlphaJourney();
        caseChunkFormatRejection();
        caseTerrainDeterminism();
        caseTerrainStructures();
        caseEcologyAndExploration();
        caseDeterministicStructurePlanning();
        caseExplorationStructuresAndLoot();
        caseInteractionAndEvents();
        caseChunkEvents();
        caseActors();
        caseWorldManager();
        }
    }
    catch (const std::exception &error) {
        std::cerr << "[VALIDATION] unhandled exception: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "[VALIDATION] checks=" << g_checkCount
              << " failures=" << g_failureCount << '\n';
    std::cout << "[VALIDATION] status="
              << (g_failureCount == 0 ? "PASS" : "FAIL") << '\n';

    return g_failureCount == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
