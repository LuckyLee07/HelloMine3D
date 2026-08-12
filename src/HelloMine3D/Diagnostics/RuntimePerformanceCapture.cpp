#include "RuntimePerformanceCapture.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace RuntimePerformanceCapture
{
namespace
{
    struct FrameSample
    {
        FrameTimings timings;
        WorldDebugStats world;
        double measuredElapsedMs = 0.0;
        std::size_t simulationTicks = 0;
    };

    struct CaptureState
    {
        bool initialized = false;
        bool enabled = false;
        bool exitWhenComplete = false;
        bool complete = false;
        bool summaryWritten = false;
        bool timingStarted = false;
        std::string outputDir;
        std::ofstream frames;
        std::vector<FrameSample> samples;
        std::chrono::steady_clock::time_point startTime;
        double warmupMs = 3000.0;
        double durationMs = 10000.0;
        std::size_t frameIndex = 0;
        std::size_t pendingSimulationTicks = 0;
        std::size_t capturedSimulationTicks = 0;
    };

    CaptureState &state()
    {
        static CaptureState captureState;
        return captureState;
    }

    bool isFalseEnvValue(const char *value)
    {
        if (value == nullptr || value[0] == '\0') {
            return false;
        }

        const std::string text(value);
        return text == "0" || text == "false" || text == "FALSE" ||
               text == "False" || text == "off" || text == "OFF";
    }

    bool isTrueEnvValue(const char *value)
    {
        return value != nullptr && value[0] != '\0' && !isFalseEnvValue(value);
    }

    std::string normalizeDirectory(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        while (path.size() > 1 && path[path.size() - 1] == '/') {
            path.erase(path.size() - 1);
        }
        return path;
    }

    bool createDirectoryIfMissing(const std::string &path)
    {
        if (path.empty() || (path.size() == 2 && path[1] == ':')) {
            return true;
        }

#if defined(_WIN32)
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    bool ensureDirectoryRecursive(const std::string &directory)
    {
        const std::string normalized = normalizeDirectory(directory);
        if (normalized.empty()) {
            return false;
        }

        std::size_t start = 0;
        if (normalized.size() >= 2 && normalized[1] == ':') {
            start = 2;
        }
        while (start < normalized.size() && normalized[start] == '/') {
            ++start;
        }

        std::size_t pos = start;
        while (pos <= normalized.size()) {
            const std::size_t slash = normalized.find('/', pos);
            const std::size_t end =
                slash == std::string::npos ? normalized.size() : slash;
            const std::string part = normalized.substr(0, end);
            if (!createDirectoryIfMissing(part)) {
                return false;
            }

            if (slash == std::string::npos) {
                break;
            }
            pos = slash + 1;
        }

        return true;
    }

    double parsePositiveDoubleEnv(const char *name, double fallback)
    {
        const char *value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return fallback;
        }

        const double parsed = std::atof(value);
        return parsed > 0.0 ? parsed : fallback;
    }

    std::string buildPath(const CaptureState &captureState,
                          const std::string &name)
    {
        return captureState.outputDir + "/" + name;
    }

    void writeCsvHeader(std::ofstream &output)
    {
        output
            << "frame_index,measured_elapsed_ms,dt_ms,event_ms,update_ms,"
               "render_ms,debug_gui_ms,render_capture_ms,display_ms,frame_ms,"
               "existing_chunks,loaded_chunks,save_dirty_chunks,sections,"
               "mesh_dirty_sections,cpu_ready_sections,gpu_buffered_sections,"
               "queued_chunk_updates,random_tick_sections,random_tick_blocks,"
               "random_tick_sections_processed,random_ticks_dispatched,"
               "mesh_rebuilds,mesh_build_total_ms,"
               "mesh_build_last_ms,mesh_build_max_ms,solid_faces,transparent_faces,"
               "water_faces,flora_faces,solid_vertices,transparent_vertices,"
               "water_vertices,flora_vertices,"
               "actor_count,terrain_seed,world_time,simulation_ticks\n";
    }

    void initialize()
    {
        CaptureState &captureState = state();
        if (captureState.initialized) {
            return;
        }

        captureState.initialized = true;
        captureState.enabled =
            isTrueEnvValue(std::getenv("HELLO_PERF_CAPTURE"));
        if (!captureState.enabled) {
            return;
        }

        const char *outputDir = std::getenv("HELLO_PERF_CAPTURE_DIR");
        if (outputDir == nullptr || outputDir[0] == '\0') {
            std::cerr << "[PerfCapture] disabled missing output dir\n";
            captureState.enabled = false;
            return;
        }

        captureState.outputDir = normalizeDirectory(outputDir);
        captureState.warmupMs = parsePositiveDoubleEnv(
            "HELLO_PERF_CAPTURE_WARMUP_MS", captureState.warmupMs);
        captureState.durationMs = parsePositiveDoubleEnv(
            "HELLO_PERF_CAPTURE_DURATION_MS", captureState.durationMs);
        captureState.exitWhenComplete =
            isTrueEnvValue(std::getenv("HELLO_PERF_CAPTURE_EXIT"));

        if (!ensureDirectoryRecursive(captureState.outputDir)) {
            std::cerr << "[PerfCapture] disabled cannot create output dir: "
                      << captureState.outputDir << "\n";
            captureState.enabled = false;
            return;
        }

        captureState.frames.open(buildPath(captureState, "frames.csv"),
                                 std::ios::out | std::ios::trunc);
        if (!captureState.frames) {
            std::cerr << "[PerfCapture] disabled cannot open frames.csv\n";
            captureState.enabled = false;
            return;
        }

        writeCsvHeader(captureState.frames);
        std::cout << "[PerfCapture] enabled dir=" << captureState.outputDir
                  << " warmupMs=" << captureState.warmupMs
                  << " durationMs=" << captureState.durationMs << "\n";
    }

    std::vector<double> collectMetric(
        const std::vector<FrameSample> &samples,
        double (*selector)(const FrameSample &))
    {
        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto &sample : samples) {
            values.push_back(selector(sample));
        }
        std::sort(values.begin(), values.end());
        return values;
    }

    double average(const std::vector<double> &values)
    {
        if (values.empty()) {
            return 0.0;
        }

        double total = 0.0;
        for (double value : values) {
            total += value;
        }
        return total / static_cast<double>(values.size());
    }

    double percentile(const std::vector<double> &values, double pct)
    {
        if (values.empty()) {
            return 0.0;
        }

        const double clamped = std::max(0.0, std::min(100.0, pct));
        const double scaled =
            (clamped / 100.0) * static_cast<double>(values.size() - 1);
        const std::size_t index =
            static_cast<std::size_t>(scaled + 0.999999);
        return values[std::min(index, values.size() - 1)];
    }

    void writeMetric(std::ofstream &output, const std::string &name,
                     const std::vector<double> &values)
    {
        output << name << "_avg_ms=" << average(values) << "\n";
        output << name << "_p50_ms=" << percentile(values, 50.0) << "\n";
        output << name << "_p90_ms=" << percentile(values, 90.0) << "\n";
        output << name << "_p95_ms=" << percentile(values, 95.0) << "\n";
        output << name << "_p99_ms=" << percentile(values, 99.0) << "\n";
        output << name << "_max_ms="
               << (values.empty() ? 0.0 : values.back()) << "\n";
    }

    void writeSummary()
    {
        CaptureState &captureState = state();
        if (!captureState.enabled || captureState.summaryWritten) {
            return;
        }
        captureState.summaryWritten = true;

        if (captureState.frames.is_open()) {
            captureState.frames.flush();
        }

        const std::string summaryPath = buildPath(captureState, "summary.txt");
        std::ofstream summary(summaryPath, std::ios::out | std::ios::trunc);
        if (!summary) {
            std::cerr << "[PerfCapture] failed summary path=" << summaryPath
                      << "\n";
            return;
        }

        summary << std::fixed << std::setprecision(3);
        summary << "frames=" << captureState.samples.size() << "\n";
        summary << "warmup_ms=" << captureState.warmupMs << "\n";
        summary << "duration_ms=" << captureState.durationMs << "\n";
        summary << "sampled_fps="
                << (captureState.durationMs > 0.0
                        ? static_cast<double>(captureState.samples.size()) *
                              1000.0 / captureState.durationMs
                        : 0.0)
                << "\n";

        auto frameMs = collectMetric(captureState.samples,
                                     [](const FrameSample &sample) {
                                         return sample.timings.frameMs;
                                     });
        auto deltaMs = collectMetric(captureState.samples,
                                     [](const FrameSample &sample) {
                                         return sample.timings.deltaMs;
                                     });
        auto eventMs = collectMetric(captureState.samples,
                                     [](const FrameSample &sample) {
                                         return sample.timings.eventMs;
                                     });
        auto updateMs = collectMetric(captureState.samples,
                                      [](const FrameSample &sample) {
                                          return sample.timings.updateMs;
                                      });
        auto renderMs = collectMetric(captureState.samples,
                                      [](const FrameSample &sample) {
                                          return sample.timings.renderMs;
                                      });
        auto displayMs = collectMetric(captureState.samples,
                                       [](const FrameSample &sample) {
                                           return sample.timings.displayMs;
                                       });

        writeMetric(summary, "frame", frameMs);
        writeMetric(summary, "delta", deltaMs);
        writeMetric(summary, "event", eventMs);
        writeMetric(summary, "update", updateMs);
        writeMetric(summary, "render", renderMs);
        writeMetric(summary, "display", displayMs);

        const double avgFrame = average(frameMs);
        summary << "avg_fps=" << (avgFrame > 0.0 ? 1000.0 / avgFrame : 0.0)
                << "\n";

        std::size_t framesOver33 = 0;
        std::size_t framesOver50 = 0;
        for (double value : frameMs) {
            if (value > 33.333) {
                ++framesOver33;
            }
            if (value > 50.0) {
                ++framesOver50;
            }
        }
        summary << "frames_over_33ms=" << framesOver33 << "\n";
        summary << "frames_over_50ms=" << framesOver50 << "\n";
        summary << "simulation_ticks="
                << captureState.capturedSimulationTicks << "\n";
        summary << "simulation_tick_hz="
                << (captureState.durationMs > 0.0
                        ? static_cast<double>(
                              captureState.capturedSimulationTicks) *
                              1000.0 / captureState.durationMs
                        : 0.0)
                << "\n";

        if (!captureState.samples.empty()) {
            const WorldDebugStats &last = captureState.samples.back().world;
            summary << "last_existing_chunks="
                    << last.chunks.existingChunks << "\n";
            summary << "last_loaded_chunks=" << last.chunks.loadedChunks
                    << "\n";
            summary << "last_sections=" << last.chunks.sections << "\n";
            summary << "last_mesh_dirty_sections="
                    << last.chunks.meshDirtySections << "\n";
            summary << "last_cpu_ready_sections="
                    << last.chunks.cpuReadySections << "\n";
            summary << "last_gpu_buffered_sections="
                    << last.chunks.gpuBufferedSections << "\n";
            summary << "last_queued_chunk_updates="
                    << last.queuedChunkUpdates << "\n";
            summary << "last_random_tick_sections="
                    << last.randomTickSections << "\n";
            summary << "last_random_tick_blocks="
                    << last.randomTickBlocks << "\n";
            summary << "last_random_tick_sections_processed="
                    << last.randomTickSectionsProcessed << "\n";
            summary << "last_random_ticks_dispatched="
                    << last.randomTicksDispatched << "\n";
            summary << "last_mesh_rebuilds=" << last.chunks.meshRebuilds
                    << "\n";
            summary << "last_mesh_build_total_ms="
                    << last.chunks.meshBuildTotalMs << "\n";
            summary << "last_mesh_build_avg_ms="
                    << (last.chunks.meshRebuilds > 0
                            ? last.chunks.meshBuildTotalMs /
                                  static_cast<double>(last.chunks.meshRebuilds)
                            : 0.0)
                    << "\n";
            summary << "last_mesh_build_last_ms="
                    << last.chunks.meshBuildLastMs << "\n";
            summary << "last_mesh_build_max_ms="
                    << last.chunks.meshBuildMaxMs << "\n";
            summary << "last_solid_faces=" << last.chunks.solidFaces << "\n";
            summary << "last_transparent_faces="
                    << last.chunks.transparentFaces << "\n";
            summary << "last_water_faces=" << last.chunks.waterFaces << "\n";
            summary << "last_flora_faces=" << last.chunks.floraFaces << "\n";
            summary << "last_solid_vertices=" << last.chunks.solidVertices
                    << "\n";
            summary << "last_transparent_vertices="
                    << last.chunks.transparentVertices << "\n";
            summary << "last_water_vertices=" << last.chunks.waterVertices
                    << "\n";
            summary << "last_flora_vertices=" << last.chunks.floraVertices
                    << "\n";
            summary << "last_actor_count=" << last.actorCount << "\n";
            summary << "terrain_seed=" << last.terrainSeed << "\n";
        }

        summary.close();
        std::cout << "[PerfCapture] summary path=" << summaryPath << "\n";
    }
} // namespace

