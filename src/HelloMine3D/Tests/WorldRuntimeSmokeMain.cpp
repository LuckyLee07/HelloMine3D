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
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <FreeImage.h>

#include "../Actor/EnemyRegistry.h"
#include "../Actor/ItemEntity.h"
#include "../Actor/LivingActor.h"
#include "../Actor/MobActor.h"
#include "../Audio/AudioDefinitionRegistry.h"
#include "../Audio/AudioRuntime.h"
#include "../Config.h"
#include "../Core/Camera.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Diagnostics/RuntimeDebugOptions.h"
#include "../Diagnostics/TerrainBufferMetrics.h"
#include "../Gameplay/AlphaJourney.h"
#include "../Item/Material.h"
#include "../Item/CraftingSession.h"
#include "../Item/ContainerInventory.h"
#include "../Item/FoodRegistry.h"
#include "../Item/RecipeRegistry.h"
#include "../Item/SmeltingRegistry.h"
#include "../Item/ToolRegistry.h"
#include "../Player/Player.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/ChunkEvents.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/FoodEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SmeltingEvents.h"
#include "../Sandbox/FixedTickScheduler.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Sandbox/WorldManager.h"
#include "../Util/ResourcePaths.h"
#include "../World/Block/BlockBehavior.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockTextureCoordinates.h"
#include "../World/Interaction/BlockSelection.h"
#include "../World/Interaction/BlockInteractionSystem.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "../World/Chunk/ChunkMeshBuilder.h"
#include "../World/Chunk/SectionMeshInput.h"
#include "../World/Environment/WorldEnvironment.h"
#include "../World/Generation/Biome/TemperateForestBiome.h"
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
// E0 - block data and mesh UV generation do not require a graphics context
// ---------------------------------------------------------------------------
void caseBlockTextureCoordinates()
{
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
              std::abs(generated.mouseSensitivity - 0.05f) < 0.0001f &&
              !generated.invertMouseY &&
              std::abs(generated.uiScale - 1.f) < 0.0001f &&
              generated.audioCaptions && generated.showActionHints &&
              generated.inputBindings.get(GameplayAction::MoveForward) ==
                  GameplayKey::W &&
              generated.inputBindings.get(GameplayAction::ConsumeFood) ==
                  GameplayKey::R &&
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
        check("G4/settings-file-is-versioned",
               text.find("settings_version 2\n") != std::string::npos &&
                   text.find("mastervolume 1") != std::string::npos &&
                   text.find("ambientvolume 1") != std::string::npos &&
                   text.find("uiscale 1") != std::string::npos &&
                   text.find("key_consume_food r") != std::string::npos,
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
        check("G4/legacy-settings-migrated-atomically",
               text.find("settings_version 2\n") == 0 &&
                   text.find("uivolume 1") != std::string::npos &&
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
        check("N6/version-one-settings-migrate-with-accessibility-defaults",
              versionOne.renderDistance == 6 && versionOne.fov == 95 &&
                  versionOne.audioCaptions && versionOne.showActionHints &&
                  versionOne.inputBindings.get(
                      GameplayAction::OpenCrafting) == GameplayKey::E &&
                  text.find("settings_version 2\n") == 0,
              text);
    }

    RuntimeSettingsSession session;
    session.begin(userSettings(customised));
    session.draft().fov = 115;
    session.draft().renderDistance = 5;
    RuntimeSettingsApplyPlan plan;
    std::string settingsError;
    check("G4/settings-draft-prepares-valid-apply",
          session.prepareApply(plan, settingsError) &&
              plan.settings.fov == 115 &&
              plan.renderDistanceChanged && !plan.restartRequired,
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
              std::abs(plan.settings.masterVolume - 1.0f) < 0.0001f);
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
    persisted.audioCaptions = false;
    persisted.showActionHints = false;
    persisted.inputBindings.set(GameplayAction::ConsumeFood,
                                GameplayKey::Q);
    check("G4/settings-save-publishes-valid-candidate",
          saveRuntimeConfig(configPath.string(), persisted,
                            &settingsError),
          settingsError);
    const Config reloaded = loadRuntimeConfig(configPath.string());
    check("G4/settings-save-preserves-world-creation-seed",
          reloaded.fov == 96 && reloaded.worldSeed.has_value() &&
              *reloaded.worldSeed == 77123 &&
              std::abs(reloaded.uiScale - 1.25f) < 0.0001f &&
              !reloaded.audioCaptions && !reloaded.showActionHints &&
              reloaded.inputBindings.get(GameplayAction::ConsumeFood) ==
                  GameplayKey::Q);

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
                  "settings_version 2\nkey_consume_food mouse9\n"));
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
    const bool interactiveCoordinates =
        chest.render.texTopCoord == glm::ivec2(0, 1) &&
        workbench.render.texTopCoord == glm::ivec2(1, 1) &&
        furnace.render.texTopCoord == glm::ivec2(2, 1);
    const std::set<std::uint64_t> interactiveHashes = {
        hashTile(0, 1), hashTile(1, 1), hashTile(2, 1)};
    check("FS3/interactive-blocks-use-dedicated-tiles",
          interactiveCoordinates && interactiveHashes.size() == 3 &&
              visiblePixelCount(0, 1) > 24 &&
              visiblePixelCount(1, 1) > 24 &&
              visiblePixelCount(2, 1) > 24);

    std::set<std::uint64_t> itemHashes;
    bool itemTilesPopulated = true;
    for (int tileX = 0; tileX < 10; ++tileX) {
        itemHashes.insert(hashTile(tileX, 2));
        itemTilesPopulated = itemTilesPopulated &&
                             visiblePixelCount(tileX, 2) > 12;
    }
    check("FS3/item-icons-are-populated-and-distinct",
          itemTilesPopulated && itemHashes.size() == 10,
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
            stats.chunks.meshDirtySections + stats.chunks.cpuReadySections +
            stats.chunks.gpuBufferedSections;
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
        const bool shouldRemainDirty =
            index >= World::ChunkMeshRebuildBudgetPerUpdate;
        fifoState = fifoState && queueTargets[index].section != nullptr &&
                    queueTargets[index].section->isMeshDirty() ==
                        shouldRemainDirty;
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
                          !entry.section->isMeshDirty();
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
    check("M4/flat-cuboid-greedy-face-drop",
          singleMaterial.solidMesh.faces == 6 &&
              singleSolid.vertexPositions.size() / 3 == 24 &&
              singleSolid.indices.size() == 36,
          "faces=" + std::to_string(singleMaterial.solidMesh.faces) +
              " naive=160");

    const float maxRepeat = singleSolid.textureRepeatCoords.empty()
                                ? 0.f
                                : *std::max_element(
                                      singleSolid.textureRepeatCoords.begin(),
                                      singleSolid.textureRepeatCoords.end());
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
    check("M4/merged-quad-repeats-atlas-tile",
          std::abs(maxRepeat - 8.f) < 0.001f && stoneTileStable &&
              singleSolid.textureRepeatCoords.size() ==
                  singleSolid.vertexPositions.size() / 3 * 2,
          "max_repeat=" + std::to_string(maxRepeat));

    for (int z = 4; z < 12; ++z) {
        for (int x = 8; x < 12; ++x) {
            world.setBlock(x, blockY, z, BlockId::Dirt);
        }
    }
    ChunkMeshCollection splitMaterials = buildSectionMeshes();
    check("M4/material-boundary-preserved",
          splitMaterials.solidMesh.faces == 10,
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

    const ChunkBlock stone(BlockId::Stone);
    const auto &stoneDefinition =
        database.getDefinition(BlockId::Stone);
    check("C1/default-behavior-preserves-drop",
          stoneDefinition.behavior->getDrop(stoneDefinition, stone) ==
              Material::ID::Stone);

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
        bool foundCaveLight = false;
        const std::size_t faceCount = solid.vertexPositions.size() / 12;
        for (std::size_t face = 0; face < faceCount; ++face) {
            bool isFloorTop = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t positionIndex =
                    face * 12 + vertex * 3 + 1;
                isFloorTop =
                    isFloorTop &&
                    std::abs(solid.vertexPositions[positionIndex] -
                             static_cast<float>(floorY + 1)) < 0.001f;
            }
            if (!isFloorTop) {
                continue;
            }

            ++floorTopFaces;
            const float faceLight = light[face * 4];
            foundSurfaceLight =
                foundSurfaceLight ||
                std::abs(faceLight - 1.f) < 0.001f;
            foundCaveLight =
                foundCaveLight ||
                std::abs(faceLight - MIN_TERRAIN_BRIGHTNESS) < 0.001f;
        }
        check("L1/mesh-distinguishes-surface-and-cave",
              foundSurfaceLight && foundCaveLight,
              "surface=" + std::to_string(foundSurfaceLight) +
                  " cave=" + std::to_string(foundCaveLight));
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

    check("L2/emissive-definition-is-data-driven",
          BlockDatabase::get().getDefinition(BlockId::Rose).light == 14,
          "light=" + std::to_string(
                         BlockDatabase::get()
                             .getDefinition(BlockId::Rose)
                             .light));

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
        world.setBlock(sourceX, floorY, z, BlockId::Stone);
        world.setBlock(targetX, floorY, z, BlockId::Stone);
        world.setBlock(sourceX, floorY + 1, z, BlockId::Rose);
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
        const std::size_t faceCount = solid.vertexPositions.size() / 12;
        for (std::size_t face = 0; face < faceCount; ++face) {
            float minX = 100000.f;
            float maxX = -100000.f;
            float minZ = 100000.f;
            float maxZ = -100000.f;
            bool topHeight = true;
            for (std::size_t vertex = 0; vertex < 4; ++vertex) {
                const std::size_t index = face * 12 + vertex * 3;
                minX = std::min(minX, solid.vertexPositions[index]);
                maxX = std::max(maxX, solid.vertexPositions[index]);
                minZ = std::min(minZ, solid.vertexPositions[index + 2]);
                maxZ = std::max(maxZ, solid.vertexPositions[index + 2]);
                topHeight =
                    topHeight &&
                    std::abs(solid.vertexPositions[index + 1] -
                             static_cast<float>(floorY + 1)) < 0.001f;
            }

            const bool targetBounds =
                std::abs(minX - targetX) < 0.001f &&
                std::abs(maxX - (targetX + 1)) < 0.001f &&
                std::abs(minZ - z) < 0.001f &&
                std::abs(maxZ - (z + 1)) < 0.001f;
            if (topHeight && targetBounds &&
                std::abs(light[face * 4] - lightLevelToBrightness(13)) <
                    0.001f) {
                foundLitTargetFace = true;
                break;
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

        world.setBlock(sourceX, sourceY, z, BlockId::Rose);
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
                  neighbourSection->isMeshDirty());

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
              section->getMeshState() == ChunkSectionMeshState::CpuReady);
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
              section->getMeshState() == ChunkSectionMeshState::CpuReady &&
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
          section->getMeshState() == ChunkSectionMeshState::CpuReady,
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
          section->getMeshState() == ChunkSectionMeshState::GpuBuffered,
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

    blocked.output.amount = 1;
    world.updateBlockEntity(position, FurnaceContainer::serialize(blocked));
    for (int tick = 110; tick < 170; ++tick) {
        world.tick(tick);
    }
    furnace = FurnaceContainer::view(
        world, player, runtimeSmeltingRegistry());
    check("N2/fuel-exhaustion-preserves-partial-progress",
          furnace && furnace->state.progressTicks == 60 &&
              furnace->state.burnTicksRemaining == 0 &&
              furnace->state.input.amount == 1);

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
        "version 1\n"
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

std::string validAudioDefinitions()
{
    return R"(# HelloMine3D audio definitions v2
sound ui.click ui 2d sine 720 45 0.22 2 "Menu selection"
sound block.break effects 3d noise 180 95 0.48 4 "Block broken"
sound block.place effects 3d square 130 70 0.32 4 "Block placed"
sound item.pickup effects 3d sine 980 85 0.28 3 "Item collected"
sound craft.success effects 2d sine 540 150 0.30 2 "Crafting complete"
sound combat.hit effects 3d noise 90 110 0.52 4 "Combat hit"
sound ambient.wind ambient 2d noise 55 1200 0.10 1 "Wind"
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
              loaded.definitions().size() == 7 && ui != nullptr &&
              block != nullptr && ui->caption == "Menu selection" &&
              block->caption == "Block broken");
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
                      "sound ui.click ui 2d sine 440 50 0.2 1 \"Again\"\n",
                  "duplicate cue id"));
    std::string missingCaption = validAudioDefinitions();
    missingCaption.replace(missingCaption.find(" \"Menu selection\""),
                           std::string(" \"Menu selection\"").size(), "");
    check("N6/audio-caption-is-required-and-bounded",
          rejects(missingCaption, "expected sound") ||
              rejects(missingCaption, "caption"));
    std::string invalidFrequency = validAudioDefinitions();
    invalidFrequency.replace(invalidFrequency.find("720 45"), 6,
                             "10 45");
    check("G5/audio-range-is-strictly-validated",
          rejects(invalidFrequency, "frequency must be between"));
    const std::size_t ambientLine =
        validAudioDefinitions().find("sound ambient.wind");
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
    std::vector<std::string> captions;
    audio->setCaptionSink([&captions](std::string caption) {
        captions.push_back(std::move(caption));
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
              captions.back() == "Menu selection");

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
    CraftCompletedEvent lastCompleted(
        "", Material::ID::Nothing, 0, 0, glm::vec3(0.f));
    craftingBus.subscribe(
        SandboxEventType::CraftCompleted,
        [&completedEvents, &lastCompleted](const SandboxEvent &event) {
            ++completedEvents;
            lastCompleted = static_cast<const CraftCompletedEvent &>(event);
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
              lastCompleted.recipeId == "hellomine:workbench" &&
              lastCompleted.outputMaterialId == Material::ID::Workbench &&
              lastCompleted.craftsCompleted == 1 &&
              lastCompleted.outputAdded == 1);
    crafter.detachEventBus(craftingBus);
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
)";
}

