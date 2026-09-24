#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include "WildlifeActor.h"

enum class WildlifeVisualRole {
    Body = 1,
    Head,
    Muzzle,
    Leg,
    Ear,
    Tail,
    Beak,
    Wing,
    Neck
};

struct WildlifeVisualPart {
    WildlifeVisualRole role = WildlifeVisualRole::Body;
    glm::vec3 offset{0.f};
    glm::vec3 scale{1.f};
};

struct WildlifeVisualProfile {
    static constexpr std::size_t MaximumParts = 8;
    std::array<WildlifeVisualPart, MaximumParts> parts{};
    std::size_t partCount = 0;
    int speciesIndex = 0;
};

struct WildlifeVisualPose {
    std::array<glm::vec3, WildlifeVisualProfile::MaximumParts> rotations{};
    std::array<glm::vec3, WildlifeVisualProfile::MaximumParts> offsets{};
    float heightOffset = 0.f;
};

namespace WildlifePresentation {
    // Interpolate the last authoritative segment, without predicting through
    // obstacles. This is presentation-only; collisions keep the World position.
    class MotionBlend {
      public:
        glm::vec3 update(const ActorSnapshot& snapshot, float dt)
        {
            if (!m_valid || glm::distance(snapshot.position, m_target) > 4.f) {
                m_from = m_target = m_position = snapshot.position;
                m_lastMoveTime = snapshot.wildlifeMotionSeconds;
                m_elapsed = m_duration = .05f;
                m_valid = true;
                return m_position;
            }
            if (snapshot.position != m_target) {
                m_from = m_position;
                m_target = snapshot.position;
                m_duration = std::clamp(
                    snapshot.wildlifeMotionSeconds - m_lastMoveTime, .05f, .20f);
                m_lastMoveTime = snapshot.wildlifeMotionSeconds;
                m_elapsed = 0.f;
            }
            m_elapsed = std::min(m_duration, m_elapsed +
                (std::isfinite(dt) ? std::clamp(dt, 0.f, .10f) : 0.f));
            m_position = glm::mix(m_from, m_target, m_elapsed / m_duration);
            return m_position;
        }
      private:
        bool m_valid = false;
        glm::vec3 m_from{0.f}, m_target{0.f}, m_position{0.f};
        float m_lastMoveTime = 0.f, m_elapsed = 0.f, m_duration = .05f;
    };

