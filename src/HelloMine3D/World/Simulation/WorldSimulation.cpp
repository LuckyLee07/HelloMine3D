#include "WorldSimulation.h"

#include <algorithm>
#include <chrono>

#include "../../Diagnostics/RuntimeProfiler.h"
#include "../../Item/SmeltingRegistry.h"
#include "../Block/FurnaceContainer.h"
#include "../World.h"

namespace
{
    using SimulationClock = std::chrono::steady_clock;

    std::size_t phaseIndex(WorldSimulationPhase phase) noexcept
    {
        return static_cast<std::size_t>(phase);
    }

    void initializePhaseIdentity(WorldSimulationSnapshot &snapshot) noexcept
    {
        for (std::size_t index = 0; index < snapshot.phases.size(); ++index) {
            snapshot.phases[index].phase =
                static_cast<WorldSimulationPhase>(index);
        }
    }

    void initializeMetricIdentity(WorldSimulationSnapshot &snapshot) noexcept
    {
        snapshot.metrics[0].phase = WorldSimulationPhase::ActorSimulation;
        snapshot.metrics[1].phase = WorldSimulationPhase::Combat;
        snapshot.metrics[1].budget =
            World::CombatProjectileStepBudgetPerTick;
        snapshot.metrics[1].budgetScope =
            SimulationPhaseBudgetScope::PerTick;
        snapshot.metrics[2].phase = WorldSimulationPhase::BlockRandomTick;
        snapshot.metrics[2].budget =
            World::RandomTickSectionBudgetPerTick;
        snapshot.metrics[2].budgetScope =
            SimulationPhaseBudgetScope::PerTick;
        snapshot.metrics[3].phase = WorldSimulationPhase::Population;
        snapshot.metrics[3].budgetScope =
            SimulationPhaseBudgetScope::PerPopulationCycle;
    }

    SimulationPhaseMetrics *findMutablePhaseMetrics(
        WorldSimulationSnapshot &snapshot,
        WorldSimulationPhase phase) noexcept
    {
        for (SimulationPhaseMetrics &metrics : snapshot.metrics) {
            if (metrics.phase == phase) {
                return &metrics;
            }
        }
        return nullptr;
    }

    class RawPhaseTimer final {
      public:
        RawPhaseTimer(WorldSimulationSnapshot &snapshot,
                      WorldSimulationPhase phase) noexcept
            : m_timing(snapshot.phases[phaseIndex(phase)])
            , m_metrics(findMutablePhaseMetrics(snapshot, phase))
            , m_start(SimulationClock::now())
        {
        }

        ~RawPhaseTimer()
        {
            const double elapsedMilliseconds =
                std::chrono::duration<double, std::milli>(
                    SimulationClock::now() - m_start)
                    .count();
            m_timing.elapsedMilliseconds = elapsedMilliseconds;
            if (m_metrics != nullptr) {
                m_metrics->elapsedMilliseconds = elapsedMilliseconds;
            }
        }

      private:
        WorldSimulationPhaseTiming &m_timing;
        SimulationPhaseMetrics *m_metrics;
        SimulationClock::time_point m_start;
    };
}

const char *simulationPhaseBudgetScopeName(
    SimulationPhaseBudgetScope scope) noexcept
{
    switch (scope) {
        case SimulationPhaseBudgetScope::Unbudgeted:
            return "unbudgeted";
        case SimulationPhaseBudgetScope::PerTick:
            return "per-tick";
        case SimulationPhaseBudgetScope::PerPopulationCycle:
            return "per-cycle";
    }
    return "unknown";
}

const char *simulationPhaseBudgetStatusName(
    SimulationPhaseBudgetStatus status) noexcept
{
    switch (status) {
        case SimulationPhaseBudgetStatus::Unbudgeted:
            return "unbudgeted";
        case SimulationPhaseBudgetStatus::WithinBudget:
            return "within";
        case SimulationPhaseBudgetStatus::AtBudget:
            return "at-budget";
        case SimulationPhaseBudgetStatus::WorkDeferred:
            return "deferred";
    }
    return "unknown";
}

