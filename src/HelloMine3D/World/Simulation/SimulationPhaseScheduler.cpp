#include "SimulationPhaseScheduler.h"

#include <algorithm>

namespace
{
std::size_t workloadIndex(SimulationScheduledWorkload workload) noexcept
{
    return static_cast<std::size_t>(workload);
}

std::size_t serviceWindow(std::size_t eligible,
                          std::size_t budget) noexcept
{
    if (eligible == 0 || budget == 0) {
        return 0;
    }
    return 1 + (eligible - 1) / budget;
}
} // namespace

const char *simulationScheduledWorkloadName(
    SimulationScheduledWorkload workload) noexcept
{
    switch (workload) {
        case SimulationScheduledWorkload::ManagedActors:
            return "Managed Actors";
        case SimulationScheduledWorkload::RandomTickSections:
            return "Random-Tick Sections";
        case SimulationScheduledWorkload::BlockEntities:
            return "Block Entities";
        case SimulationScheduledWorkload::Count:
            break;
    }
    return "Unknown";
}

SimulationWorkPlan SimulationPhaseScheduler::planManagedActors(
    std::size_t eligible) noexcept
{
    return planRoundRobin(SimulationScheduledWorkload::ManagedActors,
                          eligible, ManagedActorBudgetPerTick);
}

SimulationWorkPlan SimulationPhaseScheduler::planRandomTickSections(
    std::size_t eligible, std::size_t existingBudget) noexcept
{
    // World owns this workload's established FIFO rotation. D1 owns only
    // admission and reports the same finite service window.
    return planExistingQueue(
        SimulationScheduledWorkload::RandomTickSections, eligible,
        existingBudget);
}

SimulationWorkPlan SimulationPhaseScheduler::planBlockEntities(
    std::size_t eligible) noexcept
{
    return planRoundRobin(SimulationScheduledWorkload::BlockEntities,
                          eligible, BlockEntityBudgetPerTick);
}

SimulationWorkPlan SimulationPhaseScheduler::planRoundRobin(
    SimulationScheduledWorkload workload, std::size_t eligible,
    std::size_t budget) noexcept
{
    SimulationWorkPlan plan;
    plan.workload = workload;
    plan.eligible = eligible;
    plan.budget = budget;
    plan.admitted = std::min(eligible, budget);
    plan.deferred = eligible - plan.admitted;
    plan.serviceWindowTicks = serviceWindow(eligible, budget);

    std::size_t &next = m_nextIndices[workloadIndex(workload)];
    if (eligible == 0) {
        next = 0;
        return plan;
    }

    next %= eligible;
    plan.firstIndex = next;
    next = (next + plan.admitted) % eligible;
    return plan;
}

SimulationWorkPlan SimulationPhaseScheduler::planExistingQueue(
    SimulationScheduledWorkload workload, std::size_t eligible,
    std::size_t budget) noexcept
{
    SimulationWorkPlan plan;
    plan.workload = workload;
    plan.eligible = eligible;
    plan.budget = budget;
    plan.admitted = std::min(eligible, budget);
    plan.deferred = eligible - plan.admitted;
    plan.serviceWindowTicks = serviceWindow(eligible, budget);
    return plan;
}
