#ifndef WORLDSIMULATION_H_INCLUDED
#define WORLDSIMULATION_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../Util/NonCopyable.h"

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

const char *worldSimulationPhaseName(WorldSimulationPhase phase) noexcept;

struct WorldTickContext {
    int tick = 0;
    float deltaSeconds = 1.f / 20.f;
};

struct WorldSimulationPhaseTiming {
    WorldSimulationPhase phase = WorldSimulationPhase::TickPreparation;
    double elapsedMilliseconds = 0.0;
};

struct WorldSimulationSnapshot {
    std::uint64_t completedTicks = 0;
    int lastTick = 0;
    float deltaSeconds = 1.f / 20.f;
    double tickElapsedMilliseconds = 0.0;
    std::array<WorldSimulationPhaseTiming, WorldSimulationPhaseCount> phases;
};

/// Coordinates the existing fixed-tick phases without owning gameplay state.
class WorldSimulation final : public NonCopyable {
  public:
    static constexpr float FixedDeltaSeconds = 1.f / 20.f;

    explicit WorldSimulation(World &world);

    void fixedTick(const WorldTickContext &context);
    const WorldSimulationSnapshot &snapshot() const noexcept;

  private:
    World &m_world;
    WorldSimulationSnapshot m_snapshot;
};

#endif // WORLDSIMULATION_H_INCLUDED