SimulationPhaseBudgetStatus
SimulationPhaseMetrics::budgetStatus() const noexcept
{
    if (budgetScope == SimulationPhaseBudgetScope::Unbudgeted) {
        return SimulationPhaseBudgetStatus::Unbudgeted;
    }
    if (deferred > 0) {
        return SimulationPhaseBudgetStatus::WorkDeferred;
    }
    if (budget > 0 && processed >= budget) {
        return SimulationPhaseBudgetStatus::AtBudget;
    }
    return SimulationPhaseBudgetStatus::WithinBudget;
}

const SimulationPhaseMetrics *findSimulationPhaseMetrics(
    const WorldSimulationSnapshot &snapshot,
    WorldSimulationPhase phase) noexcept
{
    for (const SimulationPhaseMetrics &metrics : snapshot.metrics) {
        if (metrics.phase == phase) {
            return &metrics;
        }
    }
    return nullptr;
}

const char *worldSimulationPhaseName(WorldSimulationPhase phase) noexcept
{
    switch (phase) {
        case WorldSimulationPhase::TickPreparation:
            return "Tick Preparation";
        case WorldSimulationPhase::ActorSimulation:
            return "Actor Simulation";
        case WorldSimulationPhase::Combat:
            return "Combat";
        case WorldSimulationPhase::Encounter:
            return "Encounter";
        case WorldSimulationPhase::BlockRandomTick:
            return "Block Random Tick";
        case WorldSimulationPhase::Population:
            return "Population";
        case WorldSimulationPhase::BlockEntitySimulation:
            return "Block Entity Simulation";
        case WorldSimulationPhase::GameplayRuntime:
            return "Gameplay Runtime";
        case WorldSimulationPhase::Count:
            break;
    }
    return "Unknown";
}

WorldSimulation::WorldSimulation(World &world)
    : m_world(world)
{
    initializePhaseIdentity(m_snapshot);
    initializeMetricIdentity(m_snapshot);
}

