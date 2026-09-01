#ifndef CHUNKDEMAND_H_INCLUDED
#define CHUNKDEMAND_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "../../Maths/Vector2XZ.h"

enum class ChunkDemandReason : std::uint8_t {
    Player,
    Camera,
    TeleportDestination,
    Preload,
};

constexpr std::size_t ChunkDemandReasonCount = 4;

struct ChunkDemand {
    VectorXZ coord{0, 0};
    ChunkDemandReason reason = ChunkDemandReason::Player;
    int priority = 0;
    std::uint64_t epoch = 0;
    std::uint64_t expiresAfterEpoch = 0;
    int radius = 0;
};

struct ChunkDemandSnapshot {
    std::uint64_t epoch = 0;
    std::uint64_t revision = 0;
    std::vector<ChunkDemand> demands;
};

struct ChunkDemandDebugStats {
    std::uint64_t epoch = 0;
    std::uint64_t revision = 0;
    std::size_t activeDemands = 0;
    std::size_t playerDemands = 0;
    std::size_t cameraDemands = 0;
    std::size_t teleportDemands = 0;
    std::size_t preloadDemands = 0;
    std::size_t expiredDemands = 0;
    std::size_t lastPlannedTargets = 0;
};

class ChunkDemandModel {
  public:
    static constexpr int PlayerPriority = 300;
    static constexpr int CameraPriority = 200;
    static constexpr int TeleportPriority = 400;
    static constexpr int PreloadPriority = 100;

    static constexpr std::uint64_t PlayerLifetimeEpochs = 2;
    static constexpr std::uint64_t CameraLifetimeEpochs = 2;
    static constexpr std::uint64_t TeleportLifetimeEpochs = 120;
    static constexpr std::uint64_t PreloadLifetimeEpochs = 30;

    void advanceEpoch() noexcept;
    void refresh(ChunkDemandReason reason, const VectorXZ &coord,
                 int radius) noexcept;
    void updateRadius(ChunkDemandReason reason, int radius) noexcept;

    ChunkDemandSnapshot snapshot() const;
    ChunkDemandDebugStats debugStats() const noexcept;

    static int priorityFor(ChunkDemandReason reason) noexcept;
    static std::uint64_t lifetimeFor(ChunkDemandReason reason) noexcept;
    static const char *reasonName(ChunkDemandReason reason) noexcept;
    static std::uint32_t reasonBit(ChunkDemandReason reason) noexcept;

  private:
    static std::size_t indexOf(ChunkDemandReason reason) noexcept;

    std::array<std::optional<ChunkDemand>, ChunkDemandReasonCount> m_slots;
    std::uint64_t m_epoch = 0;
    std::uint64_t m_revision = 0;
    std::size_t m_expiredDemands = 0;
};

#endif // CHUNKDEMAND_H_INCLUDED
