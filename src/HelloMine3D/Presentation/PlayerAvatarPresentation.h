#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include "ToolActionPresentation.h"

// Header-only, renderer-independent values for the complete player avatar.
// The future Ogre adapter may copy these values, but this layer deliberately
// has no Player, World, camera, scene-node, save, or input ownership.
namespace PlayerAvatarPresentation
{
    struct Vec3
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    enum class PartRole
    {
        Head = 0,
        Torso,
        LeftArm,
        RightArm,
        LeftLeg,
        RightLeg,
        Hair,
        Belt
    };

    enum class MaterialRole
    {
        Skin = 0,
        Tunic,
        Trousers,
        Hair,
        Accent
    };

    enum class PoseKind
    {
        Idle = 0,
        Walk,
        Airborne,
        Land,
        Tool,
        Hurt
    };

    enum class MotionStrength
    {
        Off = 0,
        Reduced,
        Full
    };

    struct PartDefinition
    {
        PartRole role = PartRole::Torso;
        MaterialRole material = MaterialRole::Tunic;
        // Root space uses the centre of the feet as (0, 0, 0). Sizes are
        // complete box extents, not half extents. Rotation is about pivot.
        Vec3 centre{};
        Vec3 size{1.f, 1.f, 1.f};
        Vec3 pivot{};
    };

    struct Profile
    {
        static constexpr std::size_t MaximumParts = 8;
        static constexpr std::size_t TrianglesPerBox = 12;
        static constexpr std::size_t MaximumTriangles =
            MaximumParts * TrianglesPerBox;

        std::array<PartDefinition, MaximumParts> parts{};
        std::size_t partCount = 0;
    };

    struct ActionFeedback
    {
        // Normalized envelopes copied from already-authoritative actions.
        // The presentation layer never starts, completes, or scores actions.
        float landing = 0.f;
        ToolActionPresentation::Pose tool{};
        float hurt = 0.f;
    };

    struct Snapshot
    {
        Vec3 position{};
        Vec3 rotationDegrees{};
        Vec3 velocity{};
        bool grounded = true;
        ActionFeedback feedback{};
        // Accumulated movement time is the gait clock. Strength is the
        // caller's normalized horizontal movement fact.
        float movementSeconds = 0.f;
        float movementStrength = 0.f;
    };

    struct PoseWeights
    {
        float idle = 1.f;
        float walk = 0.f;
        float airborne = 0.f;
        float land = 0.f;
        float tool = 0.f;
        float hurt = 0.f;
    };

    struct PartTransform
    {
        Vec3 rotationDegrees{};
        Vec3 offset{};
        Vec3 scale{1.f, 1.f, 1.f};
    };

    struct Pose
    {
        Vec3 worldPosition{};
        float facingYawDegrees = 0.f;
        Vec3 rootOffset{};
        Vec3 rootRotationDegrees{};
        std::array<PartTransform, Profile::MaximumParts> parts{};
        PoseWeights weights{};
        PoseKind kind = PoseKind::Idle;
    };

    struct SmoothingSettings
    {
        float settleSeconds = .065f;
        float actionSettleSeconds = .040f;
        float maximumDeltaSeconds = .250f;
    };

    struct PoseHistory
    {
        Pose pose{};
        bool seeded = false;
    };

    static_assert(std::is_trivially_copyable<Vec3>::value,
                  "avatar snapshots must remain copy-only values");
    static_assert(std::is_trivially_copyable<Snapshot>::value,
                  "avatar snapshots must remain copy-only values");
    static_assert(std::is_trivially_copyable<Pose>::value,
                  "avatar poses must remain copy-only values");
    static_assert(sizeof(Profile) <= 512, "avatar profile budget exceeded");
    static_assert(sizeof(Pose) <= 512, "avatar pose budget exceeded");

    inline bool finite(float value) noexcept
    {
        return std::isfinite(value);
    }

    inline float finiteOr(float value, float fallback = 0.f) noexcept
    {
        return finite(value) ? value : fallback;
    }

    inline float clamp(float value, float low, float high) noexcept
    {
        return std::clamp(finiteOr(value), low, high);
    }

    inline float clamp01(float value) noexcept
    {
        return clamp(value, 0.f, 1.f);
    }

    inline Vec3 sanitize(Vec3 value) noexcept
    {
        return {finiteOr(value.x), finiteOr(value.y), finiteOr(value.z)};
    }

    inline float wrapDegrees(float value) noexcept
    {
        if (!finite(value)) return 0.f;
        const float wrapped = std::remainder(value, 360.f);
        return wrapped == -180.f ? 180.f : wrapped;
    }

    inline float motionScale(MotionStrength strength) noexcept
    {
        switch (strength) {
        case MotionStrength::Off: return 0.f;
        case MotionStrength::Reduced: return .55f;
        case MotionStrength::Full: return 1.f;
        }
        return .55f;
    }

