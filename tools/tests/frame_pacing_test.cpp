#include "../../src/HelloMine3D/Presentation/FramePacing.h"

#include <array>
#include <chrono>
#include <iostream>
#include <string>

namespace
{
    unsigned checks = 0;
    unsigned failures = 0;

    void check(const std::string& name, bool passed)
    {
        ++checks;
        failures += !passed;
        std::cout << "[FRAME_PACING] " << (passed ? "PASS " : "FAIL ")
                  << name << '\n';
    }
}

int main()
{
    using namespace std::chrono;
    using FramePacing::Clock;
    using FramePacing::Mode;
    using FramePacing::State;
    const Clock::time_point start(seconds(10));
    const auto budget = [&](Mode mode, nanoseconds elapsed) {
        return duration_cast<nanoseconds>(FramePacing::remainingBudget(
            start, start + elapsed, mode)).count();
    };

    struct Selection
    {
        State state;
        Mode expected;
    };
    // Focus cannot override inactivity; simulation cannot override lost focus.
    // Explicit hidden diagnostics preserve their 60 FPS policy in every state.
    const std::array<Selection, 16> selections{{
        {{false, true, true, true}, Mode::Playing},
        {{false, true, true, false}, Mode::Interface},
        {{false, true, false, true}, Mode::Background},
        {{false, true, false, false}, Mode::Background},
        {{false, false, true, true}, Mode::Inactive},
        {{false, false, true, false}, Mode::Inactive},
        {{false, false, false, true}, Mode::Inactive},
        {{false, false, false, false}, Mode::Inactive},
        {{true, true, true, true}, Mode::Playing},
        {{true, true, true, false}, Mode::Playing},
        {{true, true, false, true}, Mode::Playing},
        {{true, true, false, false}, Mode::Playing},
        {{true, false, true, true}, Mode::Playing},
        {{true, false, true, false}, Mode::Playing},
        {{true, false, false, true}, Mode::Playing},
        {{true, false, false, false}, Mode::Playing}
    }};
    for (std::size_t index = 0; index < selections.size(); ++index)
    {
        check("state-priority-" + std::to_string(index),
              FramePacing::modeFor(selections[index].state) == selections[index].expected);
    }

    struct Policy
    {
        Mode mode;
        unsigned fps;
        const char* name;
        long long periodNs;
    };
    const std::array<Policy, 4> policies{{
        {Mode::Playing, 60, "playing", 16'666'667},
        {Mode::Interface, 30, "interface", 33'333'334},
        {Mode::Background, 20, "background", 50'000'000},
        {Mode::Inactive, 10, "inactive", 100'000'000}
    }};
    for (const auto& policy : policies)
    {
        const std::string label(policy.name);
        check(label + "-rate-and-name",
              FramePacing::fpsFor(policy.mode) == policy.fps &&
                  std::string(FramePacing::modeName(policy.mode)) == policy.name);
        check(label + "-whole-frame-budget-rounded-up",
              budget(policy.mode, 0ns) == policy.periodNs &&
                  budget(policy.mode, 0ns) * policy.fps >= 1'000'000'000LL);
        check(label + "-render-work-consumes-budget",
              budget(policy.mode, 3ms) == policy.periodNs - 3'000'000);
        check(label + "-last-nanosecond-is-waitable",
              budget(policy.mode, nanoseconds(policy.periodNs - 1)) == 1);
        check(label + "-already-at-budget-does-not-wait",
              budget(policy.mode, nanoseconds(policy.periodNs)) == 0);
        check(label + "-over-budget-never-catches-up",
              budget(policy.mode, 1s) == 0);
    }

    const auto foreground = FramePacing::modeFor({false, true, true, true});
    check("restore-foreground-cancels-inactive-wait",
          budget(Mode::Inactive, 25ms) == 75'000'000 &&
              budget(foreground, 25ms) == 0);
    check("resume-playing-shortens-interface-wait",
          budget(Mode::Interface, 20ms) > 0 && budget(foreground, 20ms) == 0);
    check("partial-render-only-waits-the-remainder",
          budget(Mode::Playing, 12ms) == 4'666'667);
    check("vsync-already-spent-the-frame-does-not-wait-twice",
          budget(Mode::Playing, nanoseconds(16'666'667)) == 0 &&
              budget(Mode::Playing, 20ms) == 0);
    const auto afterStall = start + 5s;
    check("long-stall-does-not-leave-next-frame-debt",
          FramePacing::remainingBudget(start, afterStall, Mode::Playing) == Clock::duration::zero() &&
              duration_cast<nanoseconds>(FramePacing::remainingBudget(
                  afterStall, afterStall + 2ms, Mode::Playing)).count() == 14'666'667);
    check("early-time-value-does-not-extend-a-frame-budget",
          duration_cast<nanoseconds>(FramePacing::remainingBudget(
              start, start - 1ms, Mode::Playing)).count() == 16'666'667);

    std::cout << "[FRAME_PACING] checks=" << checks << " failures=" << failures
              << " status=" << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
