#ifndef FIXEDTICKSCHEDULER_H_INCLUDED
#define FIXEDTICKSCHEDULER_H_INCLUDED

#include <algorithm>
#include <chrono>
#include <cstddef>

class FixedTickScheduler {
  public:
    explicit FixedTickScheduler(std::size_t ticksPerSecond = 20,
                                std::size_t maxTicksPerFrame = 5)
        : m_step(ticksPerSecond > 0
                     ? std::chrono::microseconds(1000000 / ticksPerSecond)
                     : std::chrono::microseconds(50000))
        , m_maxTicksPerFrame(std::max<std::size_t>(1, maxTicksPerFrame))
    {
    }

    std::size_t advance(std::chrono::microseconds elapsed) noexcept
    {
        if (elapsed.count() > 0) {
            m_accumulator += elapsed;
        }

        const auto availableTicks = static_cast<std::size_t>(
            m_accumulator.count() / m_step.count());
        const auto ticks = std::min(availableTicks, m_maxTicksPerFrame);
        m_accumulator -= m_step * ticks;

        if (ticks == m_maxTicksPerFrame) {
            m_accumulator = std::chrono::microseconds::zero();
        }

        return ticks;
    }

  private:
    std::chrono::microseconds m_accumulator{0};
    std::chrono::microseconds m_step;
    std::size_t m_maxTicksPerFrame;
};

#endif // FIXEDTICKSCHEDULER_H_INCLUDED
