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
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <FreeImage.h>

#include "../Actor/ItemEntity.h"
#include "../Actor/LivingActor.h"
#include "../Actor/MobActor.h"
#include "../Config.h"
#include "../Core/Camera.h"
#include "../Diagnostics/RuntimeDebugOptions.h"
#include "../Item/Material.h"
#include "../Player/Player.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/ChunkEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/FixedTickScheduler.h"
#include "../Sandbox/WorldManager.h"
#include "../Util/ResourcePaths.h"
#include "../World/Block/BlockBehavior.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockTextureCoordinates.h"
#include "../World/Interaction/BlockSelection.h"
#include "../World/Interaction/BlockInteractionSystem.h"
#include "../World/Chunk/ChunkMeshBuilder.h"
#include "../World/Chunk/SectionMeshInput.h"
#include "../World/Generation/Biome/TemperateForestBiome.h"
#include "../World/Storage/ChunkStorage.h"
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
              generated.fov == 90 && !generated.worldSeed.has_value(),
          std::to_string(generated.renderDistance) + " " +
              std::to_string(generated.isFullscreen) + " " +
              std::to_string(generated.windowX) + "x" +
              std::to_string(generated.windowY) + " " +
              std::to_string(generated.fov));

    {
        std::ofstream output(configPath,
                             std::ios::binary | std::ios::trunc);
        output << "renderdistance 3\n"
               << "fullscreen 1\n"
               << "windowsize 1024 768\n"
               << "fov 100\n";
    }
    const Config customised = loadRuntimeConfig(configPath.string());
    check("A3/user-config-overrides-defaults",
          customised.renderDistance == 3 && customised.isFullscreen &&
              customised.windowX == 1024 && customised.windowY == 768 &&
              customised.fov == 100);
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

    const auto stoneHash = hashTile(3, 0);
    const auto coalHash = hashTile(13, 0);
    const auto ironHash = hashTile(14, 0);
    check("P4/coal-texture-distinct", coalHash != stoneHash);
    check("P4/iron-texture-distinct",
          ironHash != stoneHash && ironHash != coalHash);
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
    input.toggleSneaking = true;
    input.hotbarSlot = 3;
    input.lookDelta = {20.f, -10.f};
    controller.applyInput(player, input);

    check("V2/fly-toggle", player.isFlying());
    check("V2/sneak-toggle", player.isSneaking());
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
    const auto progressDeadline = std::chrono::steady_clock::now() +
                                  std::chrono::seconds(5);
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

    // The spawn column must actually be solid ground, not an air pocket.
    const auto ground =
        world.getBlock(World::toBlockCoord(player.position.x), spawnHeight - 1,
                       World::toBlockCoord(player.position.z));
    check("S0.6/spawn-on-solid-ground", ground.id != 0,
          "block under spawn id=" + std::to_string(static_cast<int>(ground.id)));
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

        check("S2.1/world-save-succeeds", world.save());

        WorldSaveData meta;
        WorldSave saveFile(directory);
        check("S2.1/world-meta-readable", saveFile.load(meta));
        savedSpawn = meta.spawnPoint;
        check("S2.1/world-meta-has-seed", meta.seed == firstSeed,
              "meta seed=" + std::to_string(meta.seed));
        check("P2/world-meta-stores-actors",
              meta.version == 2 && meta.actors.size() == 2,
              "version=" + std::to_string(meta.version) +
                  " actors=" + std::to_string(meta.actors.size()));
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
                  after.plantBlocks == before.plantBlocks,
              "trees " + std::to_string(before.treeBlocks) + " -> " +
                  std::to_string(after.treeBlocks) + ", plants " +
                  std::to_string(before.plantBlocks) + " -> " +
                  std::to_string(after.plantBlocks));
        check("S6.5/surface-composition-stable",
              after.topBlockIds == before.topBlockIds,
              idListToString(before.topBlockIds) + "-> " +
                  idListToString(after.topBlockIds));
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
    world.setBlock(8, y, 8, BlockId::Stone);

    const glm::vec3 target(8.5f, static_cast<float>(y) + 0.5f, 8.5f);
    const bool broke = BlockInteractionSystem::breakBlock(world, player, target);
    check("S3.4/break-through-interaction-system", broke);
    check("S3.4/break-clears-block", world.getBlock(8, y, 8).id == 0);
    check("S3.5/break-adds-configured-drop",
          player.getHeldItems().getMaterial().id == Material::ID::Stone &&
              player.getHeldItems().getNumInStack() == 1,
          "held " + player.getHeldItems().getMaterial().name + " x" +
              std::to_string(player.getHeldItems().getNumInStack()));
    check("S4.2/break-publishes-events",
          events.count(SandboxEventType::BlockBreak) == 1 &&
              events.count(SandboxEventType::BlockChanged) == 1,
          "break=" + std::to_string(events.count(SandboxEventType::BlockBreak)) +
              " changed=" +
              std::to_string(events.count(SandboxEventType::BlockChanged)));
    check("S4.5/break-publishes-inventory-event",
          events.count(SandboxEventType::PlayerInventoryChanged) == 1);

    events.reset();
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

    auto *mob = dynamic_cast<LivingActor *>(
        world.getActorManager().findActor(mobId));
    if (mob == nullptr) {
        check("S5.2/mob-is-living-actor", false);
        return;
    }
    check("S5.2/mob-is-living-actor", true);

    const glm::vec3 startPosition = mob->position;
    for (int i = 0; i < 20; ++i) {
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

    events.reset();
    mob->damage(world, 1000.f);
    check("S5.2/mob-dies", !mob->isAlive());
    check("S4.4/entity-death-event",
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

    Config config = makeConfig();
    Camera camera(config);
    Player player;

    {
        WorldManager manager(config, camera, player);
        World &world = manager.createWorld();

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

    {
        WorldManager manager(config, camera, player);
        check("S1.2/load-world", manager.loadWorld());
        check("S1.2/load-world-activates",
              manager.getActiveWorld() != nullptr);
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

        caseDebugPanelStartupOption();
        caseFixedTickScheduler();
        caseBlockTextureCoordinates();
        caseRuntimeConfigOwnership();
        caseBlockDataDiagnostics();
        caseOreTextures();
        caseConfiguredWorldSeed();
        caseBlockSelection();
        casePlayerControllerInput();
        caseHeightMapEdits();
        caseBackgroundLoaderStress();
        caseSpawnPreload();
        caseNegativeCoordinates();
        caseNoImplicitChunkCreation();
        caseMeshDirtyPropagation();
        casePersistence();
        caseSectionMeshInput();
        caseGreedyMeshing();
        caseTransparentBlockRules();
        caseBlockBehaviorDispatch();
        caseMetadataBackedBehavior();
        caseResourceDrivenBlockShapes();
        caseSunlightStorage();
        caseBlockLightStorage();
        caseLocalRelightAfterEdits();
        caseEnclosedSectionSkip();
        caseFrustumMeshPriority();
        caseSectionMeshUploadSnapshot();
        caseUnloadPersistence();
        caseChunkFormatRejection();
        caseTerrainDeterminism();
        caseTerrainStructures();
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
