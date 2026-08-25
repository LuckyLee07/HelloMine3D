#include "PresentationCaption.h"

#include <algorithm>
#include <cmath>
#include <utility>

void PresentationCaptionTimeline::submit(
    std::string cueId, std::string fallback) noexcept
{
    if (cueId.empty() || fallback.empty())
    {
        return;
    }
    const int candidatePriority = priority(cueId);
    if (m_remainingSeconds > 0.0f && cueId != m_cueId &&
        candidatePriority < m_priority)
    {
        return;
    }
    m_cueId = std::move(cueId);
    m_fallback = std::move(fallback);
    m_remainingSeconds = DurationSeconds;
    m_priority = candidatePriority;
}

void PresentationCaptionTimeline::update(float deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f ||
        m_remainingSeconds <= 0.0f)
    {
        return;
    }
    m_remainingSeconds =
        std::max(0.0f, m_remainingSeconds - deltaSeconds);
    if (m_remainingSeconds <= 0.0f)
    {
        clear();
    }
}

void PresentationCaptionTimeline::clear() noexcept
{
    m_cueId.clear();
    m_fallback.clear();
    m_remainingSeconds = 0.0f;
    m_priority = 0;
}

PresentationCaptionSnapshot PresentationCaptionTimeline::snapshot() const
{
    return {m_cueId, m_fallback, m_remainingSeconds};
}

int PresentationCaptionTimeline::priority(const std::string& cueId) noexcept
{
    if (cueId == "combat.windup" || cueId == "combat.guard")
    {
        return 3;
    }
    if (cueId == "combat.hit")
    {
        return 2;
    }
    if (cueId == "ambient.wind")
    {
        return 0;
    }
    return 1;
}