bool isEnabled()
{
    initialize();
    return state().enabled;
}

void recordSimulationTicks(std::size_t ticks)
{
    initialize();

    CaptureState &captureState = state();
    if (!captureState.enabled || captureState.complete) {
        return;
    }

    captureState.pendingSimulationTicks += ticks;
}

void recordFrame(const FrameTimings &timings,
                 const WorldDebugStats &worldStats)
{
    initialize();

    CaptureState &captureState = state();
    if (!captureState.enabled || captureState.complete) {
        return;
    }

    if (!captureState.timingStarted) {
        captureState.startTime = std::chrono::steady_clock::now();
        captureState.timingStarted = true;
    }

    const auto now = std::chrono::steady_clock::now();
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(
            now - captureState.startTime)
            .count();
    if (elapsedMs < captureState.warmupMs) {
        captureState.pendingSimulationTicks = 0;
        return;
    }

    const double measuredElapsedMs = elapsedMs - captureState.warmupMs;
    FrameSample sample;
    sample.timings = timings;
    sample.world = worldStats;
    sample.measuredElapsedMs = measuredElapsedMs;
    sample.simulationTicks = captureState.pendingSimulationTicks;
    captureState.capturedSimulationTicks +=
        captureState.pendingSimulationTicks;
    captureState.pendingSimulationTicks = 0;
    captureState.samples.push_back(sample);

    captureState.frames << captureState.frameIndex++ << ","
                        << measuredElapsedMs << "," << timings.deltaMs << ","
                        << timings.eventMs << "," << timings.updateMs << ","
                        << timings.renderMs << "," << timings.debugGuiMs
                        << "," << timings.renderCaptureMs << ","
                        << timings.displayMs << "," << timings.frameMs << ","
                        << worldStats.chunks.existingChunks << ","
                        << worldStats.chunks.loadedChunks << ","
                        << worldStats.chunks.saveDirtyChunks << ","
                        << worldStats.chunks.sections << ","
                        << worldStats.chunks.meshDirtySections << ","
                        << worldStats.chunks.cpuReadySections << ","
                        << worldStats.chunks.gpuBufferedSections << ","
                        << worldStats.queuedChunkUpdates << ","
                        << worldStats.randomTickSections << ","
                        << worldStats.randomTickBlocks << ","
                        << worldStats.randomTickSectionsProcessed << ","
                        << worldStats.randomTicksDispatched << ","
                        << worldStats.chunks.meshRebuilds << ","
                        << worldStats.chunks.meshBuildTotalMs << ","
                        << worldStats.chunks.meshBuildLastMs << ","
                        << worldStats.chunks.meshBuildMaxMs << ","
                        << worldStats.chunks.solidFaces << ","
                        << worldStats.chunks.transparentFaces << ","
                        << worldStats.chunks.waterFaces << ","
                        << worldStats.chunks.floraFaces << ","
                        << worldStats.chunks.solidVertices << ","
                        << worldStats.chunks.transparentVertices << ","
                        << worldStats.chunks.waterVertices << ","
                        << worldStats.chunks.floraVertices << ","
                        << worldStats.actorCount << ","
                        << worldStats.terrainSeed << ","
                        << worldStats.worldTime << ","
                        << sample.simulationTicks << "\n";

    if (measuredElapsedMs >= captureState.durationMs) {
        captureState.complete = true;
        writeSummary();
    }
}

bool shouldCloseWindow()
{
    initialize();
    const CaptureState &captureState = state();
    return captureState.enabled && captureState.exitWhenComplete &&
           captureState.complete;
}

void shutdown()
{
    initialize();
    writeSummary();
}
} // namespace RuntimePerformanceCapture
