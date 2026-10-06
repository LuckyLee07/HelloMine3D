#pragma once

#include "../Item/CraftingSession.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

// An operation acknowledgement never replaces the current authoritative
// preview. Its lifetime belongs to the visible crafting panel, not simulation.
class CraftingResultFeedback {
  public:
    enum class Tone { Information, Success, Failure };

    struct View {
        CraftingPreviewStatus previewStatus;
        int maximumCrafts;
        std::string_view recentMessage;
        Tone recentTone;

        bool ready() const noexcept
        {
            return previewStatus == CraftingPreviewStatus::Ready &&
                   maximumCrafts > 0;
        }
    };

    static constexpr float LifetimeSeconds = 4.f;

    void submit(std::string message, Tone tone)
    {
        m_message = std::move(message);
        m_tone = tone;
        m_remainingSeconds = m_message.empty() ? 0.f : LifetimeSeconds;
    }

    void advance(float deltaSeconds, bool panelActive) noexcept
    {
        if (!panelActive || !std::isfinite(deltaSeconds) ||
            deltaSeconds <= 0.f || m_remainingSeconds <= 0.f)
            return;
        m_remainingSeconds = std::max(0.f, m_remainingSeconds - deltaSeconds);
        if (m_remainingSeconds == 0.f) clear();
    }

    void clear() noexcept
    {
        m_message.clear();
        m_tone = Tone::Information;
        m_remainingSeconds = 0.f;
    }

    View view(const CraftingPreview& preview) const noexcept
    {
        return {preview.status, preview.ready() ? preview.maxCrafts : 0,
                m_remainingSeconds > 0.f ? std::string_view(m_message)
                                        : std::string_view{},
                m_tone};
    }

  private:
    std::string m_message;
    Tone m_tone = Tone::Information;
    float m_remainingSeconds = 0.f;
};
