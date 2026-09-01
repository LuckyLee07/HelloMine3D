// Deterministic long-running world soak for R2.
//
// The executable uses the real World, background loader, mesh state machine,
// actor lifecycle and persistence paths. The wrapper samples operating-system
// memory and handle counters while this process records bounded world stats.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <FreeImage.h>

#include "../Actor/ItemEntity.h"
#include "../Config.h"
#include "../Core/Camera.h"
#include "../Player/Player.h"
#include "../Util/ResourcePaths.h"
#include "../World/Block/BlockId.h"
#include "../World/Block/ChunkBlock.h"
#include "../World/World.h"

namespace {

constexpr int FixedTicksPerSecond = 20;
constexpr int DefaultDurationSeconds = 2;
constexpr int DefaultSeed = 20260813;

enum class SoakProfile {
    Legacy,
    Nominal,
    Stress,
};

struct Options {
    int durationSeconds = DefaultDurationSeconds;
    int seed = DefaultSeed;
    std::filesystem::path outputDirectory;
    bool explicitOutputDirectory = false;
    SoakProfile profile = SoakProfile::Legacy;
};

const char *profileName(SoakProfile profile)
{
    switch (profile) {
    case SoakProfile::Legacy: return "legacy";
    case SoakProfile::Nominal: return "nominal";
    case SoakProfile::Stress: return "stress";
    }
    return "unknown";
}

int scheduleVersion(SoakProfile profile)
{
    return profile == SoakProfile::Legacy ? 1 : 2;
}

int parseBoundedInt(const std::string &text, const char *label,
                    int minimum, int maximum)
{
    std::size_t consumed = 0;
    const long long parsed = std::stoll(text, &consumed, 10);
    if (consumed != text.size() || parsed < minimum || parsed > maximum) {
        throw std::runtime_error(std::string("Invalid ") + label + ": " +
                                 text);
    }
    return static_cast<int>(parsed);
}

Options parseOptions(int argc, char **argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--duration-seconds" && index + 1 < argc) {
            options.durationSeconds = parseBoundedInt(
                argv[++index], "duration", 1, 24 * 60 * 60);
        }
        else if (argument == "--seed" && index + 1 < argc) {
            options.seed = parseBoundedInt(
                argv[++index], "seed", 1, 2147483647);
        }
        else if (argument == "--output-dir" && index + 1 < argc) {
            options.outputDirectory = argv[++index];
            options.explicitOutputDirectory = true;
        }
        else if (argument == "--profile" && index + 1 < argc) {
            const std::string profile = argv[++index];
            if (profile == "nominal") {
                options.profile = SoakProfile::Nominal;
            }
            else if (profile == "stress") {
                options.profile = SoakProfile::Stress;
            }
            else if (profile != "legacy") {
                throw std::runtime_error("Invalid soak profile: " + profile);
            }
        }
        else {
            throw std::runtime_error("Unknown or incomplete argument: " +
                                     argument);
        }
    }

    if (options.outputDirectory.empty()) {
        options.outputDirectory =
            ResourcePaths::bin("soak_runs/developer_smoke");
    }
    return options;
}

void setEnvironment(const char *name, const std::string &value)
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

const char *buildConfiguration()
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

