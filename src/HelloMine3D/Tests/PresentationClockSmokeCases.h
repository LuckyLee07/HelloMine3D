#pragma once

#include "../Presentation/PresentationClock.h"
#include <limits>

namespace {
void casePresentationClock()
{
    double elapsed = 0.0;
    const float invalid[] = {-1.f, 0.f, std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()};
    for (const float delta : invalid) elapsed = advancePresentationClock(elapsed, delta);
    check("PRESENTATION_CLOCK/invalid-deltas-do-not-poison-phase", elapsed == 0.0);

    // A timer jump must not consume the mantissa so later small deltas vanish.
    elapsed = 0.0;
    elapsed = advancePresentationClock(elapsed, std::numeric_limits<float>::max());
    const double afterJump = elapsed;
    const double refreshDeadline = afterJump + 1.0 / 30.0;
    int distinctFrames = 0;
    for (int frame = 0; frame < 120; ++frame) {
        const double next = advancePresentationClock(elapsed, 1.f / 120.f);
        if (std::fmod(next * 2.7, 1.0) != std::fmod(elapsed * 2.7, 1.0)) ++distinctFrames;
        elapsed = next;
    }
    check("PRESENTATION_CLOCK/resume-preserves-every-motion-frame",
        afterJump <= .25 && distinctFrames == 120 && std::abs(elapsed - afterJump - 1.0) < .00001);
    check("PRESENTATION_CLOCK/map-refresh-recovers-after-timer-jump",
        refreshDeadline > afterJump && elapsed >= refreshDeadline);

    // Exercise storage and phase arithmetic after a week without running millions of frames.
    elapsed = 7.0 * 24.0 * 3600.0;
    const double before = elapsed;
    distinctFrames = 0;
    for (int frame = 0; frame < 240; ++frame) {
        const double next = advancePresentationClock(elapsed, 1.f / 240.f);
        if (std::fmod(next * 2.7, 1.0) != std::fmod(elapsed * 2.7, 1.0)) ++distinctFrames;
        elapsed = next;
    }
    check("PRESENTATION_CLOCK/long-session-retains-subframe-precision",
        distinctFrames == 240 && std::abs(elapsed - before - 1.0) < .00001);

    double slow = 0.0, fast = 0.0;
    for (int frame = 0; frame < 30; ++frame) slow = advancePresentationClock(slow, 1.f / 30.f);
    for (int frame = 0; frame < 144; ++frame) fast = advancePresentationClock(fast, 1.f / 144.f);
    check("PRESENTATION_CLOCK/frame-rate-independent-ordinary-time", std::abs(slow - fast) < .00001);
}
}
