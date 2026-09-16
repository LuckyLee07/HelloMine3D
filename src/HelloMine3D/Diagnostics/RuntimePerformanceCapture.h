#pragma once

#include <cstddef>
#include <string>

#include "../World/World.h"

namespace RuntimePerformanceCapture
{
    struct FrameTimings
    {
        double deltaMs = 0.0;
        double eventMs = 0.0;
        double updateMs = 0.0;
        double renderMs = 0.0;
        double renderDrawMs = 0.0;
        double renderPostDrawMs = 0.0;
        double renderEndedMs = 0.0;
        bool renderPhaseValid = false;
        double debugGuiMs = 0.0;
        double renderCaptureMs = 0.0;
        double displayMs = 0.0;
        double frameMs = 0.0;
    };

    bool isEnabled();
    void recordSimulationTicks(std::size_t ticks);
    void recordStreamingLatency(double milliseconds);
    void recordScenarioPopulation(std::size_t actorCount,
                                  std::size_t itemEntityCount,
                                  std::size_t cropCount,
                                  std::size_t chestCount,
                                  std::size_t capEvents);
    void recordFrame(const FrameTimings &timings,
                     const WorldDebugStats &worldStats);
    bool shouldCloseWindow();
    bool isComplete();
    void startDiagnosticSegment(const std::string &outputDirectory,
                                double warmupMs, double durationMs);
    void shutdown();
} // namespace RuntimePerformanceCapture
