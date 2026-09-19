#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>

#include "Actor.h"

enum class EnemyVisualArchetype
{
    Generic = 0,
    Stalker,
    Brute,
    Spitter
};

enum class EnemyVisualPartRole
{
    Torso = 0,
    Head,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Muzzle,
    Crest,
    Neck
};

struct EnemyVisualPartDefinition
{
    EnemyVisualPartRole role = EnemyVisualPartRole::Torso;
    glm::vec3 offset{0.f};
    glm::vec3 scale{1.f};
};

struct EnemyVisualProfile
{
    static constexpr std::size_t MaximumParts = 8;

    EnemyVisualArchetype archetype = EnemyVisualArchetype::Generic;
    std::array<EnemyVisualPartDefinition, MaximumParts> parts{};
    std::size_t partCount = 0;
    bool waystoneGuardian = false;
};

struct EnemyVisualPose
{
    std::array<glm::vec3, EnemyVisualProfile::MaximumParts> rotations{};
    std::array<glm::vec3, EnemyVisualProfile::MaximumParts> offsets{};
    std::array<float, EnemyVisualProfile::MaximumParts> scales{{
        1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f}};
    float rootPitch = 0.f;
    float rootRoll = 0.f;
    float rootYOffset = 0.f;
    float rootScale = 1.f;
};

namespace EnemyPresentation
{
    inline constexpr int DeathPoseTicks = 8;
    inline constexpr std::size_t MaximumDeathPoses = 32;

    // Owned by one render visual, not the Actor or its save state. Accumulated
    // horizontal travel avoids the x+z cancellation on diagonal paths.
    class GaitPhase
    {
      public:
        float update(const glm::vec3& position, bool walking)
        {
            if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                !std::isfinite(position.z)) {
                *this = GaitPhase{};
                return m_phase;
            }
            if (m_seeded && walking) {
                const double dx = static_cast<double>(position.x) - m_previous.x;
                const double dz = static_cast<double>(position.z) - m_previous.z;
                const double distance = std::hypot(dx, dz);
                // A normal fixed-tick stride is much smaller than two metres.
                // Rebase teleports without turning them into frantic steps.
                if (distance <= 2.0)
                    m_phase = static_cast<float>(std::fmod(
                        m_phase + distance * 4.5, 6.283185307179586));
            }
            m_previous = position;
            m_seeded = true;
            return m_phase;
        }

