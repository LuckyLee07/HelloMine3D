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
    Crest
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
            add(EnemyVisualPartRole::LeftArm, {-0.32f, 0.02f, 0.f},
                {0.16f, 0.56f, 0.16f});
            add(EnemyVisualPartRole::RightArm, {0.32f, 0.02f, 0.f},
                {0.16f, 0.56f, 0.16f});
            add(EnemyVisualPartRole::LeftLeg, {-0.13f, -0.37f, 0.f},
                {0.16f, 0.30f, 0.19f});
            add(EnemyVisualPartRole::RightLeg, {0.13f, -0.37f, 0.f},
                {0.16f, 0.30f, 0.19f});
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

    inline EnemyVisualPose poseFor(const ActorSnapshot &snapshot,
                                   const EnemyVisualProfile &profile)
    {
        EnemyVisualPose pose;
        const float stateProgress = snapshot.combatStateTicksTotal > 0
            ? 1.f - std::clamp(
                  static_cast<float>(snapshot.combatStateTicksRemaining) /
                      static_cast<float>(snapshot.combatStateTicksTotal),
                  0.f, 1.f)
            : 0.f;
        const float gait = std::sin(
            (snapshot.position.x + snapshot.position.z) * 4.5f +
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
}
