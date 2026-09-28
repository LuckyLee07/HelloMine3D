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
                {0.78f, 0.58f, 1.22f});
            add(WildlifeVisualRole::Head, {0.f, 0.27f, -0.45f},
                {0.43f, 0.38f, 0.39f});
            add(WildlifeVisualRole::Muzzle, {0.f, 0.16f, -0.69f},
                {0.30f, 0.20f, 0.22f});
            for (int x : {-1, 1}) for (int z : {-1, 1})
                add(WildlifeVisualRole::Leg,
                    {x * 0.25f, -0.32f, z * 0.36f},
                    {0.18f, 0.34f, 0.18f});
            add(WildlifeVisualRole::Tail, {0.f, 0.10f, 0.70f},
                {0.19f, 0.20f, 0.18f});
        }
        else if (type == WildlifeSpecies::Rabbit) {
            profile.speciesIndex = 1;
            add(WildlifeVisualRole::Body, {0.f, -0.12f, 0.07f},
                {0.80f, 0.50f, 0.96f});
            add(WildlifeVisualRole::Head, {0.f, 0.12f, -0.35f},
                {0.54f, 0.44f, 0.46f});
            add(WildlifeVisualRole::Ear, {-0.16f, 0.40f, -0.35f},
                {0.17f, 0.49f, 0.17f});
            add(WildlifeVisualRole::Ear, {0.16f, 0.40f, -0.35f},
                {0.17f, 0.49f, 0.17f});
            add(WildlifeVisualRole::Leg, {-0.25f, -0.36f, 0.23f},
                {0.26f, 0.27f, 0.36f});
            add(WildlifeVisualRole::Leg, {0.25f, -0.36f, 0.23f},
                {0.26f, 0.27f, 0.36f});
            add(WildlifeVisualRole::Tail, {0.f, -0.08f, 0.60f},
                {0.25f, 0.27f, 0.22f});
        }
        else if (type == WildlifeSpecies::MarshBird) {
            profile.speciesIndex = 2;
            add(WildlifeVisualRole::Body, {0.f, -0.12f, 0.05f},
                {0.66f, 0.48f, 0.88f});
            add(WildlifeVisualRole::Neck, {0.f, 0.18f, -0.27f},
                {0.27f, 0.62f, 0.25f});
            add(WildlifeVisualRole::Head, {0.f, 0.46f, -0.31f},
                {0.37f, 0.30f, 0.38f});
            add(WildlifeVisualRole::Beak, {0.f, 0.39f, -0.62f},
                {0.21f, 0.14f, 0.32f});
            add(WildlifeVisualRole::Leg, {-0.18f, -0.35f, 0.04f},
                {0.105f, 0.30f, 0.12f});
            add(WildlifeVisualRole::Leg, {0.18f, -0.35f, 0.04f},
                {0.105f, 0.30f, 0.12f});
            add(WildlifeVisualRole::Wing, {-0.32f, -0.07f, 0.07f},
                {0.15f, 0.30f, 0.61f});
            add(WildlifeVisualRole::Wing, {0.32f, -0.07f, 0.07f},
                {0.15f, 0.30f, 0.61f});
        }
        return profile;
    }

    inline float individualPhase(ActorId id) noexcept
    {
        // A stable phase per animal without advancing a second clock.
        const auto mixed = (id ^ (id >> 16)) * 2654435761u;
        return static_cast<float>(mixed & 1023u) * (6.28318530718f / 1024.f);
    }

    inline glm::vec3 pitchVector(glm::vec3 value, float degrees)
    {
        const float angle = glm::radians(degrees);
        return {value.x, std::cos(angle) * value.y - std::sin(angle) * value.z,
                std::sin(angle) * value.y + std::cos(angle) * value.z};
    }

    inline void articulate(const WildlifeVisualProfile& profile, WildlifeVisualPose& pose)
    {
        const glm::vec3 neck = profile.speciesIndex == 0 ? glm::vec3(0,.23f,-.16f) :
            profile.speciesIndex == 1 ? glm::vec3(0,.05f,-.19f) : glm::vec3(0,-.10f,-.25f);
        float headPitch = 0.f;
        for (std::size_t i=0; i<profile.partCount; ++i)
            if (profile.parts[i].role == WildlifeVisualRole::Head)
                headPitch = pose.rotations[i].x;
        for (std::size_t i=0; i<profile.partCount; ++i) {
            const auto& part = profile.parts[i];
            pose.offsets[i] = glm::vec3(0.f);
            if (part.role == WildlifeVisualRole::Leg && pose.rotations[i].x != 0.f) {
                const glm::vec3 hip = part.offset + glm::vec3(0,.5f*part.scale.y,0);
                pose.offsets[i] = hip + pitchVector(part.offset-hip,pose.rotations[i].x)-part.offset;
            }
            if (part.role == WildlifeVisualRole::Head || part.role == WildlifeVisualRole::Muzzle ||
                part.role == WildlifeVisualRole::Neck || part.role == WildlifeVisualRole::Beak ||
                part.role == WildlifeVisualRole::Ear) {
                const float earPitch = part.role == WildlifeVisualRole::Ear ?
                    pose.rotations[i].x-headPitch : 0.f;
                const glm::vec3 earRoot(0,-.5f*part.scale.y,0);
                const auto local = earRoot-pitchVector(earRoot,earPitch);
                if (headPitch != 0.f || earPitch != 0.f)
                    pose.offsets[i] = neck + pitchVector(part.offset+local-neck,headPitch)-part.offset;
            }
        }
    }

    inline void keepFeetAboveSupport(const ActorSnapshot& snapshot,
        const WildlifeVisualProfile& profile, WildlifeVisualPose& pose)
    {
        // Rotating a box about its hip can lower the toe corner below the
        // actor's copied support plane. Lift only the visual root, never query
        // or alter terrain/collision. Rabbit's existing hop takes precedence.
        float lowest = -.5f;
        for (std::size_t i=0; i<profile.partCount; ++i) {
            const auto& part = profile.parts[i];
            if (part.role != WildlifeVisualRole::Leg) continue;
            const float angle = glm::radians(pose.rotations[i].x);
            const float halfHeight = .5f * (std::abs(std::cos(angle))*part.scale.y +
                                            std::abs(std::sin(angle))*part.scale.z);
            lowest = std::min(lowest,part.offset.y+pose.offsets[i].y-halfHeight);
        }
        pose.heightOffset = std::max(pose.heightOffset,
            std::clamp(-snapshot.dimensions.y*(1.f+2.f*lowest),0.f,.04f));
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
            7.f * std::sin(snapshot.wildlifeMotionSeconds * 3.4f + individualPhase(snapshot.id))) * strength;
        // Models face -Z. A negative X pitch lowers the muzzle; all head
        // attachments orbit the same neck instead of rotating in place.
        const float headPitch = activity == WildlifeActivity::Forage ?
            -foragePitch : activity == WildlifeActivity::Flee ? 8.f * strength : 0.f;
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const auto role = profile.parts[index].role;
            if (role == WildlifeVisualRole::Leg)
                pose.rotations[index].x = stride *
                    (profile.speciesIndex == 1 ? 1.f :
                    (profile.parts[index].offset.x < 0.f ? 1.f : -1.f) *
                    (profile.parts[index].offset.z < 0.f ? 1.f : -1.f));
            if (role == WildlifeVisualRole::Head || role == WildlifeVisualRole::Muzzle ||
                role == WildlifeVisualRole::Neck || role == WildlifeVisualRole::Beak ||
                role == WildlifeVisualRole::Ear)
                pose.rotations[index].x = headPitch +
                    (role == WildlifeVisualRole::Ear && activity == WildlifeActivity::Flee
                        ? 12.f * strength : 0.f);
            if (role == WildlifeVisualRole::Wing && walking)
                pose.rotations[index].z =
                    (profile.parts[index].offset.x < 0.f ? -1.f : 1.f) *
                    std::abs(stride) * 0.24f;
        }
        if (profile.speciesIndex == 1 && walking)
            pose.heightOffset = std::max(0.f, std::sin(stridePhase)) *
                                (activity == WildlifeActivity::Flee ?
                                 0.075f : 0.035f) * strength;
        articulate(profile, pose);
        keepFeetAboveSupport(snapshot, profile, pose);
        return pose;
    }

    struct AnimatedPose {
        WildlifeVisualPose pose;
        float yawDegrees = 0.f;
    };

    // Owned by ActorVisual: bounded, disposable and never written to World.
    class PoseBlend {
      public:
        AnimatedPose update(const ActorSnapshot& snapshot, const WildlifeVisualProfile& profile,
                            glm::vec3 position, float dt, float strength)
        {
            strength = std::isfinite(strength) ? std::clamp(strength,0.f,1.f) : 0.f;
            const bool reset = !m_valid || !std::isfinite(dt) || dt < 0.f || dt > .25f ||
                glm::distance(position,m_previous) > 4.f;
            const float desiredYaw = std::remainder(snapshot.rotation.y,360.f);
            const bool moving = snapshot.wildlifeActivity == int(WildlifeActivity::Wander) ||
                                snapshot.wildlifeActivity == int(WildlifeActivity::Flee);
            if (reset) {
                m_previous = position;
                m_phase = individualPhase(snapshot.id);
                m_yaw = desiredYaw;
                m_walk = 0.f;
                m_rotations = {};
                m_height = 0.f;
                m_valid = true;
                dt = 0.f;
            }
            if (dt > 0.f) {
                const float distance = std::hypot(position.x-m_previous.x,position.z-m_previous.z);
                const float strideLength = profile.speciesIndex == 0 ? .90f :
                    profile.speciesIndex == 1 ? .55f : .65f;
                if (moving && distance <= 2.f)
                    m_phase = std::fmod(m_phase+distance*6.28318530718f/strideLength,6.28318530718f);
                const float walkTarget = moving && distance <= 2.f ?
                    std::clamp(distance/(dt*.45f),0.f,1.f) : 0.f;
                m_walk += (walkTarget-m_walk)*(-std::expm1(-dt/.08f));
                const float angle = std::remainder(desiredYaw-m_yaw,360.f);
                m_yaw = std::remainder(m_yaw+angle*(-std::expm1(-dt/.12f)),360.f);
                m_previous = position;
            }
            auto target = poseFor(snapshot,profile,m_phase,strength);
            for (std::size_t i=0; i<profile.partCount; ++i)
                if (profile.parts[i].role == WildlifeVisualRole::Leg ||
                    profile.parts[i].role == WildlifeVisualRole::Wing)
                    target.rotations[i] *= m_walk;
            target.heightOffset *= m_walk;
            const float weight = reset || strength == 0.f ? 1.f : -std::expm1(-dt/.10f);
            for (std::size_t i=0; i<profile.partCount; ++i) {
                m_rotations[i] = glm::mix(m_rotations[i],target.rotations[i],weight);
                target.rotations[i] = m_rotations[i];
            }
            m_height += (target.heightOffset-m_height)*weight;
            target.heightOffset = m_height;
            if (strength == 0.f) m_yaw = desiredYaw;
            articulate(profile,target);
            keepFeetAboveSupport(snapshot,profile,target);
            return {target,m_yaw};
        }

      private:
        std::array<glm::vec3,WildlifeVisualProfile::MaximumParts> m_rotations{};
        glm::vec3 m_previous{0.f};
        float m_phase = 0.f, m_yaw = 0.f, m_walk = 0.f, m_height = 0.f;
        bool m_valid = false;
    };
    static_assert(sizeof(PoseBlend) <= 192, "Animal animation history remains bounded");

}
