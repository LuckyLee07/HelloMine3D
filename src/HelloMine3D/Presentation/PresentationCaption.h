#pragma once

#include <string>

struct PresentationCaptionSnapshot
{
    std::string cueId;
    std::string fallback;
    float remainingSeconds = 0.0f;

    bool visible() const noexcept
    {
        return remainingSeconds > 0.0f && !fallback.empty();
    }
};

/// Bounded subtitle lifetime and replacement policy independent of the audio
/// device and renderer. Higher-priority combat warnings cannot be immediately
/// hidden by ambient feedback; submitting the same cue refreshes its lifetime.
class PresentationCaptionTimeline
{
  public:
    static constexpr float DurationSeconds = 2.5f;

    void submit(std::string cueId, std::string fallback) noexcept;
    void update(float deltaSeconds) noexcept;
    void clear() noexcept;
    PresentationCaptionSnapshot snapshot() const;

  private:
    static int priority(const std::string& cueId) noexcept;

    std::string m_cueId;
    std::string m_fallback;
    float m_remainingSeconds = 0.0f;
    int m_priority = 0;
};
