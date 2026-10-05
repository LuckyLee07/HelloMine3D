#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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
    // Follow copied, successful World segments in order. In particular, never
    // join skipped step corners with an untested diagonal. This is disposable
    // presentation state; collision and wildlife decisions keep World positions.
    class MotionBlend {
      public:
        enum class ResetReason {
            None,
            Initial,
            Teleport,
            HistoryGap,
            QueueOverflow,
            InvalidHistory,
            LongFrame
        };

        glm::vec3 update(const ActorSnapshot& snapshot, float dt)
        {
            if (!m_valid) {
                if (finite(snapshot.position)) {
                    const auto& history = snapshot.wildlifeMotionHistory;
                    const bool neutral = history.count == 0 && history.newestSequence == 0;
                    const bool valid = neutral ||
                        (validHistory(history) && history.segments[history.count - 1].to ==
                                                   snapshot.position);
                    reset(snapshot, valid ? ResetReason::Initial : ResetReason::InvalidHistory);
                }
                else m_resetReason = ResetReason::InvalidHistory;
                return m_position;
            }
            // Paused/invalid frames may carry newer snapshots. Do not consume
            // their sequence, replace the target, or rebase an in-flight path.
            if (!std::isfinite(dt) || dt <= 0.f) return m_position;
            m_resetReason = ResetReason::None;
            if (!finite(snapshot.position)) {
                m_resetReason = ResetReason::InvalidHistory;
                return m_position;
            }
            if (dt > .25f) return reset(snapshot, ResetReason::LongFrame);

            const auto& history = snapshot.wildlifeMotionHistory;
            if (history.count == 0 && history.newestSequence == 0) {
                if (m_usingHistory) return reset(snapshot, ResetReason::HistoryGap);
                return updateLegacy(snapshot, dt);
            }
            if (!validHistory(history))
                return reset(snapshot, ResetReason::InvalidHistory);
            if (history.segments[history.count - 1].to != snapshot.position)
                return reset(snapshot, ResetReason::Teleport);
            if (history.newestSequence < m_seenSequence)
                return reset(snapshot, ResetReason::HistoryGap);
            if (history.newestSequence == m_seenSequence) {
                if (snapshot.position != m_target)
                    return reset(snapshot, ResetReason::Teleport);
            }
            else {
                std::size_t first = 0;
                while (first < history.count &&
                       history.segments[first].sequence <= m_seenSequence) ++first;
                if (first == history.count ||
                    m_seenSequence == std::numeric_limits<std::uint64_t>::max() ||
                    history.segments[first].sequence != m_seenSequence + 1 ||
                    history.segments[first].from != m_target ||
                    (m_count == 0 && m_position != m_target))
                    return reset(snapshot, ResetReason::HistoryGap);
                if (m_count + history.count - first > m_queue.size())
                    return reset(snapshot, ResetReason::QueueOverflow);
                for (; first < history.count; ++first)
                    m_queue[m_count++] = {history.segments[first], 0.f,
                                         history.segments[first].seconds};
                m_target = snapshot.position;
                m_seenSequence = history.newestSequence;
                m_usingHistory = true;

                // Compress only remaining time, preserving the current segment
                // fraction and every checked corner. A backlog catches up in
                // at most .20 s without dropping a movement segment.
                float remaining = 0.f;
                for (std::size_t i = 0; i < m_count; ++i)
                    remaining += m_queue[i].remainingSeconds;
                if (remaining > .20f)
                    for (std::size_t i = 0; i < m_count; ++i)
                        m_queue[i].remainingSeconds *= .20f / remaining;
            }

            float available = std::min(dt, .20f);
            while (m_count != 0 && available > 0.f) {
                auto& queued = m_queue[0];
                // Float subtraction can leave a few nanoseconds after a full
                // .20 s catch-up. Finish within its time representation error.
                constexpr float timeRoundoff =
                    8.f * std::numeric_limits<float>::epsilon() * .20f;
                if (queued.remainingSeconds - available > timeRoundoff) {
                    queued.fraction += (1.f - queued.fraction) *
                        (available / queued.remainingSeconds);
                    queued.remainingSeconds -= available;
                    m_position = pointOnPath(queued.segment, queued.fraction);
                    available = 0.f;
                }
                else {
                    m_position = queued.segment.to;
                    available -= queued.remainingSeconds;
                    for (std::size_t i = 1; i < m_count; ++i)
                        m_queue[i - 1] = m_queue[i];
                    --m_count;
                }
            }
            return m_position;
        }

        ResetReason resetReason() const noexcept { return m_resetReason; }
        std::size_t pendingSegmentCount() const noexcept { return m_count; }
        std::uint64_t seenSequence() const noexcept { return m_seenSequence; }

      private:
        struct QueuedSegment {
            WildlifeMotionSegment segment;
            float fraction = 0.f;
            float remainingSeconds = 0.f;
        };

        static bool finite(const glm::vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                   std::isfinite(value.z);
        }

        static bool validHistory(const WildlifeMotionHistory& history)
        {
            if (history.count == 0 || history.count > history.segments.size()) return false;
            for (std::size_t i = 0; i < history.count; ++i) {
                const auto& segment = history.segments[i];
                const auto worldPosition = [](const glm::vec3& value) {
                    return finite(value) && std::abs(double(value.x)) < 2147483000. &&
                        std::abs(double(value.z)) < 2147483000. &&
                        value.y >= 1.f && value.y <= 256.f;
                };
                if (segment.sequence == 0 || !worldPosition(segment.from) ||
                    !worldPosition(segment.to) ||
                    segment.from == segment.to || !std::isfinite(segment.seconds) ||
                    segment.seconds <= 0.f || segment.seconds > .20f ||
                    std::abs(segment.to.x - segment.from.x) > .61f ||
                    std::abs(segment.to.z - segment.from.z) > .61f)
                    return false;
                const float rise = segment.to.y - segment.from.y;
                switch (segment.kind) {
                    case WildlifeMotionPath::GroundedLevel:
                        // World's level branch rounds a fractional start to
                        // the checked grid support, at most half a block away.
                        if (std::abs(rise) > .5001f) return false;
                        break;
                    case WildlifeMotionPath::SupportRise:
                        if (rise <= 0.f || rise > 1.5001f) return false;
                        break;
                    case WildlifeMotionPath::SupportDescent:
                        if (rise >= 0.f || rise < -1.5001f) return false;
                        break;
                    case WildlifeMotionPath::AirborneFall:
                        if (segment.from.x != segment.to.x || segment.from.z != segment.to.z ||
                            rise >= 0.f || rise < -.8001f) return false;
                        break;
                    default: return false;
                }
                if (i != 0 &&
                    (history.segments[i - 1].sequence ==
                         std::numeric_limits<std::uint64_t>::max() ||
                     segment.sequence != history.segments[i - 1].sequence + 1 ||
                     segment.from != history.segments[i - 1].to)) return false;
            }
            return history.segments[history.count - 1].sequence == history.newestSequence;
        }

        static glm::vec3 pointOnPath(const WildlifeMotionSegment& segment, float fraction)
        {
            if (fraction <= 0.f) return segment.from;
            if (fraction >= 1.f) return segment.to;
            if (segment.kind == WildlifeMotionPath::AirborneFall)
                return glm::mix(segment.from, segment.to, fraction);
            const float horizontal = std::hypot(segment.to.x - segment.from.x,
                                                segment.to.z - segment.from.z);
            const float rise = segment.to.y - segment.from.y;
            const float vertical = std::abs(rise);
            if (vertical == 0.f) return glm::mix(segment.from, segment.to, fraction);
            const float distance = fraction * (horizontal + vertical);
            if (rise > 0.f) {
                const glm::vec3 corner(segment.from.x, segment.to.y, segment.from.z);
                return distance <= vertical
                    ? glm::mix(segment.from, corner, distance / vertical)
                    : glm::mix(corner, segment.to, (distance - vertical) / horizontal);
            }
            const glm::vec3 corner(segment.to.x, segment.from.y, segment.to.z);
            return distance <= horizontal && horizontal > 0.f
                ? glm::mix(segment.from, corner, distance / horizontal)
                : glm::mix(corner, segment.to, (distance - horizontal) / vertical);
        }

        glm::vec3 reset(const ActorSnapshot& snapshot, ResetReason reason)
        {
            m_legacyFrom = m_target = m_position = snapshot.position;
            m_lastMoveTime = std::isfinite(snapshot.wildlifeMotionSeconds)
                ? snapshot.wildlifeMotionSeconds : 0.f;
            m_legacyElapsed = m_legacyDuration = .05f;
            m_seenSequence = snapshot.wildlifeMotionHistory.newestSequence;
            m_usingHistory = snapshot.wildlifeMotionHistory.count != 0 || m_seenSequence != 0;
            m_count = 0;
            m_resetReason = reason;
            m_valid = true;
            return m_position;
        }

        glm::vec3 updateLegacy(const ActorSnapshot& snapshot, float dt)
        {
            // Neutral history is retained for the existing diagnostic gallery.
            // Production wildlife always publishes accepted movement records.
            if (glm::distance(snapshot.position, m_target) > 4.f)
                return reset(snapshot, ResetReason::Teleport);
            if (snapshot.position != m_target) {
                m_legacyFrom = m_position;
                m_target = snapshot.position;
                const float elapsed = snapshot.wildlifeMotionSeconds - m_lastMoveTime;
                m_legacyDuration = std::isfinite(elapsed)
                    ? std::clamp(elapsed, .05f, .20f) : .05f;
                m_lastMoveTime = snapshot.wildlifeMotionSeconds;
                m_legacyElapsed = 0.f;
            }
            m_legacyElapsed = std::min(m_legacyDuration,
                m_legacyElapsed + std::min(dt, .20f));
            m_position = glm::mix(m_legacyFrom, m_target,
                                  m_legacyElapsed / m_legacyDuration);
            return m_position;
        }

        std::array<QueuedSegment, WildlifeMotionHistory::MaximumSegments> m_queue{};
        std::size_t m_count = 0;
        std::uint64_t m_seenSequence = 0;
        ResetReason m_resetReason = ResetReason::None;
        bool m_valid = false;
        bool m_usingHistory = false;
        glm::vec3 m_legacyFrom{0.f}, m_target{0.f}, m_position{0.f};
        float m_lastMoveTime = 0.f, m_legacyElapsed = 0.f, m_legacyDuration = .05f;
    };
    static_assert(sizeof(MotionBlend) <= 512, "Animal movement queue remains bounded");

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
            if (strength == 0.f && (reset || dt > 0.f)) m_yaw = desiredYaw;
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
