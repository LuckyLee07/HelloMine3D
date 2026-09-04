// Deterministic long-running world soak for R2 and Architecture Lab B10.
//
// The executable uses the real World, background loader, mesh state machine,
// actor lifecycle and persistence paths. The wrapper samples operating-system
// memory and handle counters while this process records bounded world stats.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
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
    TrackBCore,
};

enum class TrackBPhase : std::size_t {
    StraightRun = 0,
    TeleportStorm,
    Turnaround,
    RenderDistanceChurn,
    EditAndLeave,
    Count,
};

constexpr std::size_t TrackBPhaseCount =
    static_cast<std::size_t>(TrackBPhase::Count);

struct TrackBSoakStats {
    std::array<int, TrackBPhaseCount> phaseTicks{};
    std::array<int, TrackBPhaseCount> phaseMovements{};
    int renderDistanceChanges = 0;
    int persistenceChecks = 0;
    std::uint64_t scheduleDigest = 1469598103934665603ULL;
    std::uint64_t persistenceDigest = 1469598103934665603ULL;
    std::size_t maxPendingJobs = 0;
    std::size_t maxPendingGenerationJobs = 0;
    std::size_t maxPendingMeshJobs = 0;
    std::size_t maxInFlightJobs = 0;
    std::size_t maxCompletedResults = 0;
    std::size_t maxDeferredPlanJobs = 0;
    std::size_t maxAuthoritativeCommits = 0;
    std::size_t maxSectionUploads = 0;
    std::size_t maxUnloads = 0;
    std::size_t maxAbsentChunks = 0;
    std::size_t maxResidentCells = 0;
    std::size_t maxNearCells = 0;
    std::size_t maxSimulationCells = 0;
    double maxQueueLatencyMilliseconds = 0.0;
    double maxWorkerMilliseconds = 0.0;
    double maxCommitMilliseconds = 0.0;
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
    case SoakProfile::TrackBCore: return "track-b-core";
    }
    return "unknown";
}

int scheduleVersion(SoakProfile profile)
{
    if (profile == SoakProfile::Legacy) {
        return 1;
    }
    return profile == SoakProfile::TrackBCore ? 3 : 2;
}

TrackBPhase trackBPhaseForTick(int tick, int totalTicks)
{
    const int zeroBasedTick = tick - 1;
    if (zeroBasedTick < totalTicks / 3) {
        return TrackBPhase::StraightRun;
    }
    if (zeroBasedTick < totalTicks / 2) {
        return TrackBPhase::TeleportStorm;
    }
    if (zeroBasedTick < (totalTicks * 2) / 3) {
        return TrackBPhase::Turnaround;
    }
    if (zeroBasedTick < (totalTicks * 5) / 6) {
        return TrackBPhase::RenderDistanceChurn;
    }
    return TrackBPhase::EditAndLeave;
}

const char *trackBPhaseName(TrackBPhase phase)
{
    switch (phase) {
    case TrackBPhase::StraightRun: return "lw1_straight_run";
    case TrackBPhase::TeleportStorm: return "lw2_teleport_storm";
    case TrackBPhase::Turnaround: return "lw3_turnaround";
    case TrackBPhase::RenderDistanceChurn:
        return "lw4_render_distance_churn";
    case TrackBPhase::EditAndLeave: return "lw5_edit_and_leave";
    case TrackBPhase::Count: break;
    }
    return "unknown";
}

void mixDigest(std::uint64_t &digest, std::uint64_t value)
{
    for (int byte = 0; byte < 8; ++byte) {
        digest ^= (value >> (byte * 8)) & 0xffULL;
        digest *= 1099511628211ULL;
    }
}

