#pragma once

#include "../World/World.h"

namespace RuntimePerformanceCapture
{
    struct FrameTimings
    {
        double deltaMs = 0.0;
        double eventMs = 0.0;
        double updateMs = 0.0;
        double renderMs = 0.0;
        double debugGuiMs = 0.0;
        double renderCaptureMs = 0.0;
        double displayMs = 0.0;
        double frameMs = 0.0;
    };

    bool isEnabled();
    void recordFrame(const FrameTimings &timings,
                     const WorldDebugStats &worldStats);
    bool shouldCloseWindow();
    void shutdown();
} // namespace RuntimePerformanceCapture