    inline WildlifeVisualProfile profileFor(const std::string &type)
    {
        WildlifeVisualProfile profile;
        const auto add = [&profile](WildlifeVisualRole role,
                                    glm::vec3 offset, glm::vec3 scale) {
            if (profile.partCount < profile.parts.size())
                profile.parts[profile.partCount++] = {role, offset, scale};
        };
        if (type == WildlifeSpecies::Sheep) {
            profile.speciesIndex = 0;
            add(WildlifeVisualRole::Body, {0.f, 0.04f, 0.06f},
                {0.86f, 0.58f, 0.86f});
            add(WildlifeVisualRole::Head, {0.f, 0.27f, -0.31f},
                {0.48f, 0.41f, 0.43f});
            add(WildlifeVisualRole::Muzzle, {0.f, 0.16f, -0.55f},
                {0.32f, 0.22f, 0.19f});
            for (int x : {-1, 1}) for (int z : {-1, 1})
                add(WildlifeVisualRole::Leg,
                    {x * 0.28f, -0.32f, z * 0.29f},
                    {0.18f, 0.34f, 0.18f});
            add(WildlifeVisualRole::Tail, {0.f, 0.10f, 0.49f},
                {0.19f, 0.20f, 0.18f});
        }
        else if (type == WildlifeSpecies::Rabbit) {
            profile.speciesIndex = 1;
            add(WildlifeVisualRole::Body, {0.f, -0.12f, 0.07f},
                {0.82f, 0.50f, 0.84f});
            add(WildlifeVisualRole::Head, {0.f, 0.12f, -0.31f},
                {0.57f, 0.46f, 0.48f});
            add(WildlifeVisualRole::Ear, {-0.17f, 0.39f, -0.31f},
                {0.19f, 0.44f, 0.17f});
            add(WildlifeVisualRole::Ear, {0.17f, 0.39f, -0.31f},
                {0.19f, 0.44f, 0.17f});
            add(WildlifeVisualRole::Leg, {-0.25f, -0.36f, 0.18f},
                {0.25f, 0.27f, 0.32f});
            add(WildlifeVisualRole::Leg, {0.25f, -0.36f, 0.18f},
                {0.25f, 0.27f, 0.32f});
            add(WildlifeVisualRole::Tail, {0.f, -0.08f, 0.52f},
                {0.25f, 0.27f, 0.22f});
        }
        else if (type == WildlifeSpecies::MarshBird) {
            profile.speciesIndex = 2;
            add(WildlifeVisualRole::Body, {0.f, -0.12f, 0.05f},
                {0.76f, 0.48f, 0.82f});
            add(WildlifeVisualRole::Neck, {0.f, 0.16f, -0.25f},
                {0.30f, 0.54f, 0.28f});
            add(WildlifeVisualRole::Head, {0.f, 0.39f, -0.28f},
                {0.42f, 0.33f, 0.41f});
            add(WildlifeVisualRole::Beak, {0.f, 0.32f, -0.57f},
                {0.31f, 0.16f, 0.27f});
            add(WildlifeVisualRole::Leg, {-0.18f, -0.37f, 0.04f},
                {0.12f, 0.27f, 0.13f});
            add(WildlifeVisualRole::Leg, {0.18f, -0.37f, 0.04f},
                {0.12f, 0.27f, 0.13f});
            add(WildlifeVisualRole::Wing, {-0.39f, -0.07f, 0.07f},
                {0.18f, 0.30f, 0.61f});
            add(WildlifeVisualRole::Wing, {0.39f, -0.07f, 0.07f},
                {0.18f, 0.30f, 0.61f});
        }
        return profile;
    }

    inline WildlifeVisualPose poseFor(const ActorSnapshot &snapshot,
        const WildlifeVisualProfile &profile, float stridePhase,
        float animationStrength)
    {
        WildlifeVisualPose pose;
        const float strength = std::clamp(animationStrength, 0.f, 1.f);
        const auto activity = static_cast<WildlifeActivity>(
            snapshot.wildlifeActivity);
        const bool walking = activity == WildlifeActivity::Wander ||
                             activity == WildlifeActivity::Flee;
        const float stride = walking ?
            std::sin(stridePhase) *
                (activity == WildlifeActivity::Flee ? 23.f : 12.f) * strength
            : 0.f;
        const float foragePitch = (12.f +
            7.f * std::sin(snapshot.wildlifeMotionSeconds * 3.4f)) * strength;
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const auto role = profile.parts[index].role;
            if (role == WildlifeVisualRole::Leg)
                pose.rotations[index].x = stride *
                    (profile.parts[index].offset.x < 0.f ? 1.f : -1.f) *
                    (profile.parts[index].offset.z < 0.f ? 1.f : -1.f);
            if (role == WildlifeVisualRole::Head ||
                role == WildlifeVisualRole::Muzzle ||
                role == WildlifeVisualRole::Neck ||
                role == WildlifeVisualRole::Beak) {
                if (activity == WildlifeActivity::Forage)
                    pose.rotations[index].x = foragePitch;
                else if (activity == WildlifeActivity::Flee)
                    pose.rotations[index].x = -8.f * strength;
            }
            if (role == WildlifeVisualRole::Ear &&
                activity == WildlifeActivity::Flee)
                pose.rotations[index].x = 12.f * strength;
            if (role == WildlifeVisualRole::Wing && walking)
                pose.rotations[index].z =
                    (profile.parts[index].offset.x < 0.f ? -1.f : 1.f) *
                    std::abs(stride) * 0.24f;
        }
        if (profile.speciesIndex == 1 && walking)
            pose.heightOffset = std::max(0.f, std::sin(stridePhase)) *
                                (activity == WildlifeActivity::Flee ?
                                 0.075f : 0.035f) * strength;
        return pose;
    }
}
