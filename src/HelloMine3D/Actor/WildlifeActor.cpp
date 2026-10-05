#include "WildlifeActor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Player/Player.h"
#include "../World/World.h"

namespace {
    glm::vec3 dimensionsFor(const std::string &type)
    {
        if (type == WildlifeSpecies::Sheep) return {0.42f, 0.55f, 0.30f};
        if (type == WildlifeSpecies::Rabbit) return {0.22f, 0.27f, 0.22f};
        return {0.25f, 0.36f, 0.23f};
    }

    float speedFor(const std::string &type, bool fleeing)
    {
        if (type == WildlifeSpecies::Rabbit) return fleeing ? 3.0f : 1.35f;
        if (type == WildlifeSpecies::MarshBird) return fleeing ? 2.1f : 0.70f;
        return fleeing ? 2.25f : 0.75f;
    }
}

WildlifeActor::WildlifeActor(ActorId id, const std::string &type,
                             const glm::vec3 &actorPosition)
    : LivingActor(id, type, actorPosition, dimensionsFor(type),
                  type == WildlifeSpecies::Sheep ? 8.f : 4.f)
    , m_home(actorPosition)
    , m_headingRadians(static_cast<float>(id % 628u) * 0.01f)
{
}

void WildlifeActor::tick(World &world, float dt)
{
    LivingActor::tick(world, dt);
    if (!isAlive() || dt <= 0.f) return;
    m_ageSeconds += dt;
    m_decisionClock += dt;
    m_motionElapsed = std::min(0.20f, m_motionElapsed + dt);
    m_alarmSeconds = std::max(0.f, m_alarmSeconds - dt);

    const bool decide = m_decisionClock >= 0.20f;
    if (decide) {
        m_decisionClock = 0.f;
        // Failed turns remain relative to the intended direction. Wander
        // does not recompute an away heading, so undo its failed side before
        // the ordinary home/phase decision can update that intention.
        if (m_hasBlockedTurn) m_headingRadians = m_blockedHeadingRadians;
        const Player *player = world.getPlayer();
        const glm::vec3 relative = player != nullptr
            ? position - player->position : glm::vec3(1000.f, 0.f, 0.f);
        const float playerDistance = std::hypot(relative.x, relative.z);
        if (player != nullptr && playerDistance < 6.f) {
            m_alarmSeconds = getType() == WildlifeSpecies::Rabbit ? 4.f : 2.5f;
        }
        if (m_alarmSeconds > 0.f) {
            m_activity = WildlifeActivity::Flee;
            if (playerDistance > 0.001f)
                m_headingRadians = std::atan2(relative.x, -relative.z);
        }
        else {
            // Species have different dwell rhythms. Decisions happen at
            // most once per 0.20 s, with no random allocation or path search.
            const int phase = static_cast<int>(m_ageSeconds *
                (getType() == WildlifeSpecies::Rabbit ? 0.55f : 0.35f) +
                static_cast<float>(getId() % 7u)) % 9;
            m_activity = phase < 2 ? WildlifeActivity::Rest
                       : phase < 5 ? WildlifeActivity::Forage
                       : WildlifeActivity::Wander;
            const float homeX = m_home.x - position.x;
            const float homeZ = m_home.z - position.z;
            if (homeX * homeX + homeZ * homeZ > 64.f)
                m_headingRadians = std::atan2(homeX, -homeZ);
            else if (phase == 5)
                m_headingRadians += 0.11f +
                    static_cast<float>(getId() % 5u) * 0.06f;
        }
    }

    const bool moving = m_activity == WildlifeActivity::Wander ||
                        m_activity == WildlifeActivity::Flee;
    const float step = moving ? speedFor(getType(),
        m_activity == WildlifeActivity::Flee) * m_motionElapsed : 0.f;
    glm::vec3 candidate = position;
    candidate.x += std::sin(m_headingRadians) * step;
    candidate.z -= std::cos(m_headingRadians) * step;
    const float fallSpeed = std::min(8.f,
        m_fallSpeed + 18.f * m_motionElapsed);
    candidate.y = std::max(1.f, candidate.y -
        std::min(.8f, fallSpeed * m_motionElapsed));
    glm::vec3 settled{0.f};
    bool grounded = false;
    WildlifeMotionPath pathKind = WildlifeMotionPath::None;
    const World::WildlifeStepResult result = world.tryWildlifeStep(
        position, candidate, box.dimensions, settled, &grounded, &pathKind);
    if (result == World::WildlifeStepResult::BudgetDenied) return;
    const float motionSeconds = m_motionElapsed;
    m_motionElapsed = 0.f;
    if (result != World::WildlifeStepResult::Allowed) {
        if (decide) {
            // Try the other local side on the next blocked decision. Each
            // tick still submits exactly one candidate to the shared budget.
            m_blockedHeadingRadians = m_headingRadians;
            m_hasBlockedTurn = true;
            m_headingRadians += m_nextBlockedTurnNegative ? -1.57f : 1.57f;
            m_nextBlockedTurnNegative = !m_nextBlockedTurnNegative;
        }
        return;
    }
    m_fallSpeed = grounded ? 0.f : fallSpeed;
    if (settled != position) {
        // A stationary support check cannot erase an unsuccessful side.
        // Budget denial returned above without changing this turn memory.
        m_hasBlockedTurn = false;
        m_nextBlockedTurnNegative = false;
        // Overflow starts a new stream; renderers detect the sequence gap and
        // explicitly rebase. Accepted zero-distance checks are not motion.
        if (m_motionHistory.newestSequence ==
            std::numeric_limits<std::uint64_t>::max())
            m_motionHistory = {};
        if (m_motionHistory.count == WildlifeMotionHistory::MaximumSegments) {
            for (std::size_t i = 1; i < m_motionHistory.count; ++i)
                m_motionHistory.segments[i - 1] = m_motionHistory.segments[i];
            --m_motionHistory.count;
        }
        m_motionHistory.segments[m_motionHistory.count++] = {
            ++m_motionHistory.newestSequence, position, settled,
            motionSeconds, pathKind};
    }
    position = settled;
    box.update(position);
    rotation.y = glm::degrees(m_headingRadians);
}

ActorSnapshot WildlifeActor::getSnapshot() const
{
    ActorSnapshot snapshot = LivingActor::getSnapshot();
    snapshot.wildlifeActivity = static_cast<int>(m_activity);
    snapshot.wildlifeMotionSeconds = m_ageSeconds;
    snapshot.wildlifeMotionHistory = m_motionHistory;
    return snapshot;
}