    inline Profile defaultProfile() noexcept
    {
        Profile result;
        const auto add = [&result](PartRole role, MaterialRole material,
                                   Vec3 centre, Vec3 size, Vec3 pivot) {
            if (result.partCount < result.parts.size())
                result.parts[result.partCount++] =
                    {role, material, centre, size, pivot};
        };

        // A compact explorer silhouette: 1.95 m including the hair cap. The
        // cap and belt provide pixel-scale identity without multiplying limbs.
        add(PartRole::Head, MaterialRole::Skin,
            {0.f, 1.66f, 0.f}, {.48f, .48f, .48f}, {0.f, 1.43f, 0.f});
        add(PartRole::Torso, MaterialRole::Tunic,
            {0.f, 1.17f, 0.f}, {.58f, .70f, .30f}, {0.f, 1.47f, 0.f});
        add(PartRole::LeftArm, MaterialRole::Tunic,
            {-.41f, 1.16f, 0.f}, {.22f, .70f, .24f}, {-.41f, 1.47f, 0.f});
        add(PartRole::RightArm, MaterialRole::Tunic,
            {.41f, 1.16f, 0.f}, {.22f, .70f, .24f}, {.41f, 1.47f, 0.f});
        add(PartRole::LeftLeg, MaterialRole::Trousers,
            {-.145f, .43f, 0.f}, {.27f, .86f, .28f}, {-.145f, .84f, 0.f});
        add(PartRole::RightLeg, MaterialRole::Trousers,
            {.145f, .43f, 0.f}, {.27f, .86f, .28f}, {.145f, .84f, 0.f});
        add(PartRole::Hair, MaterialRole::Hair,
            {0.f, 1.885f, .015f}, {.50f, .13f, .50f}, {0.f, 1.43f, 0.f});
        add(PartRole::Belt, MaterialRole::Accent,
            {0.f, .855f, -.005f}, {.60f, .10f, .32f}, {0.f, 1.47f, 0.f});
        return result;
    }

    inline std::size_t partIndex(const Profile& profile, PartRole role) noexcept
    {
        const std::size_t count = std::min(profile.partCount, profile.parts.size());
        for (std::size_t index = 0; index < count; ++index)
            if (profile.parts[index].role == role) return index;
        return count;
    }

    inline PoseKind primaryKind(const PoseWeights& weights) noexcept
    {
        if (weights.hurt > .001f) return PoseKind::Hurt;
        if (weights.tool > .001f) return PoseKind::Tool;
        if (weights.land > .001f) return PoseKind::Land;
        if (weights.airborne > .5f) return PoseKind::Airborne;
        if (weights.walk > .02f) return PoseKind::Walk;
        return PoseKind::Idle;
    }

    inline void addRotation(Pose& pose, const Profile& profile, PartRole role,
                            Vec3 value) noexcept
    {
        const std::size_t index = partIndex(profile, role);
        if (index >= std::min(profile.partCount, profile.parts.size())) return;
        pose.parts[index].rotationDegrees.x += value.x;
        pose.parts[index].rotationDegrees.y += value.y;
        pose.parts[index].rotationDegrees.z += value.z;
    }

    inline void copyTransform(Pose& pose, const Profile& profile,
                              PartRole destination, PartRole source) noexcept
    {
        const std::size_t destinationIndex = partIndex(profile, destination);
        const std::size_t sourceIndex = partIndex(profile, source);
        const std::size_t count = std::min(profile.partCount, profile.parts.size());
        if (destinationIndex < count && sourceIndex < count)
            pose.parts[destinationIndex] = pose.parts[sourceIndex];
    }

