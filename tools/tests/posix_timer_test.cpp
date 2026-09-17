#include "OgreTimer.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <sys/time.h>
#include <thread>

namespace {
std::int64_t wallMicroseconds = 100000000000LL;
unsigned wallReads = 0;
int failures = 0;
int checks = 0;
void check(const char* name, bool passed)
{
    ++checks;
    if (!passed) ++failures;
    std::cout << "[POSIX_TIMER] " << (passed ? "PASS " : "FAIL ") << name << '\n';
}
}

// Only OgrePOSIXTimer.cpp is compiled with gettimeofday renamed to this
// function. This never changes the machine's clock or interposes other code.
extern "C" int hello_test_gettimeofday(struct timeval* value, void*)
{
    ++wallReads;
    value->tv_sec = wallMicroseconds / 1000000;
    value->tv_usec = wallMicroseconds % 1000000;
    return 0;
}

int main()
{
    using Clock = std::chrono::steady_clock;
    Ogre::Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    wallMicroseconds += 20000;
    const auto before = timer.getMilliseconds();
    const auto beforeMicro = timer.getMicroseconds();
    wallMicroseconds -= 1000;
    const auto after = timer.getMilliseconds();
    const auto afterMicro = timer.getMicroseconds();
    std::cout << "[POSIX_TIMER] rollback before_ms=" << before << " after_ms=" << after
              << " unsigned_delta_ms=" << after - before << '\n';
    check("one-millisecond-wall-rollback-keeps-event-delta-monotonic", after >= before);
    check("wall-rollback-keeps-microseconds-monotonic", afterMicro >= beforeMicro);

    wallMicroseconds -= 3000000;
    const auto earlier = timer.getMilliseconds();
    check("wall-before-reset-does-not-wrap", earlier >= after && earlier - after < 1000);

    const auto referenceStart = Clock::now();
    const auto elapsedStart = timer.getMicroseconds();
    wallMicroseconds += 3600000000LL;
    const auto elapsedEnd = timer.getMicroseconds();
    const auto referenceEnd = Clock::now();
    const auto actualUs = std::chrono::duration_cast<std::chrono::microseconds>(
        referenceEnd - referenceStart).count();
    check("one-hour-wall-jump-does-not-advance-elapsed-time",
          elapsedEnd >= elapsedStart && elapsedEnd - elapsedStart <= actualUs + 2000);

    timer.reset();
    const auto pauseStart = Clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto pauseMs = timer.getMilliseconds();
    const auto pauseActual = std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now() - pauseStart).count();
    check("elapsed-time-advances-while-wall-clock-is-unchanged",
          pauseMs >= 15 && pauseMs <= pauseActual + 2);

    bool monotonic = true;
    auto lastMs = timer.getMilliseconds();
    auto lastUs = timer.getMicroseconds();
    for (int i = 0; i < 10000; ++i) {
        wallMicroseconds += i % 2 ? -1000000 : 1000000;
        const auto ms = timer.getMilliseconds();
        const auto us = timer.getMicroseconds();
        monotonic = monotonic && ms >= lastMs && us >= lastUs;
        lastMs = ms; lastUs = us;
    }
    check("ten-thousand-reads-with-alternating-wall-clock", monotonic);

    Ogre::Timer second;
    const auto firstBeforeReset = timer.getMicroseconds();
    second.reset();
    const auto secondElapsed = second.getMicroseconds();
    const auto firstAfterReset = timer.getMicroseconds();
    check("reset-is-local-to-one-timer", firstAfterReset >= firstBeforeReset &&
          secondElapsed < firstBeforeReset);
    timer.reset();
    check("reset-restarts-elapsed-time", timer.getMicroseconds() < firstBeforeReset);

    const auto usBefore = timer.getMicroseconds();
    const auto ms = timer.getMilliseconds();
    const auto usAfter = timer.getMicroseconds();
    check("millisecond-and-microsecond-units-agree",
          ms * 1000 <= usAfter && ms * 1000 + 1000 > usBefore);
    const auto cpuBefore = timer.getMicrosecondsCPU();
    volatile std::uint64_t sum = 0;
    for (unsigned i = 0; i < 100000; ++i) sum += i;
    check("cpu-time-api-remains-monotonic", timer.getMicrosecondsCPU() >= cpuBefore);
    std::cout << "[POSIX_TIMER] checks=" << checks << " failures=" << failures
              << " injected_wall_reads=" << wallReads << '\n';
    return failures ? 1 : 0;
}
