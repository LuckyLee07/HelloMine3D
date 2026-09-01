#include "ChunkLifecycle.h"

bool canTransition(ChunkDataResidencyState from,
                   ChunkDataResidencyState to) noexcept
{
    switch (from) {
    case ChunkDataResidencyState::Absent:
        return to == ChunkDataResidencyState::Requested;
    case ChunkDataResidencyState::Requested:
        return to == ChunkDataResidencyState::Loading;
    case ChunkDataResidencyState::Loading:
        return to == ChunkDataResidencyState::Generating ||
               to == ChunkDataResidencyState::Resident ||
               to == ChunkDataResidencyState::Absent;
    case ChunkDataResidencyState::Generating:
        return to == ChunkDataResidencyState::Resident;
    case ChunkDataResidencyState::Resident:
        return to == ChunkDataResidencyState::Saving ||
               to == ChunkDataResidencyState::EvictRequested;
    case ChunkDataResidencyState::EvictRequested:
        return to == ChunkDataResidencyState::Saving ||
               to == ChunkDataResidencyState::Absent ||
               to == ChunkDataResidencyState::Resident;
    case ChunkDataResidencyState::Saving:
        return to == ChunkDataResidencyState::Resident ||
               to == ChunkDataResidencyState::EvictRequested;
    }
    return false;
}

bool canTransition(ChunkMeshState from, ChunkMeshState to) noexcept
{
    if (to == ChunkMeshState::Dirty && from != ChunkMeshState::Dirty) {
        return true;
    }

    switch (from) {
    case ChunkMeshState::Clean:
        return false;
    case ChunkMeshState::Dirty:
        return to == ChunkMeshState::Queued;
    case ChunkMeshState::Queued:
        return to == ChunkMeshState::Building;
    case ChunkMeshState::Building:
        return to == ChunkMeshState::CpuReady;
    case ChunkMeshState::CpuReady:
        return to == ChunkMeshState::Clean;
    }
    return false;
}

bool canTransition(ChunkRenderState from, ChunkRenderState to) noexcept
{
    switch (from) {
    case ChunkRenderState::NotResident:
        return to == ChunkRenderState::UploadPending;
    case ChunkRenderState::UploadPending:
        return to == ChunkRenderState::GpuResident ||
               to == ChunkRenderState::Stale ||
               to == ChunkRenderState::NotResident;
    case ChunkRenderState::GpuResident:
        return to == ChunkRenderState::Stale ||
               to == ChunkRenderState::NotResident;
    case ChunkRenderState::Stale:
        return to == ChunkRenderState::UploadPending ||
               to == ChunkRenderState::NotResident;
    }
    return false;
}

const char *chunkDataResidencyStateName(
    ChunkDataResidencyState state) noexcept
{
    switch (state) {
    case ChunkDataResidencyState::Absent:
        return "Absent";
    case ChunkDataResidencyState::Requested:
        return "Requested";
    case ChunkDataResidencyState::Loading:
        return "Loading";
    case ChunkDataResidencyState::Generating:
        return "Generating";
    case ChunkDataResidencyState::Resident:
        return "Resident";
    case ChunkDataResidencyState::EvictRequested:
        return "EvictRequested";
    case ChunkDataResidencyState::Saving:
        return "Saving";
    }
    return "Unknown";
}

const char *chunkMeshStateName(ChunkMeshState state) noexcept
{
    switch (state) {
    case ChunkMeshState::Clean:
        return "Clean";
    case ChunkMeshState::Dirty:
        return "Dirty";
    case ChunkMeshState::Queued:
        return "Queued";
    case ChunkMeshState::Building:
        return "Building";
    case ChunkMeshState::CpuReady:
        return "CpuReady";
    }
    return "Unknown";
}

const char *chunkRenderStateName(ChunkRenderState state) noexcept
{
    switch (state) {
    case ChunkRenderState::NotResident:
        return "NotResident";
    case ChunkRenderState::UploadPending:
        return "UploadPending";
    case ChunkRenderState::GpuResident:
        return "GpuResident";
    case ChunkRenderState::Stale:
        return "Stale";
    }
    return "Unknown";
}
