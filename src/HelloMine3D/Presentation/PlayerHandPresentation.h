#pragma once

#include <algorithm>
#include <cmath>
#include "ItemVisualGeometry.h"

// Render-only geometry and motion. No Player, inventory or world ownership.
namespace PlayerHandPresentation {
enum class Grip { Empty, Icon, Block };
struct Face {
    ItemVisualGeometry::Face geometry;
    glm::vec3 colour;
};
using Mesh = std::vector<Face>;

inline glm::vec3 turn(glm::vec3 point, float angle)
{
    return {point.x * std::cos(angle) - point.y * std::sin(angle),
            point.x * std::sin(angle) + point.y * std::cos(angle), point.z};
}

inline Mesh build(Grip grip)
{
    Mesh result;
    result.reserve(54);
    const auto cube = ItemVisualGeometry::cube({0,0}, {0,0}, {0,0});
    const auto box = [&](glm::vec3 center, glm::vec3 size, float angle, glm::vec3 colour) {
        for (auto face : cube) {
            for (auto& point : face.positions) point = center + turn(point * size, angle);
            face.normal = turn(face.normal, angle);
            result.push_back({face, colour});
        }
    };
    // Icon handles occupy the lower-left diagonal; blocks are supported below
    // their bottom face. Empty slots retain a relaxed, closed hand.
    const glm::vec3 palm = grip == Grip::Icon ? glm::vec3(-.28f,-.32f,.09f) :
        grip == Grip::Block ? glm::vec3(.04f,-.57f,.20f) : glm::vec3(-.10f,-.08f,.18f);
    constexpr float angle = .52f;
    const glm::vec3 arm = turn({0,-1,0}, angle);
    const glm::vec3 wrist = palm + arm * .23f;
    box(wrist + arm * .50f, {.34f,1.00f,.31f}, angle, {63,96,98}); // sleeve
    box(wrist + arm * .30f + glm::vec3(0,0,.162f), {.19f,.35f,.016f},
        angle, {72,107,108}); // woven sleeve panel
    box(wrist, {.38f,.14f,.35f}, angle, {111,91,66}); // leather cuff
    box(wrist + arm * .044f, {.388f,.025f,.36f}, angle, {157,133,94}); // cuff seam
    box(palm, {.30f,.31f,.25f}, .12f, {185,137,101});
    // Three broad knuckle planes read at game resolution without tiny finger
    // geometry. The thumb wraps the near side, leaving the tool head exposed.
    for (int finger = 0; finger < 3; ++finger)
        box(palm + glm::vec3(-.097f + finger * .097f,.084f,.116f),
            {.087f,.145f,.10f}, .12f, {204.f - finger * 5.f,157.f - finger * 5.f,117.f - finger * 4.f});
    box(palm + glm::vec3(-.153f,-.005f,.13f), {.12f,.22f,.13f}, -.28f, {217,166,122});
    return result;
}

inline const Mesh& mesh(Grip grip)
{
    static const Mesh empty = build(Grip::Empty);
    static const Mesh icon = build(Grip::Icon);
    static const Mesh block = build(Grip::Block);
    return grip == Grip::Block ? block : grip == Grip::Icon ? icon : empty;
}

struct Motion {
    float pitch, yaw, roll, swing, bob;
    glm::vec3 rotate(glm::vec3 point) const
    {
        point = {point.x, point.y * std::cos(pitch) - point.z * std::sin(pitch),
                 point.y * std::sin(pitch) + point.z * std::cos(pitch)};
        point = {point.x * std::cos(yaw) + point.z * std::sin(yaw), point.y,
                 -point.x * std::sin(yaw) + point.z * std::cos(yaw)};
        return turn(point, roll);
    }
};

enum class Action { None, Mining, Strike, Use, Consume };

struct MotionInput {
    double ambientSeconds = 0.;
    float actionSeconds = 0.f;
    float movement = 0.f;
    float strength = 0.f;
    // Recoil is already scaled by ActionFeedbackTimeline. Contact remains a
    // separate hold so an attack miss can follow through without pretending
    // that it struck a surface.
    float recoil = 0.f;
    float contact = 0.f;
    Action action = Action::None;
};

struct ActionPhase {
    Action action = Action::None;
    float seconds = 0.f;
    bool acceptsFeedbackContact = true;
};

inline ActionPhase actionPhase(bool miningActive, float miningSeconds,
                               Action feedbackAction,
                               float feedbackSeconds) noexcept
{
    return miningActive
        ? ActionPhase{Action::Mining, miningSeconds, false}
        : ActionPhase{feedbackAction, feedbackSeconds, true};
}

inline Motion motion(const MotionInput &input)
{
    const auto unit = [](float value) {
        return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
    };
    const auto ease = [&](float value) {
        value = unit(value);
        return value * value * (3.f - 2.f * value);
    };
    const auto pulse = [&](float seconds, float rise, float hold,
                           float fall) {
        seconds = std::isfinite(seconds) ? std::max(0.f, seconds) : 0.f;
        if (seconds < rise)
            return ease(seconds / std::max(0.001f, rise));
        seconds -= rise;
        if (seconds < hold) return 1.f;
        seconds -= hold;
        if (seconds < fall)
            return 1.f - ease(seconds / std::max(0.001f, fall));
        return 0.f;
    };

    const double ambientSeconds = std::isfinite(input.ambientSeconds)
        ? std::max(0., input.ambientSeconds) : 0.;
    const float actionSeconds = std::isfinite(input.actionSeconds)
        ? std::max(0.f, input.actionSeconds) : 0.f;
    const float strength = unit(input.strength);
    const float movement = unit(input.movement);
    const float recoil = unit(input.recoil);
    const float contact = unit(input.contact) * strength;

    float mining = 0.f;
    float strike = 0.f;
    float use = 0.f;
    float consume = 0.f;
    switch (input.action) {
        case Action::Mining: {
            const float cycle = std::fmod(actionSeconds * 2.7f, 1.f);
            mining = strength * (cycle < .38f
                ? ease(cycle / .38f)
                : 1.f - ease((cycle - .38f) / .62f));
            break;
        }
        case Action::Strike:
            strike = recoil * pulse(actionSeconds, .055f, .025f, .12f);
            break;
        case Action::Use:
            use = recoil * pulse(actionSeconds, .08f, .025f, .155f);
            break;
        case Action::Consume:
            consume = recoil * pulse(actionSeconds, .11f, .15f, .16f);
            break;
        case Action::None:
            break;
    }

    const float strikePose = std::max({mining, strike, contact});
    const float swing = std::clamp(
        std::max({strikePose, use * .58f, consume * .42f}), 0.f, 1.f);
    const float walk = static_cast<float>(
        std::sin(ambientSeconds * 7.5)) * movement * strength;
    const float idle = static_cast<float>(
        std::sin(ambientSeconds * 1.7)) * strength;
    return {
        -.22f - strikePose * .65f - use * .18f + consume * .43f +
            walk * .055f,
        -.52f + strikePose * .32f + use * .18f + consume * .38f,
        -.24f + strikePose * .85f + use * .34f - consume * .28f +
            idle * .02f,
        swing,
        (static_cast<float>(std::sin(ambientSeconds * 2.)) * 2.f +
         std::abs(static_cast<float>(std::sin(ambientSeconds * 7.5))) *
             movement * 7.f) * strength
    };
}

// Keep the existing held-item timing; hand and item share one rigid pose so
// contact and recovery cannot separate the grip from its contents.
inline Motion motion(double seconds, float movement, float strength, bool mining, float contact)
{
    MotionInput input;
    input.ambientSeconds = seconds;
    input.actionSeconds = static_cast<float>(seconds);
    input.movement = movement;
    input.strength = strength;
    input.contact = contact;
    input.action = mining ? Action::Mining : Action::None;
    return motion(input);
}
}
