#pragma once

#include <algorithm>
#include <cmath>

// Copy-only action articulation shared by both player views. The caller owns
// action time and feedback; no simulation, input, cooldown, or contact query.
namespace ToolActionPresentation {
enum class Action { None, Mining, Strike, Use, Consume };
struct Pose {
    float preparation = 0.f;
    float strike = 0.f;
    float use = 0.f;
    float consume = 0.f;
    float activity() const noexcept
    {
        return std::max({preparation, strike, use * .58f, consume * .42f});
    }
};
inline float unit(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
}
inline float ease(float value) noexcept
{
    value = unit(value);
    return value * value * (3.f - 2.f * value);
}
inline float pulse(float seconds, float rise, float hold, float fall) noexcept
{
    if (seconds < rise) return ease(seconds / rise);
    seconds -= rise;
    if (seconds < hold) return 1.f;
    seconds -= hold;
    return 1.f - ease(seconds / fall);
}
inline Pose derive(Action action, float seconds, float strength,
                   float recoil, float contact) noexcept
{
    Pose pose;
    strength = unit(strength);
    if (strength == 0.f) return pose;
    seconds = std::isfinite(seconds) ? std::max(0.f, seconds) : 0.f;
    recoil = unit(recoil); // The existing feedback timeline already scales it.
    switch (action) {
    case Action::Mining: {
        // Slower lift, fast downswing, brief contact, deliberate recovery.
        // The 2.7 Hz period and mining's authoritative elapsed are unchanged.
        const float phase = std::fmod(seconds * 2.7f, 1.f);
        pose.preparation = strength * (phase < .20f ? ease(phase / .20f)
            : 1.f - ease((phase - .20f) / .18f));
        pose.strike = strength * (phase < .38f ? ease((phase - .20f) / .18f)
            : phase < .46f ? 1.f : 1.f - ease((phase - .46f) / .54f));
        break;
    }
    case Action::Strike:
        // Feedback follows an already committed result. This flourish never
        // delays that result, and a miss still has no contact hold.
        pose.preparation = recoil * pulse(seconds, .025f, 0.f, .030f);
        pose.strike = recoil * (seconds < .055f
            ? ease((seconds - .025f) / .030f)
            : seconds < .080f ? 1.f : 1.f - ease((seconds - .080f) / .120f));
        break;
    case Action::Use:
        pose.use = recoil * pulse(seconds, .080f, .025f, .155f);
        break;
    case Action::Consume:
        pose.consume = recoil * pulse(seconds, .110f, .150f, .160f);
        break;
    case Action::None: break;
    }
    const float hold = unit(contact) * strength;
    pose.strike = std::max(pose.strike, hold);
    pose.preparation *= 1.f - unit(contact);
    return pose;
}
}
