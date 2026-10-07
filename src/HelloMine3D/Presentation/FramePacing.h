#pragma once

#include <chrono>

// Application frame budget only. The caller owns event pumping and waiting;
// simulation still receives the actual elapsed time, including this wait.
namespace FramePacing
{
    using Clock = std::chrono::steady_clock;

    struct State
    {
        bool hiddenDiagnostic = false;
        bool windowActive = true;
        bool focused = true;
        bool worldSimulation = true;
    };

    enum class Mode
    {
        Playing,
        Interface,
        Background,
        Inactive
    };

    constexpr Mode modeFor(State state) noexcept
    {
        if (state.hiddenDiagnostic) return Mode::Playing;
        if (!state.windowActive) return Mode::Inactive;
        if (!state.focused) return Mode::Background;
        return state.worldSimulation ? Mode::Playing : Mode::Interface;
    }

    constexpr unsigned fpsFor(Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::Playing: return 60;
            case Mode::Interface: return 30;
            case Mode::Background: return 20;
            case Mode::Inactive: return 10;
        }
        return 60;
    }

    constexpr const char* modeName(Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::Playing: return "playing";
            case Mode::Interface: return "interface";
            case Mode::Background: return "background";
            case Mode::Inactive: return "inactive";
        }
        return "playing";
    }

    inline Clock::duration remainingBudget(Clock::time_point frameStart,
                                          Clock::time_point now,
                                          Mode mode) noexcept
    {
        const auto fps = fpsFor(mode);
        // Round up twice if needed: to nanoseconds, then to this clock's tick.
        // Integer truncation must not permit a rate above the requested cap.
        const auto period = std::chrono::ceil<Clock::duration>(
            std::chrono::nanoseconds((1'000'000'000LL + fps - 1) / fps));
        const auto elapsed = now > frameStart
            ? now - frameStart : Clock::duration::zero();
        // No saved deadline or catch-up debt. Re-evaluate after every state
        // change so returning to the foreground can end a longer idle wait.
        return elapsed >= period ? Clock::duration::zero() : period - elapsed;
    }
}