std::string validEnemyDefinitions()
{
    return R"(# HelloMine3D enemy registry v1
enemy hellomine:natural_mob
health 10
dimensions 0.35 0.9 0.35
wander_speed 1.2
chase_radius 12
chase_speed 2.4
contact_damage 2
natural 0
loot hellomine:dirt 1 1
end
enemy hellomine:stalker
health 8
dimensions 0.30 0.75 0.30
wander_speed 1.6
chase_radius 14
chase_speed 3.2
contact_damage 1
natural 1
loot hellomine:dirt 1 1
loot hellomine:wheat 1 2
end
enemy hellomine:brute
health 16
dimensions 0.45 1.05 0.45
wander_speed 0.8
chase_radius 10
chase_speed 1.6
contact_damage 4
natural 1
loot hellomine:dirt 1 1
loot hellomine:coal_ore 1 1
loot hellomine:wheat 1 1
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
fuel hellomine:coal_ore
ticks 160
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
                             Material::ID::Stone &&
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
              upgraded.find("version 8") != std::string::npos &&
              upgraded.find("alpha_journey_flags 0") !=
                  std::string::npos &&
              upgraded.find("objective_definition_version 1") !=
                  std::string::npos &&
              upgraded.find("objective_completed_count 0") !=
                  std::string::npos &&
              upgraded.find("objective_progress_count 0") !=
                  std::string::npos);
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

    const int secondDamageDeadline = tick + 14;
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
              stalker->getLootTable().size() == 2);

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
    int dirtAmount = 0;
    int stalkerItemCount = 0;
    for (const ActorSaveState &state : stalkerDrops) {
        if (state.kind != ActorSaveKind::Item) {
            continue;
        }
        ++stalkerItemCount;
        if (state.materialId == static_cast<int>(Material::ID::Wheat)) {
            wheatAmount += state.amount;
        }
        else if (state.materialId == static_cast<int>(Material::ID::Dirt)) {
            dirtAmount += state.amount;
        }
    }
    check("N4/stalker-death-drops-bounded-recovery-loot",
          lethal == CombatAttackResult::Hit && stalker != nullptr &&
              !stalker->isAlive() && wheatAmount >= 1 && wheatAmount <= 2 &&
              dirtAmount == 1 && stalkerItemCount == 2 &&
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
              brute->getLootTable().size() == 3);
    const bool bruteKilled = world.attackActor(bruteId, 100.f);
    const std::vector<ActorSaveState> allDrops =
        world.getActorManager().collectSaveStates();
    int coalAmount = 0;
    for (const ActorSaveState &state : allDrops) {
        if (state.kind == ActorSaveKind::Item &&
            state.materialId == static_cast<int>(Material::ID::CoalOre)) {
            coalAmount += state.amount;
        }
    }
    check("N4/brute-death-drops-bounded-progression-loot",
          bruteKilled && brute != nullptr && !brute->isAlive() &&
              coalAmount == 1 && allDrops.size() == beforeBruteDrops + 3);
    const WorldDebugStats enemyStats = world.collectDebugStats();
    check("N4/both-new-archetypes-are-natural-population-types",
          World::isNaturalMobType(World::StalkerMobType) &&
              World::isNaturalMobType(World::BruteMobType) &&
              enemyStats.naturalMobWorldCap == World::NaturalMobWorldCap);

    const auto contactDamage = [&](const std::string &name,
                                   const std::string &type) {
        const auto contactDirectory = freshSaveDirectory(name);
        Player contactPlayer;
        World contactWorld(camera, config, contactPlayer,
                           contactDirectory, false, 1);
        contactWorld.spawnMob(type, contactPlayer.position);
        contactWorld.tick(1);
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
    BlockMetadata_t cropStageBeforeSave = 0;
    int dirtAfterPickup = 0;
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
            if (target != nullptr) {
                const float x = target->position.x - player.position.x;
                const float z = target->position.z - player.position.z;
                mobReachedPlayer = x * x + z * z <= 1.f;
            }
        }
        const bool lethalHit =
            firstHit && mobReachedPlayer &&
            world.attackActor(defeatedMobId, 100.f);
        const std::vector<ActorSaveState> postCombat =
            world.getActorManager().collectSaveStates();
        const auto loot = std::find_if(
            postCombat.begin(), postCombat.end(),
            [](const ActorSaveState &state) {
                return state.kind == ActorSaveKind::Item &&
                       state.materialId ==
                           static_cast<int>(Material::ID::Dirt) &&
                       state.amount == 1;
            });
        check("D6/natural-encounter-produces-combat-loot",
              foundNaturalMob && firstHit && lethalHit &&
                  loot != postCombat.end());

        const int dirtBeforePickup =
            countPlayer(player, Material::ID::Dirt);
        for (int pickupTick = 0; pickupTick < 11; ++pickupTick) {
            world.tick(++combatTick);
        }
        dirtAfterPickup = countPlayer(player, Material::ID::Dirt);
        const std::vector<ActorSaveState> postPickup =
            world.getActorManager().collectSaveStates();
        const bool lootStillExists = std::any_of(
            postPickup.begin(), postPickup.end(),
            [](const ActorSaveState &state) {
                return state.kind == ActorSaveKind::Item &&
                       state.materialId ==
                           static_cast<int>(Material::ID::Dirt);
            });
        check("D6/pick-up-defeated-mob-loot",
              dirtAfterPickup == dirtBeforePickup + 1 &&
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
                  countPlayer(restoredPlayer, Material::ID::Dirt) ==
                      dirtAfterPickup &&
                  !defeatedMobRestored);
        ChestContainer::close(restoredPlayer);
    }
}