    inline void boundPose(Pose& pose, const Profile& profile) noexcept
    {
        pose.worldPosition = sanitize(pose.worldPosition);
        pose.facingYawDegrees = wrapDegrees(pose.facingYawDegrees);
        pose.rootOffset = sanitize(pose.rootOffset);
        pose.rootOffset.x = clamp(pose.rootOffset.x, -.12f, .12f);
        pose.rootOffset.y = clamp(pose.rootOffset.y, -.18f, .08f);
        pose.rootOffset.z = clamp(pose.rootOffset.z, -.12f, .12f);
        pose.rootRotationDegrees = sanitize(pose.rootRotationDegrees);
        pose.rootRotationDegrees.x = clamp(pose.rootRotationDegrees.x, -18.f, 18.f);
        pose.rootRotationDegrees.y = clamp(pose.rootRotationDegrees.y, -18.f, 18.f);
        pose.rootRotationDegrees.z = clamp(pose.rootRotationDegrees.z, -14.f, 14.f);
        const std::size_t count = std::min(profile.partCount, profile.parts.size());
        for (std::size_t index = 0; index < count; ++index) {
            PartTransform& part = pose.parts[index];
            part.rotationDegrees = sanitize(part.rotationDegrees);
            part.rotationDegrees.x = clamp(part.rotationDegrees.x, -105.f, 105.f);
            part.rotationDegrees.y = clamp(part.rotationDegrees.y, -45.f, 45.f);
            part.rotationDegrees.z = clamp(part.rotationDegrees.z, -45.f, 45.f);
            part.offset = sanitize(part.offset);
            part.offset.x = clamp(part.offset.x, -.15f, .15f);
            part.offset.y = clamp(part.offset.y, -.15f, .15f);
            part.offset.z = clamp(part.offset.z, -.15f, .15f);
            part.scale = sanitize(part.scale);
            part.scale.x = clamp(part.scale.x, .85f, 1.15f);
            part.scale.y = clamp(part.scale.y, .85f, 1.15f);
            part.scale.z = clamp(part.scale.z, .85f, 1.15f);
        }
        pose.weights.idle = clamp01(pose.weights.idle);
        pose.weights.walk = clamp01(pose.weights.walk);
        pose.weights.airborne = clamp01(pose.weights.airborne);
        pose.weights.land = clamp01(pose.weights.land);
        pose.weights.tool = clamp01(pose.weights.tool);
        pose.weights.hurt = clamp01(pose.weights.hurt);
    }

    inline Pose derivePose(const Snapshot& source, const Profile& profile,
                           MotionStrength strength = MotionStrength::Full) noexcept
    {
        Pose pose;
        pose.worldPosition = sanitize(source.position);
        pose.facingYawDegrees = wrapDegrees(source.rotationDegrees.y);

        const float scale = motionScale(strength);
        pose.weights.walk = source.grounded ? clamp01(source.movementStrength) : 0.f;
        pose.weights.airborne = source.grounded ? 0.f : 1.f;
        pose.weights.land = source.grounded ? clamp01(source.feedback.landing) : 0.f;
        const auto& action = source.feedback.tool;
        pose.weights.tool = clamp01(action.activity());
        pose.weights.hurt = clamp01(source.feedback.hurt);
        const float active = std::max({pose.weights.walk, pose.weights.airborne,
            pose.weights.land, pose.weights.tool, pose.weights.hurt});
        pose.weights.idle = 1.f - clamp01(active);
        pose.kind = primaryKind(pose.weights);

        // Looking direction is an actual copied orientation cue rather than
        // decorative motion, so the accessibility strength does not remove it.
        addRotation(pose, profile, PartRole::Head,
                    {clamp(source.rotationDegrees.x, -35.f, 35.f), 0.f, 0.f});

        const float gaitClock = finiteOr(source.movementSeconds);
        const float gait = std::sin(std::remainder(gaitClock * 8.2f, 6.28318530718f));
        const float legSwing = gait * 32.f * pose.weights.walk * scale;
        const float armSwing = gait * 25.f * pose.weights.walk * scale;
        addRotation(pose, profile, PartRole::LeftLeg, {legSwing, 0.f, 0.f});
        addRotation(pose, profile, PartRole::RightLeg, {-legSwing, 0.f, 0.f});
        addRotation(pose, profile, PartRole::LeftArm, {-armSwing, 0.f, -2.f * scale});
        addRotation(pose, profile, PartRole::RightArm, {armSwing, 0.f, 2.f * scale});
        pose.rootOffset.y += std::abs(gait) * .018f * pose.weights.walk * scale;
        pose.rootRotationDegrees.x += 3.f * pose.weights.walk * scale;

        if (pose.weights.airborne > 0.f) {
            const float rise = clamp(source.velocity.y * .12f, -1.f, 1.f);
            addRotation(pose, profile, PartRole::LeftLeg,
                        {(-13.f - rise * 8.f) * scale, 0.f, 0.f});
            addRotation(pose, profile, PartRole::RightLeg,
                        {(7.f - rise * 5.f) * scale, 0.f, 0.f});
            addRotation(pose, profile, PartRole::LeftArm,
                        {(16.f + rise * 5.f) * scale, 0.f, 0.f});
            addRotation(pose, profile, PartRole::RightArm,
                        {(16.f + rise * 5.f) * scale, 0.f, 0.f});
            pose.rootRotationDegrees.x += -rise * 5.f * scale;
        }

        const float land = pose.weights.land * scale;
        if (land > 0.f) {
            addRotation(pose, profile, PartRole::LeftLeg, {24.f * land, 0.f, 0.f});
            addRotation(pose, profile, PartRole::RightLeg, {24.f * land, 0.f, 0.f});
            addRotation(pose, profile, PartRole::LeftArm, {-10.f * land, 0.f, 0.f});
            addRotation(pose, profile, PartRole::RightArm, {-10.f * land, 0.f, 0.f});
            pose.rootOffset.y -= .14f * land;
            pose.rootRotationDegrees.x += 7.f * land;
        }

        const float preparation = clamp01(action.preparation) * scale;
        const float strike = clamp01(action.strike) * scale;
        const float use = clamp01(action.use) * scale;
        const float consume = clamp01(action.consume) * scale;
        // Local forward is -Z. Positive X pitch raises a hanging arm toward
        // the target; the previous negative pitch swung it behind the body.
        const float activity = std::max({preparation, strike, use, consume});
        if (activity > 0.f) {
            const auto arm = partIndex(profile, PartRole::RightArm);
            if (arm < std::min(profile.partCount, profile.parts.size()))
                pose.parts[arm].rotationDegrees.x *= 1.f - activity;
            addRotation(pose, profile, PartRole::RightArm,
                        {102.f * preparation + 48.f * strike + 64.f * use + 104.f * consume,
                         -8.f * preparation + 12.f * strike + 28.f * consume,
                         12.f * preparation + 7.f * strike - 26.f * consume});
            addRotation(pose, profile, PartRole::LeftArm,
                        {12.f * preparation + 9.f * strike + 10.f * consume,
                         0.f, -4.f * activity});
            // Keep shoulder sockets and the eight-part body connected. A
            // small whole-body twist conveys load without twisting only the
            // torso away from its sibling arm nodes.
            pose.rootRotationDegrees.y += -4.f * preparation + 5.f * strike;
        }

        const float hurt = pose.weights.hurt * scale;
        if (hurt > 0.f) {
            addRotation(pose, profile, PartRole::Head, {-8.f * hurt, 0.f, 5.f * hurt});
            addRotation(pose, profile, PartRole::LeftArm, {14.f * hurt, 0.f, -9.f * hurt});
            addRotation(pose, profile, PartRole::RightArm, {14.f * hurt, 0.f, 9.f * hurt});
            pose.rootOffset.z += .07f * hurt;
            pose.rootRotationDegrees.z += 11.f * hurt;
        }

        // Cosmetic layers use the exact parent articulation. The renderer can
        // therefore keep cap/head and belt/torso seams connected.
        copyTransform(pose, profile, PartRole::Hair, PartRole::Head);
        copyTransform(pose, profile, PartRole::Belt, PartRole::Torso);
        boundPose(pose, profile);
        return pose;
    }