      private:
        glm::vec3 m_previous{0.f};
        float m_phase = 0.f;
        bool m_seeded = false;
    };

    // Match Ogre's yaw * pitch * roll order (roll is applied first).
    inline glm::vec3 rotatePart(glm::vec3 point, const glm::vec3& degrees)
    {
        const glm::vec3 a = glm::radians(degrees);
        point = {point.x * std::cos(a.z) - point.y * std::sin(a.z),
                 point.x * std::sin(a.z) + point.y * std::cos(a.z), point.z};
        point = {point.x, point.y * std::cos(a.x) - point.z * std::sin(a.x),
                 point.y * std::sin(a.x) + point.z * std::cos(a.x)};
        return {point.x * std::cos(a.y) + point.z * std::sin(a.y), point.y,
                -point.x * std::sin(a.y) + point.z * std::cos(a.y)};
    }

    inline bool isWaystoneGuardianType(const std::string &type) noexcept
    {
        return type == "hellomine:waystone_stalker" ||
               type == "hellomine:waystone_brute";
    }

    inline EnemyVisualProfile profileForType(const std::string &type)
    {
        EnemyVisualProfile profile;
        profile.waystoneGuardian = isWaystoneGuardianType(type);
        const bool stalker = type == "hellomine:stalker" ||
                             type == "hellomine:waystone_stalker";
        const bool brute = type == "hellomine:brute" ||
                           type == "hellomine:waystone_brute";
        const bool spitter = type == "hellomine:spitter";

        const auto add = [&profile](EnemyVisualPartRole role,
                                    const glm::vec3 &offset,
                                    const glm::vec3 &scale) {
            if (profile.partCount < profile.parts.size()) {
                profile.parts[profile.partCount++] = {role, offset, scale};
            }
        };

        if (stalker) {
            profile.archetype = EnemyVisualArchetype::Stalker;
            add(EnemyVisualPartRole::Torso, {0.f, 0.04f, 0.f},
                {0.42f, 0.48f, 0.34f});
            add(EnemyVisualPartRole::Head, {0.f, 0.39f, -0.02f},
                {0.34f, 0.25f, 0.32f});
            add(EnemyVisualPartRole::LeftArm, {-0.28f, 0.02f, 0.f},
                {0.16f, 0.56f, 0.16f});
            add(EnemyVisualPartRole::RightArm, {0.28f, 0.02f, 0.f},
                {0.16f, 0.56f, 0.16f});
            add(EnemyVisualPartRole::LeftLeg, {-0.13f, -0.355f, 0.f},
                {0.16f, 0.33f, 0.19f});
            add(EnemyVisualPartRole::RightLeg, {0.13f, -0.355f, 0.f},
                {0.16f, 0.33f, 0.19f});
        }
        else if (brute) {
            profile.archetype = EnemyVisualArchetype::Brute;
            add(EnemyVisualPartRole::Torso, {0.f, 0.02f, 0.f},
                {0.70f, 0.52f, 0.52f});
            add(EnemyVisualPartRole::Head, {0.f, 0.38f, -0.04f},
                {0.42f, 0.27f, 0.40f});
            add(EnemyVisualPartRole::LeftArm, {-0.46f, 0.f, 0.f},
                {0.27f, 0.55f, 0.31f});
            add(EnemyVisualPartRole::RightArm, {0.46f, 0.f, 0.f},
                {0.27f, 0.55f, 0.31f});
            add(EnemyVisualPartRole::LeftLeg, {-0.20f, -0.37f, 0.f},
                {0.25f, 0.30f, 0.29f});
            add(EnemyVisualPartRole::RightLeg, {0.20f, -0.37f, 0.f},
                {0.25f, 0.30f, 0.29f});
        }
        else if (spitter) {
            profile.archetype = EnemyVisualArchetype::Spitter;
            add(EnemyVisualPartRole::Torso, {0.f, -0.08f, 0.04f},
                {0.66f, 0.36f, 0.72f});
            add(EnemyVisualPartRole::Head, {0.f, 0.23f, -0.18f},
                {0.38f, 0.29f, 0.40f});
            add(EnemyVisualPartRole::Muzzle, {0.f, 0.23f, -0.43f},
                {0.19f, 0.18f, 0.26f});
            add(EnemyVisualPartRole::LeftLeg, {-0.28f, -0.34f, -0.18f},
                {0.16f, 0.31f, 0.17f});
            add(EnemyVisualPartRole::RightLeg, {0.28f, -0.34f, -0.18f},
                {0.16f, 0.31f, 0.17f});
            add(EnemyVisualPartRole::LeftArm, {-0.28f, -0.34f, 0.25f},
                {0.16f, 0.31f, 0.17f});
            add(EnemyVisualPartRole::RightArm, {0.28f, -0.34f, 0.25f},
                {0.16f, 0.31f, 0.17f});
            // A fixed neck bridges the torso and rotating head with visible
            // volume; a shared pivot alone only leaves a thin contact sliver.
            add(EnemyVisualPartRole::Neck, {0.f, 0.13f, -0.12f},
                {0.18f, 0.16f, 0.22f});
        }
        else {
            add(EnemyVisualPartRole::Torso, glm::vec3(0.f),
                glm::vec3(1.f));
        }

        if (profile.waystoneGuardian) {
            add(EnemyVisualPartRole::Crest, {0.f, 0.58f, 0.f},
                {0.18f, 0.18f, 0.18f});
        }
        return profile;
    }

    // Rebuild offsets after any angle/scale blend. Interpolating the already
    // anchored offsets independently would pull shoulder and neck joints apart.
    inline void anchorParts(const EnemyVisualProfile& profile, EnemyVisualPose& pose)
    {
        std::size_t head = profile.partCount;
        for (std::size_t index = 0; index < profile.partCount; ++index)
            if (profile.parts[index].role == EnemyVisualPartRole::Head) head = index;
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const auto& part = profile.parts[index];
            if (part.role == EnemyVisualPartRole::LeftArm ||
                part.role == EnemyVisualPartRole::RightArm ||
                part.role == EnemyVisualPartRole::LeftLeg ||
                part.role == EnemyVisualPartRole::RightLeg) {
                const glm::vec3 pivot(0.f, part.scale.y * .5f, 0.f);
                pose.offsets[index] = pivot - rotatePart(
                    pivot * pose.scales[index], pose.rotations[index]);
            }
            else if (head < profile.partCount &&
                     (part.role == EnemyVisualPartRole::Head ||
                      part.role == EnemyVisualPartRole::Muzzle)) {
                const auto& headPart = profile.parts[head];
                const glm::vec3 neck = headPart.offset + glm::vec3(
                    0.f, -.5f * headPart.scale.y, .25f * headPart.scale.z);
                // Muzzle shares the head's complete neck transform, including
                // Idle yaw and Windup scale; their seam cannot pull apart.
                pose.rotations[index] = pose.rotations[head];
                pose.scales[index] = pose.scales[head];
                pose.offsets[index] = neck + rotatePart(
                    (part.offset - neck) * pose.scales[index],
                    pose.rotations[index]) - part.offset;
            }
        }
    }

    inline EnemyVisualPose poseFor(const ActorSnapshot &snapshot,
                                   const EnemyVisualProfile &profile,
                                   float travelPhase = 0.f)
    {
        EnemyVisualPose pose;
        const float stateProgress = snapshot.combatStateTicksTotal > 0
            ? 1.f - std::clamp(
                  static_cast<float>(snapshot.combatStateTicksRemaining) /
                      static_cast<float>(snapshot.combatStateTicksTotal),
                  0.f, 1.f)
            : 0.f;
        const float gait = std::sin(
            (std::isfinite(travelPhase) ? travelPhase : 0.f) +
            static_cast<float>(snapshot.id % 17u) * 0.37f);

        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const EnemyVisualPartRole role = profile.parts[index].role;
            if (snapshot.combatState == MobCombatState::Chase) {
                if (role == EnemyVisualPartRole::LeftLeg ||
                    role == EnemyVisualPartRole::RightArm) {
                    pose.rotations[index].x = gait * 28.f;
                }
                else if (role == EnemyVisualPartRole::RightLeg ||
                         role == EnemyVisualPartRole::LeftArm) {
                    pose.rotations[index].x = -gait * 28.f;
                }
                pose.rootYOffset = std::abs(gait) * 0.015f;
            }
            else if (snapshot.combatState == MobCombatState::Idle &&
                     role == EnemyVisualPartRole::Head) {
                pose.rotations[index].y = std::sin(
                    static_cast<float>(snapshot.id % 23u) * 0.41f) * 7.f;
            }

            if (snapshot.combatState == MobCombatState::Windup) {
                if (profile.archetype == EnemyVisualArchetype::Stalker &&
                    role == EnemyVisualPartRole::RightArm) {
                    pose.rotations[index].x = -30.f - stateProgress * 95.f;
                }
                else if (profile.archetype == EnemyVisualArchetype::Brute &&
                         (role == EnemyVisualPartRole::LeftArm ||
                          role == EnemyVisualPartRole::RightArm)) {
                    pose.rotations[index].x = -20.f - stateProgress * 85.f;
                }
                else if (profile.archetype == EnemyVisualArchetype::Spitter &&
                         (role == EnemyVisualPartRole::Head ||
                          role == EnemyVisualPartRole::Muzzle)) {
                    pose.rotations[index].x = -stateProgress * 24.f;
                    pose.scales[index] = 1.f + stateProgress * 0.16f;
                }
                else if (role == EnemyVisualPartRole::Crest) {
                    pose.rotations[index].y = stateProgress * 180.f;
                    pose.scales[index] = 1.f + stateProgress * 0.28f;
                }
                pose.rootPitch = -stateProgress *
                    (profile.archetype == EnemyVisualArchetype::Brute
                         ? 10.f : 5.f);
            }
            else if (snapshot.combatState == MobCombatState::Recover) {
                const float followThrough = 1.f - stateProgress;
                if (profile.archetype == EnemyVisualArchetype::Stalker &&
                    role == EnemyVisualPartRole::RightArm) {
                    pose.rotations[index].x = 78.f * followThrough;
                }
                else if (profile.archetype == EnemyVisualArchetype::Brute &&
                         (role == EnemyVisualPartRole::LeftArm ||
                          role == EnemyVisualPartRole::RightArm)) {
                    pose.rotations[index].x = 52.f * followThrough;
                }
                else if (profile.archetype == EnemyVisualArchetype::Spitter &&
                         (role == EnemyVisualPartRole::Head ||
                          role == EnemyVisualPartRole::Muzzle)) {
                    pose.rotations[index].x = 18.f * followThrough;
                }
            }
        }

        anchorParts(profile, pose);

        pose.rootRoll += (snapshot.id % 2u == 0u ? 1.f : -1.f) *
            snapshot.hitFeedback * 9.f;
        pose.rootScale += snapshot.hitFeedback * 0.08f;

        if (snapshot.deathPresentation &&
            snapshot.deathPresentationTicksTotal > 0) {
            const float deathProgress = 1.f - std::clamp(
                static_cast<float>(
                    snapshot.deathPresentationTicksRemaining) /
                    static_cast<float>(snapshot.deathPresentationTicksTotal),
                0.f, 1.f);
            pose.rootRoll += (snapshot.id % 2u == 0u ? 1.f : -1.f) *
                deathProgress * 82.f;
            pose.rootYOffset -= deathProgress * 0.30f;
            pose.rootScale *= 1.f - deathProgress * 0.24f;
        }
        return pose;
    }

    // Bounded, render-owned pose history. Combat facts and hit/death feedback
    // remain immediate; only limb articulation and body lean settle over frames.
    class PoseBlend
    {
      public:
        EnemyVisualPose update(const ActorSnapshot& snapshot,
            const EnemyVisualProfile& profile, EnemyVisualPose target,
            float deltaSeconds)
        {
            const float travel = m_seeded
                ? glm::length(snapshot.position - m_previousPosition) : 0.f;
            const bool reset = !m_seeded || snapshot.deathPresentation ||
                m_wasDead || !std::isfinite(deltaSeconds) || deltaSeconds < 0.f ||
                deltaSeconds > .25f || !std::isfinite(travel) || travel > 2.f;
            m_previousPosition = snapshot.position;
            m_wasDead = snapshot.deathPresentation;
            if (!reset) {
                const float settleSeconds = snapshot.combatState == MobCombatState::Recover
                    ? .035f : snapshot.combatState == MobCombatState::Windup ? .045f : .06f;
                const float weight = -std::expm1(-deltaSeconds / settleSeconds);
                for (std::size_t i = 0; i < profile.partCount; ++i) {
                    target.rotations[i] = m_pose.rotations[i] +
                        (target.rotations[i] - m_pose.rotations[i]) * weight;
                    target.scales[i] = m_pose.scales[i] +
                        (target.scales[i] - m_pose.scales[i]) * weight;
                }
                target.rootPitch = m_pose.rootPitch +
                    (target.rootPitch - m_pose.rootPitch) * weight;
                target.rootYOffset = m_pose.rootYOffset +
                    (target.rootYOffset - m_pose.rootYOffset) * weight;
            }
            anchorParts(profile, target);
            m_pose = target;
            m_seeded = true;
            return target;
        }

      private:
        EnemyVisualPose m_pose;
        glm::vec3 m_previousPosition{0.f};
        bool m_seeded = false;
        bool m_wasDead = false;
    };

}
