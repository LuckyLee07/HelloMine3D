#include "BlockMiningProgress.h"

#include <algorithm>

namespace
{
    bool samePosition(const glm::ivec3 &left,
                      const glm::ivec3 &right) noexcept
    {
        return left.x == right.x && left.y == right.y &&
               left.z == right.z;
    }
}

float MiningProgressSnapshot::normalized() const noexcept
{
    if (!active || requiredSeconds <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(elapsedSeconds / requiredSeconds, 0.0f, 1.0f);
}

int MiningProgressSnapshot::crackStage() const noexcept
{
    if (!active) {
        return -1;
    }
    return std::clamp(static_cast<int>(normalized() * 10.f), 0, 9);
}

bool BlockMiningProgress::advance(
    const glm::ivec3 &target, BlockId blockId,
    Material::ID toolMaterialId, float requiredSeconds,
    float deltaSeconds)
{
    const float boundedRequired = std::max(0.05f, requiredSeconds);
    if (!m_snapshot.active || !samePosition(m_snapshot.target, target) ||
        m_snapshot.blockId != blockId ||
        m_snapshot.toolMaterialId != toolMaterialId) {
        m_snapshot.active = true;
        m_snapshot.target = target;
        m_snapshot.blockId = blockId;
        m_snapshot.toolMaterialId = toolMaterialId;
        m_snapshot.elapsedSeconds = 0.0f;
        m_snapshot.requiredSeconds = boundedRequired;
    }
    else {
        m_snapshot.requiredSeconds = boundedRequired;
    }

    m_snapshot.elapsedSeconds += std::clamp(
        deltaSeconds, 0.0f, MaxFrameContributionSeconds);
    if (m_snapshot.elapsedSeconds + 0.000001f <
        m_snapshot.requiredSeconds) {
        return false;
    }
    cancel();
    return true;
}

void BlockMiningProgress::cancel() noexcept
{
    m_snapshot = {};
}

const MiningProgressSnapshot &BlockMiningProgress::snapshot() const noexcept
{
    return m_snapshot;
}
