#ifndef CHUNKLIFECYCLE_H_INCLUDED
#define CHUNKLIFECYCLE_H_INCLUDED

#include <cstddef>

enum class ChunkDataResidencyState {
    Absent,
    Requested,
    Loading,
    Generating,
    Resident,
    EvictRequested,
    Saving,
};

enum class ChunkMeshState {
    Clean,
    Dirty,
    Queued,
    Building,
    CpuReady,
};

enum class ChunkRenderState {
    NotResident,
    UploadPending,
    GpuResident,
    Stale,
};

constexpr std::size_t ChunkDataResidencyStateCount = 7;
constexpr std::size_t ChunkMeshStateCount = 5;
constexpr std::size_t ChunkRenderStateCount = 4;

bool canTransition(ChunkDataResidencyState from,
                   ChunkDataResidencyState to) noexcept;
bool canTransition(ChunkMeshState from, ChunkMeshState to) noexcept;
bool canTransition(ChunkRenderState from, ChunkRenderState to) noexcept;

const char *chunkDataResidencyStateName(
    ChunkDataResidencyState state) noexcept;
const char *chunkMeshStateName(ChunkMeshState state) noexcept;
const char *chunkRenderStateName(ChunkRenderState state) noexcept;

#endif // CHUNKLIFECYCLE_H_INCLUDED