void writeSummary(const std::filesystem::path &path, const Options &options,
                  int completedTicks, int failures, int movements, int edits,
                  int actorCycles, int reloads,
                  const WorldDebugStats &finalStats,
                  std::size_t maxExistingChunks,
                  std::size_t maxLoadedChunks,
                  std::size_t maxQueuedUpdates,
                  std::size_t maxActors,
                  std::size_t maxDirtySections)
{
    std::ofstream summary(path, std::ios::trunc);
    if (!summary) {
        throw std::runtime_error("Unable to write soak summary: " +
                                 path.string());
    }

    summary << "status=" << (failures == 0 ? "PASS" : "FAIL") << '\n'
            << "schedule_version=" << scheduleVersion(options.profile) << '\n'
            << "profile=" << profileName(options.profile) << '\n'
            << "build_configuration=" << buildConfiguration() << '\n'
            << "seed=" << options.seed << '\n'
            << "difficulty_profile_version="
            << finalStats.difficultyProfileVersion << '\n'
            << "difficulty_id="
            << static_cast<int>(finalStats.difficulty) << '\n'
            << "post_victory_event_version="
            << finalStats.postVictoryEventVersion << '\n'
            << "post_victory_completed_events="
            << finalStats.completedPostVictoryEvents << '\n'
            << "post_victory_active_event="
            << finalStats.activePostVictoryEvent << '\n'
            << "post_victory_event_wave="
            << finalStats.postVictoryEventWave << '\n'
            << "duration_requested_seconds=" << options.durationSeconds
            << '\n'
            << "duration_completed_seconds="
            << completedTicks / FixedTicksPerSecond << '\n'
            << "fixed_ticks=" << completedTicks << '\n'
            << "failures=" << failures << '\n'
            << "movement_actions=" << movements << '\n'
            << "block_edit_actions=" << edits << '\n'
            << "actor_lifecycle_actions=" << actorCycles << '\n'
            << "save_reload_actions=" << reloads << '\n'
            << "max_existing_chunks=" << maxExistingChunks << '\n'
            << "max_loaded_chunks=" << maxLoadedChunks << '\n'
            << "max_queued_chunk_updates=" << maxQueuedUpdates << '\n'
            << "max_actor_count=" << maxActors << '\n'
            << "max_mesh_dirty_sections=" << maxDirtySections << '\n'
            << "final_existing_chunks="
            << finalStats.chunks.existingChunks << '\n'
            << "final_loaded_chunks=" << finalStats.chunks.loadedChunks
            << '\n'
            << "final_sections=" << finalStats.chunks.sections << '\n'
            << "final_mesh_dirty_sections="
            << finalStats.chunks.meshDirtySections << '\n'
            << "final_queued_chunk_updates="
            << finalStats.queuedChunkUpdates << '\n'
            << "final_mesh_rebuilds=" << finalStats.chunks.meshRebuilds
            << '\n'
            << "final_actor_count=" << finalStats.actorCount << '\n'
            << "final_random_tick_sections="
            << finalStats.randomTickSections << '\n'
            << "final_random_tick_blocks="
            << finalStats.randomTickBlocks << '\n';
}

} // namespace