std::string validObjectiveTestDefinitions()
{
    return R"(# HelloMine3D objective registry v1
version 1
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
          baseLoaded && baseRegistry.definitionVersion() == 1 &&
              baseRegistry.definitions().size() == 23 &&
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
    invalidDefinition.objectiveState.definitionVersion = 2;
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
              loaded.objectiveState.definitionVersion == 1 &&
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
              migratedData.objectiveState.definitionVersion == 1 &&
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
    int dirtBeforeRelaunch = 0;
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
                  countPlayer(player, Material::ID::Stone) ==
                      AlphaJourney::RequiredStone &&
                  player.getInventorySlot(woodenSlot).getDurability() == 13 &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CraftStonePickaxe);

        const bool reopenedWorkbench = BlockInteractionSystem::useBlock(
            world, player,
            glm::vec3(workbenchPosition) + glm::vec3(0.5f));
        const bool stonePickaxeCrafted = reopenedWorkbench && craft(
            player, CraftingSession::WorkbenchGridSize,
            {{0, Material::ID::Stone}, {1, Material::ID::Stone},
             {2, Material::ID::Stone}, {4, Material::ID::OakBark},
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
            if (target != nullptr) {
                const float x = target->position.x - player.position.x;
                const float z = target->position.z - player.position.z;
                mobReachedPlayer = x * x + z * z <= 1.f;
            }
        }
        const bool lethalHit = firstHit && mobReachedPlayer &&
                               world.attackActor(defeatedMobId, 100.f);
        check("G6/natural-mob-is-defeated-through-combat-rules",
              foundNaturalMob && lethalHit &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::CollectMobLoot);

        const int dirtBeforePickup =
            countPlayer(player, Material::ID::Dirt);
        for (int pickupTick = 0; pickupTick < 11; ++pickupTick) {
            world.tick(++combatTick);
        }
        dirtBeforeRelaunch = countPlayer(player, Material::ID::Dirt);
        check("G6/physical-mob-drop-is-picked-up",
              dirtBeforeRelaunch == dirtBeforePickup + 1 &&
                  world.getAlphaJourneySnapshot().step ==
                      AlphaJourneyStep::ReopenWorld,
              "dirt=" + std::to_string(dirtBeforePickup) + "->" +
                  std::to_string(dirtBeforeRelaunch) + " step=" +
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
            ironBeforeRelaunch + dirtBeforeRelaunch;
        check("G6/pre-save-state-conserves-tools-ore-and-loot",
              totalBeforeSave == 4 && flagsBeforeRelaunch == 9u &&
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
              countPlayer(restoredPlayer, Material::ID::Dirt) ==
                  dirtBeforeRelaunch &&
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
    Config config = makeConfig();
    Camera camera(config);

    SurfaceSample before;
    const int chunkSpan = kStructureChunkRadius * 2 + 1;
    const std::size_t chunkCount =
        static_cast<std::size_t>(chunkSpan) * chunkSpan;

    {
        Player player;
        World world(camera, config, player, directory, false, 1);
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

        ClassicOverWorldGenerator forwardGenerator(kValidationSeed);
        ClassicOverWorldGenerator reverseGenerator(kValidationSeed);
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
        kValidationSeed, CurrentTerrainGenerationVersion);
    ClassicOverWorldGenerator legacyGenerator(
        kValidationSeed, LegacyTerrainGenerationVersion);
    ClassicOverWorldGenerator otherSeedGenerator(
        kValidationSeed + 1, CurrentTerrainGenerationVersion);
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
                  TerrainBiome::TemperateForest)) == World::StalkerMobType &&
              std::string(World::naturalMobTypeForBiome(
                  TerrainBiome::Ocean)) == World::StalkerMobType);

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
    check("N5/new-world-persists-generation-v2-and-rejects-invalid-version",
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
                  Material::ID::Stone &&
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
          world.getBlock(8, y, 8).id == static_cast<Block_t>(BlockId::Stone));
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

    events.reset();
    const bool used =
        BlockInteractionSystem::useBlock(world, player, target);
    check("P5/use-through-interaction-system", used);
    check("P5/use-publishes-event",
          events.count(SandboxEventType::BlockUse) == 1);
    check("P5/use-event-identifies-target",
          usedPosition == glm::ivec3(8, y, 8) &&
              usedBlock == BlockId::Stone);

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

    // S5.5 - item entity spawn and pickup.
    events.reset();
    player.position = glm::vec3(8.5f, 100.f, 8.5f);
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

        const bool crossWorld = manager.teleportPlayer(player, 7, destination);
        check("S1.4/cross-world-teleport-rejected", !crossWorld);

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

        caseDebugPanelStartupOption();
        caseFixedTickScheduler();
        caseWorldEnvironment();
        caseBlockTextureCoordinates();
        caseRuntimeConfigOwnership();
        casePausedApplicationFlow();
        caseAudioFeedback();
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
        caseSectionMeshUploadSnapshot();
        caseUnloadPersistence();
        caseBlockEntityLifecycle();
        caseChestContainer();
        caseFurnaceProgression();
        caseFoodRecovery();
        caseWorkbenchCrafting();
        caseToolMiningProgression();
        caseNaturalMobPopulation();
        caseCombatAndRespawn();
        caseCombatDepth();
        caseWheatCropLoop();
        casePlayableVerticalSlice();
        caseDataDrivenObjectives();
        casePlayableAlphaJourney();
        caseChunkFormatRejection();
        caseTerrainDeterminism();
        caseTerrainStructures();
        caseEcologyAndExploration();
        caseInteractionAndEvents();
        caseChunkEvents();
        caseActors();
        caseWorldManager();
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
