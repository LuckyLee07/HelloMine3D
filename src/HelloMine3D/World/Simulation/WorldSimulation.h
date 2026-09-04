#ifndef WORLDSIMULATION_H_INCLUDED
#define WORLDSIMULATION_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../Util/NonCopyable.h"
#include "SimulationPhaseScheduler.h"

class World;

enum class WorldSimulationPhase : std::uint8_t {
    TickPreparation = 0,
    ActorSimulation,
    Combat,
    Encounter,
    BlockRandomTick,
    Population,
    BlockEntitySimulation,
    GameplayRuntime,
    Count
};

constexpr std::size_t WorldSimulationPhaseCount =
    static_cast<std::size_t>(WorldSimulationPhase::Count);
constexpr std::size_t SimulationMetricPhaseCount = 5;

const char *worldSimulationPhaseName(WorldSimulationPhase phase) noexcept;

enum class SimulationPhaseBudgetScope : std::uint8_t {
    Unbudgeted = 0,
    PerTick,
    PerPopulationCycle,
};

enum class SimulationPhaseBudgetStatus : std::uint8_t {
    Unbudgeted = 0,
    WithinBudget,
    AtBudget,
    WorkDeferred,
};

const char *simulationPhaseBudgetScopeName(
    SimulationPhaseBudgetScope scope) noexcept;
const char *simulationPhaseBudgetStatusName(
    SimulationPhaseBudgetStatus status) noexcept;

struct WorldTickContext {
    int tick = 0;
    float deltaSeconds = 1.f / 20.f;
};

struct WorldSimulationPhaseTiming {
    WorldSimulationPhase phase = WorldSimulationPhase::TickPreparation;
    double elapsedMilliseconds = 0.0;
};

struct SimulationPhaseMetrics {
    WorldSimulationPhase phase = WorldSimulationPhase::ActorSimulation;
    double elapsedMilliseconds = 0.0;
    std::size_t processed = 0;
    std::size_t deferred = 0;
    std::size_t budget = 0;
    std::size_t eligible = 0;
    std::size_t serviceWindowTicks = 0;
    bool schedulerManaged = false;
    SimulationPhaseBudgetScope budgetScope =
        SimulationPhaseBudgetScope::Unbudgeted;

    SimulationPhaseBudgetStatus budgetStatus() const noexcept;
};

struct WorldSimulationSnapshot {
    std::uint64_t completedTicks = 0;
    int lastTick = 0;
    float deltaSeconds = 1.f / 20.f;
    double tickElapsedMilliseconds = 0.0;
    std::array<WorldSimulationPhaseTiming, WorldSimulationPhaseCount> phases;
    std::array<SimulationPhaseMetrics, SimulationMetricPhaseCount> metrics;
    std::array<SimulationWorkPlan, SimulationScheduledWorkloadCount>
        scheduledWorkloads;
};

const SimulationPhaseMetrics *findSimulationPhaseMetrics(
    const WorldSimulationSnapshot &snapshot,
    WorldSimulationPhase phase) noexcept;

/// Coordinates the existing fixed-tick phases without owning gameplay state.
class WorldSimulation final : public NonCopyable {
  public:
    static constexpr float FixedDeltaSeconds = 1.f / 20.f;

    explicit WorldSimulation(World &world);

    void fixedTick(const WorldTickContext &context);
    const WorldSimulationSnapshot &snapshot() const noexcept;

  private:
    World &m_world;
    SimulationPhaseScheduler m_scheduler;
    WorldSimulationSnapshot m_snapshot;
};

#endif // WORLDSIMULATION_H_INCLUDED