int main(int argc, char **argv)
{
    struct FreeImageScope {
        FreeImageScope() { FreeImage_Initialise(FALSE); }
        ~FreeImageScope() { FreeImage_DeInitialise(); }
    } freeImageScope;

    Options options;
    std::ofstream snapshots;
    int failures = 0;
    int completedTicks = 0;
    int movementActions = 0;
    int blockEditActions = 0;
    int actorLifecycleActions = 0;
    int saveReloadActions = 0;
    std::size_t maxExistingChunks = 0;
    std::size_t maxLoadedChunks = 0;
    std::size_t maxQueuedUpdates = 0;
    std::size_t maxActors = 0;
    std::size_t maxDirtySections = 0;
    WorldDebugStats finalStats;

    try {
        options = parseOptions(argc, argv);
        options.outputDirectory =
            std::filesystem::absolute(options.outputDirectory);
        if (!options.explicitOutputDirectory) {
            std::error_code removeError;
            std::filesystem::remove_all(options.outputDirectory,
                                        removeError);
        }
        std::filesystem::create_directories(options.outputDirectory);
        const std::filesystem::path saveDirectory =
            options.outputDirectory / "save";

        snapshots.open(options.outputDirectory / "world-snapshots.csv",
                       std::ios::trunc);
        if (!snapshots) {
            throw std::runtime_error("Unable to create world snapshot CSV.");
        }
        snapshots
            << "elapsed_seconds,tick,existing_chunks,loaded_chunks,sections,"
               "mesh_dirty_sections,cpu_ready_sections,gpu_buffered_sections,"
               "queued_chunk_updates,mesh_rebuilds,actors,natural_mobs,"
               "random_tick_sections,random_tick_blocks,random_dispatches\n";

        Config config;
        config.renderDistance =
            options.profile == SoakProfile::Stress ? 2 : 1;
        config.worldSeed = options.seed;
        Player player;
        Camera camera(config);
        camera.hookEntity(player);
        player.position = {8.f, 90.f, 8.f};
        player.box.update(player.position);
        camera.update();

        setEnvironment("HELLOMINE3D_SEED", std::to_string(options.seed));
        setEnvironment("HELLOMINE3D_PLAYER_POSITION", "8 90 8");

        auto makeWorld = [&]() {
            auto created = std::make_unique<World>(
                camera, config, player, saveDirectory.string(), true, 1);
            camera.update();
            created->update(camera);
            return created;
        };

        std::unique_ptr<World> world = makeWorld();
        setEnvironment("HELLOMINE3D_PLAYER_POSITION", "");

        const int travelRadius =
            options.profile == SoakProfile::Stress ? 8 : 4;
        const std::array<VectorXZ, 9> centers = {
            VectorXZ{0, 0},
            VectorXZ{travelRadius, 0},
            VectorXZ{travelRadius, travelRadius},
            VectorXZ{0, travelRadius},
            VectorXZ{-travelRadius, travelRadius},
            VectorXZ{-travelRadius, 0},
            VectorXZ{-travelRadius, -travelRadius},
            VectorXZ{0, -travelRadius},
            VectorXZ{travelRadius, -travelRadius},
        };

        const int totalTicks =
            options.durationSeconds * FixedTicksPerSecond;
        const int movementInterval = options.profile == SoakProfile::Legacy
            ? std::max(10, std::min(100, totalTicks / 4))
            : (options.profile == SoakProfile::Stress ? 40 : 100);
        const int reloadInterval = options.profile == SoakProfile::Legacy
            ? std::max(20, std::min(200, totalTicks / 2))
            : (options.profile == SoakProfile::Stress ? 100 : 200);
        std::size_t centerIndex = 0;
        std::size_t previousRebuilds = 0;
        int stalledSnapshots = 0;
        const auto started = std::chrono::steady_clock::now();
        auto nextTick = started;

        const auto fail = [&](const std::string &reason) {
            ++failures;
            std::cerr << "[SOAK] FAIL " << reason << '\n';
        };

        for (int tick = 1; tick <= totalTicks; ++tick) {
            nextTick += std::chrono::milliseconds(50);

            if (tick == 1 || tick % movementInterval == 0) {
                const VectorXZ center = centers[centerIndex % centers.size()];
                ++centerIndex;
                player.position = {
                    static_cast<float>(center.x * CHUNK_SIZE + CHUNK_SIZE / 2),
                    90.f,
                    static_cast<float>(center.z * CHUNK_SIZE + CHUNK_SIZE / 2),
                };
                player.velocity = glm::vec3(0.f);
                player.box.update(player.position);
                camera.update();
                world->preloadAround(player.position);
                ++movementActions;
            }

            const int editInterval =
                options.profile == SoakProfile::Stress
                    ? FixedTicksPerSecond / 4
                    : FixedTicksPerSecond;
            if (tick % editInterval == 0) {
                const int x = World::toBlockCoord(player.position.x) +
                              (blockEditActions % 4);
                const int z = World::toBlockCoord(player.position.z) + 2;
                const BlockId replacement =
                    blockEditActions % 2 == 0 ? BlockId::Stone
                                              : BlockId::Air;
                world->setBlock(x, 100, z, replacement);
                ++blockEditActions;
            }

            const int actorInterval =
                options.profile == SoakProfile::Stress
                    ? FixedTicksPerSecond
                    : FixedTicksPerSecond * 2;
            if (tick % actorInterval == 0) {
                world->getActorManager().removeActorsIf(
                    [](const Actor &actor) {
                        return actor.getType() == "hellomine:soak_mob" ||
                               actor.getType() == "item";
                    });
                const int actorPairs =
                    options.profile == SoakProfile::Stress ? 8 : 1;
                for (int actor = 0; actor < actorPairs; ++actor) {
                    const glm::vec3 offset(
                        3.f + static_cast<float>(actor % 4), 0.f,
                        static_cast<float>(actor / 4) * 2.f);
                    const ActorId mobId = world->spawnMob(
                        "hellomine:soak_mob", player.position + offset);
                    if (mobId == InvalidActorId ||
                        !world->attackActor(mobId, 100.f)) {
                        fail("actor damage lifecycle rejected");
                    }
                    if (world->spawnItemEntity(
                            Material::ID::Stone, 1,
                            player.position + offset +
                                glm::vec3(0.f, 1.f, 0.f)) ==
                        InvalidActorId) {
                        fail("item lifecycle spawn rejected");
                    }
                }
                ++actorLifecycleActions;
            }

            world->tick(tick);
            camera.update();
            world->update(camera);

            const WorldMeshSnapshot meshSnapshot =
                world->collectSectionMeshSnapshot();
            std::vector<WorldSectionMeshVersion> uploaded;
            uploaded.reserve(meshSnapshot.cpuReadySections.size());
            for (const auto &section : meshSnapshot.cpuReadySections) {
                uploaded.push_back(
                    {section.location, section.blockRevision});
            }
            world->acknowledgeSectionMeshUploads(uploaded);

            // The final save below is the terminal persistence check. Avoid
            // creating a fresh loader on the last tick only to destroy it
            // immediately, which turns shutdown timing into the test subject.
            if (tick % reloadInterval == 0 && tick != totalTicks) {
                const glm::ivec3 marker{
                    World::toBlockCoord(player.position.x) + 5, 110,
                    World::toBlockCoord(player.position.z) + 5};
                const BlockId markerId =
                    saveReloadActions % 2 == 0 ? BlockId::Stone
                                               : BlockId::Dirt;
                world->setBlock(marker.x, marker.y, marker.z, markerId);

                world->getActorManager().removeActorsIf(
                    [](const Actor &actor) {
                        return actor.getType() == "hellomine:soak_persisted" ||
                               actor.getType() == "item";
                    });
                world->spawnMob("hellomine:soak_persisted",
                                player.position + glm::vec3(4.f, 0.f, 0.f));
                const ActorId itemId = world->spawnItemEntity(
                    Material::ID::Dirt, 1,
                    player.position + glm::vec3(4.f, 2.f, 0.f));
                auto *item = dynamic_cast<ItemEntity *>(
                    world->getActorManager().findActor(itemId));
                if (item != nullptr) {
                    item->setPickupDelay(60.f);
                }

                if (!world->save()) {
                    fail("world save failed");
                }
                const int expectedSeed =
                    world->collectDebugStats().terrainSeed;
                world.reset();
                world = makeWorld();

                if (world->collectDebugStats().terrainSeed != expectedSeed) {
                    fail("terrain seed changed after reload");
                }
                if (world->getBlock(marker.x, marker.y, marker.z).id !=
                    static_cast<Block_t>(markerId)) {
                    fail("edited block did not survive reload");
                }
                if (world->getActorManager().countActorsByType(
                        "hellomine:soak_persisted") != 1 ||
                    world->getActorManager().countActorsByType("item") < 1) {
                    fail("actor or item did not survive reload");
                }
                world->getActorManager().removeActorsIf(
                    [](const Actor &actor) {
                        return actor.getType() == "hellomine:soak_persisted" ||
                               actor.getType() == "item";
                    });
                previousRebuilds = 0;
                stalledSnapshots = 0;
                ++saveReloadActions;
            }

            if (tick % FixedTicksPerSecond == 0 || tick == totalTicks) {
                const WorldDebugStats stats = world->collectDebugStats();
                const std::size_t classifiedSections =
                    stats.chunks.meshCleanSections +
                    stats.chunks.meshDirtySections +
                    stats.chunks.meshQueuedSections +
                    stats.chunks.meshBuildingSections +
                    stats.chunks.cpuReadySections;
                if (stats.chunks.loadedChunks > stats.chunks.existingChunks ||
                    classifiedSections != stats.chunks.sections) {
                    fail("chunk/section accounting invariant failed");
                }
                if (stats.queuedChunkUpdates > 4096) {
                    fail("dirty update queue exceeded 4096 entries");
                }
                if (stats.actorCount > 32) {
                    fail("actor count exceeded 32");
                }
                if (stats.chunks.existingChunks > 1024) {
                    fail("existing chunk count exceeded 1024");
                }

                const bool workPending =
                    stats.chunks.meshDirtySections > 0 ||
                    stats.queuedChunkUpdates > 0;
                if (workPending &&
                    stats.chunks.meshRebuilds == previousRebuilds) {
                    ++stalledSnapshots;
                }
                else {
                    stalledSnapshots = 0;
                }
                if (stalledSnapshots > 60) {
                    fail("mesh progress stalled for more than 60 seconds");
                    stalledSnapshots = 0;
                }
                previousRebuilds = stats.chunks.meshRebuilds;

                maxExistingChunks = std::max(
                    maxExistingChunks, stats.chunks.existingChunks);
                maxLoadedChunks = std::max(
                    maxLoadedChunks, stats.chunks.loadedChunks);
                maxQueuedUpdates = std::max(
                    maxQueuedUpdates, stats.queuedChunkUpdates);
                maxActors = std::max(maxActors, stats.actorCount);
                maxDirtySections = std::max(
                    maxDirtySections, stats.chunks.meshDirtySections);

                snapshots << tick / FixedTicksPerSecond << ',' << tick << ','
                          << stats.chunks.existingChunks << ','
                          << stats.chunks.loadedChunks << ','
                          << stats.chunks.sections << ','
                          << stats.chunks.meshDirtySections << ','
                          << stats.chunks.cpuReadySections << ','
                          << stats.chunks.gpuBufferedSections << ','
                          << stats.queuedChunkUpdates << ','
                          << stats.chunks.meshRebuilds << ','
                          << stats.actorCount << ','
                          << stats.naturalMobCount << ','
                          << stats.randomTickSections << ','
                          << stats.randomTickBlocks << ','
                          << stats.randomTicksDispatched << '\n';
                snapshots.flush();

                if (tick % (FixedTicksPerSecond * 60) == 0 ||
                    tick == totalTicks) {
                    std::cout << "[SOAK] elapsed_seconds="
                              << tick / FixedTicksPerSecond
                              << " chunks=" << stats.chunks.existingChunks
                              << '/' << stats.chunks.loadedChunks
                              << " dirty="
                              << stats.chunks.meshDirtySections
                              << " queue=" << stats.queuedChunkUpdates
                              << " actors=" << stats.actorCount
                              << " reloads=" << saveReloadActions << '\n';
                }
            }

            completedTicks = tick;
            std::this_thread::sleep_until(nextTick);
        }

        if (!world->save()) {
            fail("final world save failed");
        }
        finalStats = world->collectDebugStats();
        world.reset();

        const double elapsedSeconds =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started)
                .count();
        if (elapsedSeconds + 0.1 < options.durationSeconds) {
            fail("wall-clock duration ended early");
        }

        writeSummary(options.outputDirectory / "summary.txt", options,
                     completedTicks, failures, movementActions,
                     blockEditActions, actorLifecycleActions,
                     saveReloadActions, finalStats, maxExistingChunks,
                     maxLoadedChunks, maxQueuedUpdates, maxActors,
                     maxDirtySections);
    }
    catch (const std::exception &error) {
        ++failures;
        std::cerr << "[SOAK] FAIL unhandled exception: " << error.what()
                  << '\n';
        if (!options.outputDirectory.empty()) {
            try {
                std::filesystem::create_directories(
                    options.outputDirectory);
                writeSummary(options.outputDirectory / "summary.txt", options,
                             completedTicks, failures, movementActions,
                             blockEditActions, actorLifecycleActions,
                             saveReloadActions, finalStats,
                             maxExistingChunks, maxLoadedChunks,
                             maxQueuedUpdates, maxActors,
                             maxDirtySections);
            }
            catch (...) {
            }
        }
    }

    setEnvironment("HELLOMINE3D_SEED", "");
    setEnvironment("HELLOMINE3D_PLAYER_POSITION", "");
    std::cout << "[SOAK] status=" << (failures == 0 ? "PASS" : "FAIL")
              << " duration_seconds=" << completedTicks / FixedTicksPerSecond
              << " failures=" << failures << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