void WorldSimulation::fixedTick(const WorldTickContext &context)
{
    HELLOMINE3D_PROFILE_SCOPE("WorldSimulation::fixedTick");

    WorldSimulationSnapshot next;
    initializePhaseIdentity(next);
    initializeMetricIdentity(next);
    next.completedTicks = m_snapshot.completedTicks + 1;
    next.lastTick = context.tick;
    next.deltaSeconds = context.deltaSeconds;
    const SimulationClock::time_point tickStart = SimulationClock::now();

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::TickPreparation");
        RawPhaseTimer timing(next, WorldSimulationPhase::TickPreparation);
        m_world.applyPendingDifficulty();
        m_world.m_worldSaveData.worldTime =
            static_cast<float>(context.tick);
        m_world.m_combatRaycastsUsed = 0;
        m_world.m_combatRaycastBudgetDenied = 0;
        m_world.m_combatChaseStepsUsed = 0;
        m_world.m_combatChaseStepBudgetDenied = 0;
        m_world.m_combatProjectileStepsUsed = 0;
        m_world.m_combatProjectileStepBudgetDenied = 0;
        if (m_world.m_playerGuardRecoverTicksRemaining > 0) {
            --m_world.m_playerGuardRecoverTicksRemaining;
        }
        if (m_world.m_playerCombatFeedback.ticksRemaining > 0) {
            --m_world.m_playerCombatFeedback.ticksRemaining;
            if (m_world.m_playerCombatFeedback.ticksRemaining == 0) {
                m_world.m_playerCombatFeedback.kind =
                    PlayerCombatFeedbackKind::None;
                m_world.m_playerCombatFeedback.direction =
                    CombatDirection::None;
                m_world.m_playerCombatFeedback.sourceId = InvalidActorId;
            }
        }
        if (m_world.m_player != nullptr) {
            m_world.m_playerActor.syncFromPlayer(*m_world.m_player);
        }
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::ActorSimulation");
        RawPhaseTimer timing(next, WorldSimulationPhase::ActorSimulation);
        m_world.m_playerActor.tick(m_world, context.deltaSeconds);
        m_world.m_foodCooldownTicksRemaining = std::max(
            0, m_world.m_foodCooldownTicksRemaining - 1);
        m_world.m_attackCooldownTicksRemaining = std::max(
            0, m_world.m_attackCooldownTicksRemaining - 1);
        m_world.m_waystoneResonanceCooldownTicks = std::max(
            0, m_world.m_waystoneResonanceCooldownTicks - 1);
        const std::size_t managedActorsProcessed =
            m_world.m_actorManager.tick(m_world, context.deltaSeconds);
        SimulationPhaseMetrics *metrics = findMutablePhaseMetrics(
            next, WorldSimulationPhase::ActorSimulation);
        metrics->processed = managedActorsProcessed + 1;
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::Combat");
        RawPhaseTimer timing(next, WorldSimulationPhase::Combat);
        m_world.tickCombatProjectiles();
        SimulationPhaseMetrics *metrics = findMutablePhaseMetrics(
            next, WorldSimulationPhase::Combat);
        metrics->processed = m_world.m_combatProjectileStepsUsed;
        metrics->deferred =
            m_world.m_combatProjectileStepBudgetDenied;
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::Encounter");
        RawPhaseTimer timing(next, WorldSimulationPhase::Encounter);
        m_world.reconcileWaystoneEncounter();
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::BlockRandomTick");
        RawPhaseTimer timing(next, WorldSimulationPhase::BlockRandomTick);
        m_world.runRandomTicks(context.tick);
        SimulationPhaseMetrics *metrics = findMutablePhaseMetrics(
            next, WorldSimulationPhase::BlockRandomTick);
        metrics->processed = m_world.m_randomTickSectionsProcessed;
        {
            std::unique_lock<std::mutex> lock(m_world.m_mainMutex);
            metrics->deferred = m_world.m_randomTickSections.size() >
                                        metrics->processed
                                    ? m_world.m_randomTickSections.size() -
                                          metrics->processed
                                    : 0;
        }
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::Population");
        RawPhaseTimer timing(next, WorldSimulationPhase::Population);
        const std::size_t attemptsBefore =
            m_world.m_naturalMobSpawnAttempts;
        SimulationPhaseMetrics *metrics = findMutablePhaseMetrics(
            next, WorldSimulationPhase::Population);
        metrics->budget = m_world.getDifficultySnapshot()
                              .parameters.naturalSpawnAttemptsPerCycle;
        m_world.runNaturalMobPopulation(context.tick);
        metrics->processed = m_world.m_naturalMobSpawnAttempts -
                             attemptsBefore;
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::BlockEntitySimulation");
        RawPhaseTimer timing(next,
                             WorldSimulationPhase::BlockEntitySimulation);
        if (runtimeSmeltingRegistry().isFrozen()) {
            FurnaceContainer::tickLoaded(m_world,
                                         runtimeSmeltingRegistry());
        }
    }

    {
        HELLOMINE3D_PROFILE_SCOPE("Simulation::GameplayRuntime");
        RawPhaseTimer timing(next, WorldSimulationPhase::GameplayRuntime);
        if (m_world.m_alphaJourney != nullptr) {
            m_world.m_alphaJourney->update(context.deltaSeconds);
        }
        if (m_world.m_playerRespawnPending) {
            m_world.respawnPlayer();
        }
    }

    next.tickElapsedMilliseconds =
        std::chrono::duration<double, std::milli>(
            SimulationClock::now() - tickStart)
            .count();
    m_snapshot = next;
}

const WorldSimulationSnapshot &WorldSimulation::snapshot() const noexcept
{
    return m_snapshot;
}
