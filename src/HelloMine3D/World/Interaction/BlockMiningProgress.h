#pragma once

#include "../../Item/Material.h"
#include "../../Maths/glm.h"
#include "../Block/BlockId.h"

struct MiningProgressSnapshot
{
    bool active = false;
    glm::ivec3 target{0};
    BlockId blockId = BlockId::Air;
    Material::ID toolMaterialId = Material::ID::Nothing;
    float elapsedSeconds = 0.0f;
    float requiredSeconds = 0.0f;

    float normalized() const noexcept;
    int crackStage() const noexcept;
};

/// Tracks one continuous hold-to-mine action.
class BlockMiningProgress
{
  public:
    static constexpr float MaxFrameContributionSeconds = 0.25f;

    bool advance(const glm::ivec3 &target, BlockId blockId,
                 Material::ID toolMaterialId, float requiredSeconds,
                 float deltaSeconds);
    void cancel() noexcept;
    const MiningProgressSnapshot &snapshot() const noexcept;

  private:
    MiningProgressSnapshot m_snapshot;
};