    inline float blendScalar(float from, float to, float weight) noexcept
    {
        return from + (to - from) * weight;
    }

    inline Vec3 blendVector(Vec3 from, Vec3 to, float weight) noexcept
    {
        return {blendScalar(from.x, to.x, weight),
                blendScalar(from.y, to.y, weight),
                blendScalar(from.z, to.z, weight)};
    }

    // Render-owned, frame-rate-independent articulation smoothing. World
    // position and facing stay on the newest copied snapshot; this function
    // never predicts movement through collision geometry.
    inline Pose smoothPose(PoseHistory& history, Pose target,
                           const Profile& profile, float deltaSeconds,
                           SmoothingSettings settings = {}) noexcept
    {
        const float maximumDelta = clamp(settings.maximumDeltaSeconds, .05f, .250f);
        if (!history.seeded || !finite(deltaSeconds) || deltaSeconds < 0.f ||
            deltaSeconds > maximumDelta) {
            boundPose(target, profile);
            history.pose = target;
            history.seeded = true;
            return target;
        }

        const bool fastAction = target.weights.tool > .001f ||
                                target.weights.hurt > .001f ||
                                target.weights.land > .001f;
        const float requestedSettle = fastAction ? settings.actionSettleSeconds
                                                 : settings.settleSeconds;
        const float settle = clamp(requestedSettle, .010f, .250f);
        const float dt = std::clamp(deltaSeconds, 0.f, maximumDelta);
        const float weight = -std::expm1(-dt / settle);

        // Keep authoritative placement immediate. Only presentation deltas
        // settle, avoiding camera/player drift and teleport trails.
        target.rootOffset = blendVector(history.pose.rootOffset,
                                        target.rootOffset, weight);
        target.rootRotationDegrees = blendVector(
            history.pose.rootRotationDegrees, target.rootRotationDegrees, weight);
        const std::size_t count = std::min(profile.partCount, profile.parts.size());
        for (std::size_t index = 0; index < count; ++index) {
            target.parts[index].rotationDegrees = blendVector(
                history.pose.parts[index].rotationDegrees,
                target.parts[index].rotationDegrees, weight);
            target.parts[index].offset = blendVector(history.pose.parts[index].offset,
                                                      target.parts[index].offset,
                                                      weight);
            target.parts[index].scale = blendVector(history.pose.parts[index].scale,
                                                     target.parts[index].scale,
                                                     weight);
        }
        boundPose(target, profile);
        history.pose = target;
        return target;
    }
}
