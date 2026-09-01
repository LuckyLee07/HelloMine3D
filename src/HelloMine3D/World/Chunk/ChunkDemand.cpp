#include "ChunkDemand.h"

#include <algorithm>

void ChunkDemandModel::advanceEpoch() noexcept
{
    ++m_epoch;
    for (auto &slot : m_slots) {
        if (!slot.has_value() || m_epoch <= slot->expiresAfterEpoch) {
            continue;
        }
        slot.reset();
        ++m_expiredDemands;
        ++m_revision;
    }
}

void ChunkDemandModel::refresh(ChunkDemandReason reason,
                               const VectorXZ &coord,
                               int radius) noexcept
{
    radius = std::max(0, radius);
    auto &slot = m_slots[indexOf(reason)];
    const int priority = priorityFor(reason);
    const bool semanticChange =
        !slot.has_value() || !(slot->coord == coord) ||
        slot->priority != priority || slot->radius != radius;

    slot = ChunkDemand{coord, reason, priority, m_epoch,
                       m_epoch + lifetimeFor(reason), radius};
    if (semanticChange) {
        ++m_revision;
    }
}

void ChunkDemandModel::updateRadius(ChunkDemandReason reason,
                                    int radius) noexcept
{
    auto &slot = m_slots[indexOf(reason)];
    if (!slot.has_value()) {
        return;
    }
    radius = std::max(0, radius);
    if (slot->radius == radius) {
        return;
    }
    slot->radius = radius;
    ++m_revision;
}

ChunkDemandSnapshot ChunkDemandModel::snapshot() const
{
    ChunkDemandSnapshot result;
    result.epoch = m_epoch;
    result.revision = m_revision;
    result.demands.reserve(ChunkDemandReasonCount);
    for (const auto &slot : m_slots) {
        if (slot.has_value()) {
            result.demands.push_back(*slot);
        }
    }
    return result;
}

ChunkDemandDebugStats ChunkDemandModel::debugStats() const noexcept
{
    ChunkDemandDebugStats stats;
    stats.epoch = m_epoch;
    stats.revision = m_revision;
    stats.expiredDemands = m_expiredDemands;
    for (const auto &slot : m_slots) {
        if (!slot.has_value()) {
            continue;
        }
        ++stats.activeDemands;
        switch (slot->reason) {
        case ChunkDemandReason::Player:
            ++stats.playerDemands;
            break;
        case ChunkDemandReason::Camera:
            ++stats.cameraDemands;
            break;
        case ChunkDemandReason::TeleportDestination:
            ++stats.teleportDemands;
            break;
        case ChunkDemandReason::Preload:
            ++stats.preloadDemands;
            break;
        }
    }
    return stats;
}

int ChunkDemandModel::priorityFor(ChunkDemandReason reason) noexcept
{
    switch (reason) {
    case ChunkDemandReason::Player:
        return PlayerPriority;
    case ChunkDemandReason::Camera:
        return CameraPriority;
    case ChunkDemandReason::TeleportDestination:
        return TeleportPriority;
    case ChunkDemandReason::Preload:
        return PreloadPriority;
    }
    return 0;
}

std::uint64_t
ChunkDemandModel::lifetimeFor(ChunkDemandReason reason) noexcept
{
    switch (reason) {
    case ChunkDemandReason::Player:
        return PlayerLifetimeEpochs;
    case ChunkDemandReason::Camera:
        return CameraLifetimeEpochs;
    case ChunkDemandReason::TeleportDestination:
        return TeleportLifetimeEpochs;
    case ChunkDemandReason::Preload:
        return PreloadLifetimeEpochs;
    }
    return 0;
}

const char *ChunkDemandModel::reasonName(
    ChunkDemandReason reason) noexcept
{
    switch (reason) {
    case ChunkDemandReason::Player:
        return "Player";
    case ChunkDemandReason::Camera:
        return "Camera";
    case ChunkDemandReason::TeleportDestination:
        return "TeleportDestination";
    case ChunkDemandReason::Preload:
        return "Preload";
    }
    return "Unknown";
}

std::uint32_t ChunkDemandModel::reasonBit(
    ChunkDemandReason reason) noexcept
{
    return 1u << static_cast<unsigned int>(reason);
}

std::size_t ChunkDemandModel::indexOf(
    ChunkDemandReason reason) noexcept
{
    return static_cast<std::size_t>(reason);
}