std::string digestText(std::uint64_t digest)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << digest;
    return stream.str();
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
            else if (profile == "track-b-core") {
                options.profile = SoakProfile::TrackBCore;
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
                  std::size_t maxDirtySections,
                  const TrackBSoakStats &trackB)
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
            << "track_b_core="
            << (options.profile == SoakProfile::TrackBCore ? "true" : "false")
            << '\n'
            << "track_b_schedule_digest="
            << digestText(trackB.scheduleDigest) << '\n'
            << "track_b_persistence_digest="
            << digestText(trackB.persistenceDigest) << '\n'
            << "track_b_render_distance_changes="
            << trackB.renderDistanceChanges << '\n'
            << "track_b_persistence_checks="
            << trackB.persistenceChecks << '\n'
            << "track_b_max_pending_jobs=" << trackB.maxPendingJobs << '\n'
            << "track_b_max_pending_generation_jobs="
            << trackB.maxPendingGenerationJobs << '\n'
            << "track_b_max_pending_mesh_jobs="
            << trackB.maxPendingMeshJobs << '\n'
            << "track_b_max_in_flight_jobs=" << trackB.maxInFlightJobs << '\n'
            << "track_b_max_completed_results="
            << trackB.maxCompletedResults << '\n'
            << "track_b_max_deferred_plan_jobs="
            << trackB.maxDeferredPlanJobs << '\n'
            << "track_b_max_authoritative_commits="
            << trackB.maxAuthoritativeCommits << '\n'
            << "track_b_max_section_uploads="
            << trackB.maxSectionUploads << '\n'
            << "track_b_max_unloads=" << trackB.maxUnloads << '\n'
            << "track_b_max_absent_chunks=" << trackB.maxAbsentChunks << '\n'
            << "track_b_max_resident_cells="
            << trackB.maxResidentCells << '\n'
            << "track_b_max_near_cells=" << trackB.maxNearCells << '\n'
            << "track_b_max_simulation_cells="
            << trackB.maxSimulationCells << '\n'
            << "track_b_max_queue_latency_ms="
            << trackB.maxQueueLatencyMilliseconds << '\n'
            << "track_b_max_worker_ms="
            << trackB.maxWorkerMilliseconds << '\n'
            << "track_b_max_commit_ms="
            << trackB.maxCommitMilliseconds << '\n'
            << "track_b_cancelled_jobs="
            << finalStats.worldJobs.cancelledJobs << '\n'
            << "track_b_commit_rejected_jobs="
            << finalStats.worldJobs.commitRejectedJobs << '\n'
            << "track_b_stale_submit_rejections="
            << finalStats.worldJobs.staleSubmitRejections << '\n'
            << "track_b_stale_plan_rejections="
            << finalStats.worldJobs.stalePlanRejections << '\n'
            << "track_b_generation_invalidations="
            << finalStats.worldJobs.generationInvalidations << '\n'
            << "final_existing_chunks="
            << finalStats.chunks.existingChunks << '\n'
            << "final_loaded_chunks=" << finalStats.chunks.loadedChunks
            << '\n'
            << "final_data_absent_chunks="
            << finalStats.chunks.dataAbsentChunks << '\n'
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
    for (std::size_t phase = 0; phase < TrackBPhaseCount; ++phase) {
        const TrackBPhase typedPhase = static_cast<TrackBPhase>(phase);
        summary << trackBPhaseName(typedPhase) << "_ticks="
                << trackB.phaseTicks[phase] << '\n'
                << trackBPhaseName(typedPhase) << "_movements="
                << trackB.phaseMovements[phase] << '\n';
    }
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
    TrackBSoakStats trackB;
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
            << "elapsed_seconds,tick,existing_chunks,loaded_chunks,"
               "data_absent_chunks,data_requested_chunks,data_loading_chunks,"
               "data_generating_chunks,data_resident_chunks,"
               "data_evict_requested_chunks,data_saving_chunks,sections,"
               "mesh_dirty_sections,cpu_ready_sections,gpu_buffered_sections,"
               "queued_chunk_updates,mesh_rebuilds,actors,natural_mobs,"
               "random_tick_sections,random_tick_blocks,random_dispatches,"
               "job_pending,job_in_flight,job_completed,deferred_plan,"
               "cancelled_jobs,commit_rejected_jobs,spatial_resident,"
               "spatial_near,spatial_simulation,commits,uploads,unloads\n";

        Config config;
        config.renderDistance =
            options.profile == SoakProfile::Stress ||
                    options.profile == SoakProfile::TrackBCore
                ? 2
                : 1;
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
            options.profile == SoakProfile::Stress ||
                    options.profile == SoakProfile::TrackBCore
                ? 8
                : 4;
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
            : (options.profile == SoakProfile::Nominal ? 100 : 40);
        const int reloadInterval = options.profile == SoakProfile::Legacy
            ? std::max(20, std::min(200, totalTicks / 2))
            : (options.profile == SoakProfile::Nominal ? 200 : 100);
        // Track B's formal LW5 is six times longer than the 300-second
        // developer schedule. Scaling a five-second persistence cadence with
        // duration would turn ten coverage checks into sixty full save/reopen
        // cycles and make backup-copy amplification, rather than streaming,
        // dominate the wall-clock contract. Keep the short schedule's ten
        // checks as the minimum and spread the same bounded coverage over a
        // longer LW5 interval.
        const int trackBReloadInterval = std::max(
            reloadInterval, (totalTicks / 6) / 10);
        std::size_t centerIndex = 0;
        TrackBPhase previousTrackBPhase = TrackBPhase::Count;
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

            const bool isTrackBCore =
                options.profile == SoakProfile::TrackBCore;
            const TrackBPhase trackBPhase = isTrackBCore
                ? trackBPhaseForTick(tick, totalTicks)
                : TrackBPhase::Count;
            const bool trackBPhaseChanged =
                isTrackBCore && trackBPhase != previousTrackBPhase;
            if (isTrackBCore) {
                const std::size_t phase =
                    static_cast<std::size_t>(trackBPhase);
                ++trackB.phaseTicks[phase];
                if (trackBPhaseChanged) {
                    mixDigest(trackB.scheduleDigest, 0xB100ULL + phase);
                }
            }

            if (tick == 1 || tick % movementInterval == 0 ||
                trackBPhaseChanged) {
                VectorXZ center{0, 0};
                if (!isTrackBCore) {
                    center = centers[centerIndex % centers.size()];
                }
                else {
                    const std::size_t phase =
                        static_cast<std::size_t>(trackBPhase);
                    const int phaseAction =
                        trackB.phaseMovements[phase];
                    switch (trackBPhase) {
                    case TrackBPhase::StraightRun:
                        // One Chunk every two seconds for the formal ten-
                        // minute LW1 interval: an unbounded path with bounded
                        // residency behind it.
                        center = VectorXZ{phaseAction, 0};
                        break;
                    case TrackBPhase::TeleportStorm: {
                        // Deterministic pseudo-random far destinations. The
                        // prime multipliers deliberately avoid a short axis
                        // cycle while keeping generated files bounded.
                        const int salt = options.seed % 257;
                        center = VectorXZ{
                            ((phaseAction * 97 + salt) % 257) - 128,
                            ((phaseAction * 193 + salt * 3) % 257) - 128};
                        break;
                    }
                    case TrackBPhase::Turnaround:
                        center = VectorXZ{
                            phaseAction % 2 == 0 ? 12 : -12, 0};
                        break;
                    case TrackBPhase::RenderDistanceChurn:
                        center = centers[phaseAction % centers.size()];
                        break;
                    case TrackBPhase::EditAndLeave:
                        center = VectorXZ{phaseAction * 16, 16};
                        break;
                    case TrackBPhase::Count:
                        break;
                    }
                    ++trackB.phaseMovements[phase];
                    mixDigest(trackB.scheduleDigest,
                              0xB200ULL + phase);
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(center.x));
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(center.z));
                }
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

            if (isTrackBCore &&
                trackBPhase == TrackBPhase::RenderDistanceChurn &&
                (trackBPhaseChanged || tick % (FixedTicksPerSecond * 5) == 0)) {
                static const std::array<int, 4> distances = {1, 2, 4, 8};
                const int distance = distances[
                    trackB.renderDistanceChanges % distances.size()];
                config.renderDistance = distance;
                world->setRenderDistance(distance);
                ++trackB.renderDistanceChanges;
                mixDigest(trackB.scheduleDigest, 0xB300ULL);
                mixDigest(trackB.scheduleDigest,
                          static_cast<std::uint64_t>(distance));
            }

            const int editInterval =
                options.profile == SoakProfile::Stress || isTrackBCore
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
                if (isTrackBCore) {
                    mixDigest(trackB.scheduleDigest, 0xB400ULL);
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(x));
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(z));
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint64_t>(replacement));
                }
            }

            const int actorInterval =
                options.profile == SoakProfile::Stress || isTrackBCore
                    ? FixedTicksPerSecond
                    : FixedTicksPerSecond * 2;
            if (tick % actorInterval == 0) {
                world->getActorManager().removeActorsIf(
                    [](const Actor &actor) {
                        return actor.getType() == "hellomine:soak_mob" ||
                               actor.getType() == "item";
                    });
                const int actorPairs =
                    options.profile == SoakProfile::Stress || isTrackBCore
                        ? 8
                        : 1;
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

            previousTrackBPhase = trackBPhase;

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
            const bool shouldReload = isTrackBCore
                ? (trackBPhase == TrackBPhase::EditAndLeave &&
                   (trackBPhaseChanged ||
                    tick % trackBReloadInterval == 0))
                : tick % reloadInterval == 0;
            if (shouldReload && tick != totalTicks) {
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
                const glm::vec3 markerPlayerPosition = player.position;
                if (isTrackBCore) {
                    // LW5 deliberately invalidates the current demand after
                    // the save, then reopens at the edited Chunk to prove the
                    // just-left authoritative state is recoverable.
                    player.position.x += CHUNK_SIZE * 32.f;
                    player.position.z += CHUNK_SIZE * 32.f;
                    player.box.update(player.position);
                    camera.update();
                    world->preloadAround(player.position);
                    world->update(camera);
                    mixDigest(trackB.scheduleDigest, 0xB500ULL);
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(marker.x));
                    mixDigest(trackB.scheduleDigest,
                              static_cast<std::uint32_t>(marker.z));
                    player.position = markerPlayerPosition;
                    player.box.update(player.position);
                    camera.update();
                }
                world.reset();
                world = makeWorld();

                if (world->collectDebugStats().terrainSeed != expectedSeed) {
                    fail("terrain seed changed after reload");
                }
                if (world->getBlock(marker.x, marker.y, marker.z).id !=
                    static_cast<Block_t>(markerId)) {
                    fail("edited block did not survive reload");
                }
                else if (isTrackBCore) {
                    ++trackB.persistenceChecks;
                    mixDigest(trackB.persistenceDigest,
                              static_cast<std::uint32_t>(marker.x));
                    mixDigest(trackB.persistenceDigest,
                              static_cast<std::uint32_t>(marker.y));
                    mixDigest(trackB.persistenceDigest,
                              static_cast<std::uint32_t>(marker.z));
                    mixDigest(trackB.persistenceDigest,
                              static_cast<std::uint64_t>(markerId));
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
                const std::size_t classifiedDataChunks =
                    stats.chunks.dataAbsentChunks +
                    stats.chunks.dataRequestedChunks +
                    stats.chunks.dataLoadingChunks +
                    stats.chunks.dataGeneratingChunks +
                    stats.chunks.dataResidentChunks +
                    stats.chunks.dataEvictRequestedChunks +
                    stats.chunks.dataSavingChunks;
                if (classifiedDataChunks != stats.chunks.existingChunks ||
                    stats.chunks.dataAbsentChunks != 0) {
                    fail("Chunk data-state accounting retained Absent entries");
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
                if (stats.worldJobs.pendingJobs >
                        WorldJobScheduler::MaxPendingJobs ||
                    stats.worldJobs.pendingGenerationJobs >
                        WorldJobScheduler::MaxPendingGenerationJobs ||
                    stats.worldJobs.pendingMeshJobs >
                        WorldJobScheduler::MaxPendingMeshJobs ||
                    stats.worldJobs.pendingJobs !=
                        stats.worldJobs.pendingGenerationJobs +
                            stats.worldJobs.pendingMeshJobs) {
                    fail("Track B scheduler queue bound/accounting failed");
                }
                if (stats.worldJobs.inFlightJobs > 1 ||
                    stats.worldJobs.completedResults > 1) {
                    fail("single-worker job lifecycle bound failed");
                }
                if (stats.streamingBackpressure.lastAuthoritativeCommits >
                        ChunkRuntime::MaxAuthoritativeCommitsPerPass ||
                    stats.streamingBackpressure.lastSectionUploadsOffered >
                        ChunkRuntime::MaxSectionUploadsPerFrame ||
                    stats.streamingBackpressure.lastUnloads >
                        ChunkRuntime::MaxUnloadsPerUpdate) {
                    fail("Track B consumer budget failed");
                }
                if (stats.spatialInterest.simulationRequestedCells >
                        stats.spatialInterest.nearRepresentationCells ||
                    stats.spatialInterest.nearRepresentationCells >
                        stats.spatialInterest.residentDataCells ||
                    stats.spatialInterest.residentDataCells !=
                        stats.spatialInterest.totalCells) {
                    fail("Track B spatial-interest hierarchy failed");
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
                trackB.maxPendingJobs = std::max(
                    trackB.maxPendingJobs, stats.worldJobs.pendingJobs);
                trackB.maxPendingGenerationJobs = std::max(
                    trackB.maxPendingGenerationJobs,
                    stats.worldJobs.pendingGenerationJobs);
                trackB.maxPendingMeshJobs = std::max(
                    trackB.maxPendingMeshJobs,
                    stats.worldJobs.pendingMeshJobs);
                trackB.maxInFlightJobs = std::max(
                    trackB.maxInFlightJobs, stats.worldJobs.inFlightJobs);
                trackB.maxCompletedResults = std::max(
                    trackB.maxCompletedResults,
                    stats.worldJobs.completedResults);
                trackB.maxDeferredPlanJobs = std::max(
                    trackB.maxDeferredPlanJobs,
                    stats.streamingBackpressure.deferredPlanJobs);
                trackB.maxAuthoritativeCommits = std::max(
                    trackB.maxAuthoritativeCommits,
                    stats.streamingBackpressure.lastAuthoritativeCommits);
                trackB.maxSectionUploads = std::max(
                    trackB.maxSectionUploads,
                    stats.streamingBackpressure.lastSectionUploadsOffered);
                trackB.maxUnloads = std::max(
                    trackB.maxUnloads,
                    stats.streamingBackpressure.lastUnloads);
                trackB.maxAbsentChunks = std::max(
                    trackB.maxAbsentChunks, stats.chunks.dataAbsentChunks);
                trackB.maxResidentCells = std::max(
                    trackB.maxResidentCells,
                    stats.spatialInterest.residentDataCells);
                trackB.maxNearCells = std::max(
                    trackB.maxNearCells,
                    stats.spatialInterest.nearRepresentationCells);
                trackB.maxSimulationCells = std::max(
                    trackB.maxSimulationCells,
                    stats.spatialInterest.simulationRequestedCells);
                trackB.maxQueueLatencyMilliseconds = std::max(
                    trackB.maxQueueLatencyMilliseconds,
                    stats.worldJobs.lastQueueLatencyMilliseconds);
                trackB.maxWorkerMilliseconds = std::max(
                    trackB.maxWorkerMilliseconds,
                    stats.worldJobs.lastWorkerMilliseconds);
                trackB.maxCommitMilliseconds = std::max(
                    trackB.maxCommitMilliseconds,
                    stats.worldJobs.lastCommitMilliseconds);

                snapshots << tick / FixedTicksPerSecond << ',' << tick << ','
                          << stats.chunks.existingChunks << ','
                          << stats.chunks.loadedChunks << ','
                          << stats.chunks.dataAbsentChunks << ','
                          << stats.chunks.dataRequestedChunks << ','
                          << stats.chunks.dataLoadingChunks << ','
                          << stats.chunks.dataGeneratingChunks << ','
                          << stats.chunks.dataResidentChunks << ','
                          << stats.chunks.dataEvictRequestedChunks << ','
                          << stats.chunks.dataSavingChunks << ','
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
                          << stats.randomTicksDispatched << ','
                          << stats.worldJobs.pendingJobs << ','
                          << stats.worldJobs.inFlightJobs << ','
                          << stats.worldJobs.completedResults << ','
                          << stats.streamingBackpressure.deferredPlanJobs
                          << ',' << stats.worldJobs.cancelledJobs << ','
                          << stats.worldJobs.commitRejectedJobs << ','
                          << stats.spatialInterest.residentDataCells << ','
                          << stats.spatialInterest.nearRepresentationCells
                          << ','
                          << stats.spatialInterest.simulationRequestedCells
                          << ','
                          << stats.streamingBackpressure
                                 .lastAuthoritativeCommits
                          << ','
                          << stats.streamingBackpressure
                                 .lastSectionUploadsOffered
                          << ','
                          << stats.streamingBackpressure.lastUnloads << '\n';
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
                     maxDirtySections, trackB);
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
                             maxDirtySections, trackB);
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
