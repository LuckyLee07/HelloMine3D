#pragma once

// Transient, per-world health; the map file remains the persistent truth.
// Reset warnings last for this session even after new observations are saved.
struct ExplorationMapStatus {
    enum class ResetReason { None, Corrupt, ForeignIdentity };
    ResetReason resetReason = ResetReason::None;
    bool quarantineFailed = false;
    bool saveFailed = false;
    bool full = false;

    bool needsAttention() const noexcept
    {
        return resetReason != ResetReason::None || quarantineFailed ||
               saveFailed || full;
    }
    const char* recoveryMessageKey() const noexcept
    {
        if (quarantineFailed) return "map.status_isolation_failed";
        if (saveFailed) return "map.status_save_failed";
        if (resetReason == ResetReason::Corrupt) return "map.status_reset";
        if (resetReason == ResetReason::ForeignIdentity) return "map.status_foreign";
        return nullptr;
    }
};
