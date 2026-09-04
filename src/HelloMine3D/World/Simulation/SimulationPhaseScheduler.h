#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

enum class SimulationScheduledWorkload : std::uint8_t
{
    ManagedActors = 0,
    RandomTickSections,
    BlockEntities,
    Count
};

constexpr std::size_t SimulationScheduledWorkloadCount =
    static_cast<std::size_t>(SimulationScheduledWorkload::Count);

const char *simulationScheduledWorkloadName(
    SimulationScheduledWorkload workload) noexcept;

struct SimulationWorkPlan
{
    SimulationScheduledWorkload workload =
        SimulationScheduledWorkload::ManagedActors;
    std::size_t eligible = 0;
    std::size_t admitted = 0;
    std::size_t deferred = 0;
    std::size_t budget = 0;
    std::size_t firstIndex = 0;
    std::size_t serviceWindowTicks = 0;
};

/// Concrete admission policy for the three real D1 fixed-tick workloads.
class SimulationPhaseScheduler
{
  public:
    static constexpr std::size_t ManagedActorBudgetPerTick = 64;
    static constexpr std::size_t BlockEntityBudgetPerTick = 32;

    SimulationWorkPlan planManagedActors(std::size_t eligible) noexcept;
    SimulationWorkPlan planRandomTickSections(
        std::size_t eligible, std::size_t existingBudget) noexcept;
    SimulationWorkPlan planBlockEntities(std::size_t eligible) noexcept;

  private:
    SimulationWorkPlan planRoundRobin(
        SimulationScheduledWorkload workload, std::size_t eligible,
        std::size_t budget) noexcept;
    static SimulationWorkPlan planExistingQueue(
        SimulationScheduledWorkload workload, std::size_t eligible,
        std::size_t budget) noexcept;

    std::array<std::size_t, SimulationScheduledWorkloadCount>
        m_nextIndices{{0, 0, 0}};
};
