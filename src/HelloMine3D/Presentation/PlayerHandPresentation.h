#pragma once

#include <algorithm>
#include <cmath>
#include "ItemVisualGeometry.h"
#include "ToolActionPresentation.h"

// Render-only geometry and motion. No Player, inventory or world ownership.
namespace PlayerHandPresentation {
enum class Grip { Empty, Icon, Block };
struct Face {
    ItemVisualGeometry::Face geometry;
    glm::vec3 colour;
};
using Mesh = std::vector<Face>;

struct LightingState {
    float exposure = .12f;
    bool initialized = false;
};

// Match the current terrain's local-light shaping and day/night exposure.
// Inputs are copied light-level brightness values, never world queries.
inline float lightingExposure(float sunlight, float blockLight, float daylight)
{
    const auto bounded = [](float value, float fallback) {
        return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : fallback;
    };
    const float local = std::max(bounded(sunlight, .15f), bounded(blockLight, .15f));
    return (.24f + .76f * local) * (.34f + .66f * bounded(daylight, 0.f));
}

inline float updateLighting(LightingState& state, bool known, float sunlight,
                            float blockLight, float daylight, float deltaSeconds)
{
    if (!known) return state.exposure;
    const float target = lightingExposure(sunlight, blockLight, daylight);
    if (!state.initialized) {
        state.exposure = target;
        state.initialized = true;
    } else if (std::isfinite(deltaSeconds) && deltaSeconds > 0.f) {
        state.exposure += (target - state.exposure) *
            (1.f - std::exp(-std::min(deltaSeconds, .25f) / .1f));
    }
    return state.exposure;
}

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

using Action = ToolActionPresentation::Action;

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
    const double ambientSeconds = std::isfinite(input.ambientSeconds)
        ? std::max(0., input.ambientSeconds) : 0.;
    const float actionSeconds = std::isfinite(input.actionSeconds)
        ? std::max(0.f, input.actionSeconds) : 0.f;
    const float strength = unit(input.strength);
    const float movement = unit(input.movement);
    const auto tool = ToolActionPresentation::derive(input.action,
        actionSeconds, strength, input.recoil, input.contact);
    const float preparation = tool.preparation;
    const float strikePose = tool.strike;
    const float use = tool.use;
    const float consume = tool.consume;
    const float swing = tool.activity();
    const float walk = static_cast<float>(
        std::sin(ambientSeconds * 7.5)) * movement * strength;
    const float idle = static_cast<float>(
        std::sin(ambientSeconds * 1.7)) * strength;
    return {
        -.22f + preparation * .30f - strikePose * .65f - use * .18f + consume * .43f +
            walk * .055f,
        -.52f - preparation * .12f + strikePose * .32f + use * .18f + consume * .38f,
        -.24f - preparation * .38f + strikePose * .85f + use * .34f - consume * .28f +
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
