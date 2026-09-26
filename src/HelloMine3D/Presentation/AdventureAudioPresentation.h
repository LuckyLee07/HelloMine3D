#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Read-only presentation scheduling for the adventure soundscape. Callers
// provide facts sampled from the active world; this state never writes back to
// gameplay, terrain, actors, or saves.
namespace AdventureAudioPresentation
{
enum class AmbientKind : std::uint8_t
{
    OpenLand,
    Forest,
    InlandWater,
    Coast,
    Count
};

enum class SurfaceKind : std::uint8_t
{
    Unknown,
    GrassDirt,
    Stone,
    Wood,
    Sand
};

enum class AnimalSpecies : std::uint8_t
{
    Unknown,
    Sheep,
    Rabbit,
    WetlandBird
};

enum class AnimalActivity : std::uint8_t
{
    Rest,
    Forage,
    Wander,
    Flee
};

enum class CueKind : std::uint8_t
{
    None,
    AmbientOpenLand,
    AmbientForest,
    AmbientInlandWater,
    AmbientCoast,
    FootstepGrassDirt,
    FootstepStone,
    FootstepWood,
    FootstepSand,
    AnimalSheep,
    AnimalRabbit,
    AnimalWetlandBird
};

struct Vec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct EnvironmentTarget
{
    float weight = 0.f;
    bool hasPosition = false;
    Vec3 position{};
};

struct AnimalSource
{
    std::uint64_t stableId = 0;
    AnimalSpecies species = AnimalSpecies::Unknown;
    AnimalActivity activity = AnimalActivity::Rest;
    Vec3 position{};
};

inline constexpr std::size_t AmbientKindCount =
    static_cast<std::size_t>(AmbientKind::Count);
inline constexpr std::size_t MaxAmbientLayers = 3;
inline constexpr std::size_t MaxAnimalSources = 24;
inline constexpr std::size_t MaxCuesPerFrame = 3;
inline constexpr float EnvironmentTransitionSeconds = 2.f;
inline constexpr float MinimumAudibleLayerWeight = .04f;
inline constexpr float MinimumCueLayerWeight = .16f;
inline constexpr float FootstepStrideMetres = 1.65f;
inline constexpr float MaximumTrustedHorizontalDisplacement = 4.f;
// Leave audible headroom inside the native backends' 40 metre spatial
// falloff. A nearly silent remote source must not consume the global animal
// cooldown while a nearby animal remains clear.
inline constexpr float MaximumAnimalDistance = 32.f;
inline constexpr float AnimalForgetSeconds = 1.f;
inline constexpr float AmbientGlobalCooldownSeconds = 1.25f;
inline constexpr float AnimalGlobalCooldownSeconds = 1.1f;
inline constexpr float MaximumPresentationDeltaSeconds = .25f;

struct Input
{
    float deltaSeconds = 0.f;
    std::uint64_t worldEpoch = 0;
    bool worldActive = false;
    bool paused = false;
    bool uiBlocked = false;
    bool playerPositionValid = false;
    Vec3 playerPosition{};
    bool grounded = false;
    bool flying = false;
    SurfaceKind supportSurface = SurfaceKind::Unknown;
    // 0 is night and 1 is full daylight. Invalid values use a neutral value.
    float daylight = .5f;
    std::array<EnvironmentTarget, AmbientKindCount> environment{};
    std::array<AnimalSource, MaxAnimalSources> animals{};
    std::size_t animalCount = 0;
};

struct AmbientLayer
{
    AmbientKind kind = AmbientKind::OpenLand;
    float weight = 0.f;
    bool hasPosition = false;
    Vec3 position{};
};

struct Cue
{
    CueKind kind = CueKind::None;
    float gain = 0.f;
    bool spatial = false;
    Vec3 position{};
    std::uint64_t stableSourceId = 0;
    std::uint8_t variant = 0;
};

struct Frame
{
    std::array<AmbientLayer, MaxAmbientLayers> ambientLayers{};
    std::size_t ambientLayerCount = 0;
    std::array<Cue, MaxCuesPerFrame> cues{};
    std::size_t cueCount = 0;
    bool resetThisFrame = false;
};

struct AnimalSlot
{
    std::uint64_t stableId = 0;
    AnimalSpecies species = AnimalSpecies::Unknown;
    AnimalActivity activity = AnimalActivity::Rest;
    Vec3 position{};
    float cooldownSeconds = 0.f;
    float unseenSeconds = 0.f;
    bool occupied = false;
    bool seenThisFrame = false;
};

struct State
{
    std::uint64_t worldEpoch = 0;
    bool epochInitialized = false;
    bool playerPositionInitialized = false;
    Vec3 previousPlayerPosition{};
    float accumulatedStepDistance = 0.f;
    SurfaceKind accumulatedStepSurface = SurfaceKind::Unknown;
    std::array<float, AmbientKindCount> ambientWeights{};
    std::array<Vec3, AmbientKindCount> ambientPositions{};
    std::array<bool, AmbientKindCount> ambientHasPosition{};
    std::array<float, AmbientKindCount> ambientCountdownSeconds{
        3.25f, 4.5f, 5.75f, 7.f};
    float ambientGlobalCooldownSeconds = 0.f;
    float animalGlobalCooldownSeconds = 0.f;
    std::array<AnimalSlot, MaxAnimalSources> animalSlots{};
    std::uint32_t ambientSequence = 0;
    std::uint32_t footstepSequence = 0;
};

inline bool finite(float value) noexcept
{
    return std::isfinite(value);
}

inline bool finite(const Vec3 &value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

inline float clamp01(float value) noexcept
{
    return finite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
}

inline float sanitizeDaylight(float value) noexcept
{
    return finite(value) ? std::clamp(value, 0.f, 1.f) : .5f;
}

inline float sanitizeDelta(float value) noexcept
{
    if (!finite(value) || value <= 0.f)
        return 0.f;
    return std::min(value, MaximumPresentationDeltaSeconds);
}

inline float horizontalDistance(const Vec3 &first,
                                const Vec3 &second) noexcept
{
    if (!finite(first) || !finite(second))
        return std::numeric_limits<float>::infinity();
    const double x = static_cast<double>(first.x) - second.x;
    const double z = static_cast<double>(first.z) - second.z;
    const double squared = x * x + z * z;
    const double distance = std::sqrt(squared);
    return distance <= std::numeric_limits<float>::max()
               ? static_cast<float>(distance)
               : std::numeric_limits<float>::infinity();
}

inline float distance(const Vec3 &first, const Vec3 &second) noexcept
{
    if (!finite(first) || !finite(second))
        return std::numeric_limits<float>::infinity();
    const double x = static_cast<double>(first.x) - second.x;
    const double y = static_cast<double>(first.y) - second.y;
    const double z = static_cast<double>(first.z) - second.z;
    const double value = std::sqrt(x * x + y * y + z * z);
    return value <= std::numeric_limits<float>::max()
               ? static_cast<float>(value)
               : std::numeric_limits<float>::infinity();
}

inline std::array<EnvironmentTarget, AmbientKindCount>
sanitizeEnvironmentTargets(
    const std::array<EnvironmentTarget, AmbientKindCount> &raw) noexcept
{
    auto result = raw;
    for (EnvironmentTarget &target : result)
    {
        target.weight = clamp01(target.weight);
        if (!target.hasPosition || !finite(target.position))
        {
            target.hasPosition = false;
            target.position = {};
        }
    }

    const std::size_t open = static_cast<std::size_t>(AmbientKind::OpenLand);
    const std::size_t forest = static_cast<std::size_t>(AmbientKind::Forest);
    const float landTotal = result[open].weight + result[forest].weight;
    if (landTotal > 1.f)
    {
        result[open].weight /= landTotal;
        result[forest].weight /= landTotal;
    }

    const std::size_t inland =
        static_cast<std::size_t>(AmbientKind::InlandWater);
    const std::size_t coast = static_cast<std::size_t>(AmbientKind::Coast);
    if (result[inland].weight >= result[coast].weight)
        result[coast].weight = 0.f;
    else
        result[inland].weight = 0.f;
    return result;
}

inline CueKind ambientCue(AmbientKind kind) noexcept
{
    switch (kind)
    {
    case AmbientKind::OpenLand:
        return CueKind::AmbientOpenLand;
    case AmbientKind::Forest:
        return CueKind::AmbientForest;
    case AmbientKind::InlandWater:
        return CueKind::AmbientInlandWater;
    case AmbientKind::Coast:
        return CueKind::AmbientCoast;
    case AmbientKind::Count:
        break;
    }
    return CueKind::None;
}

inline CueKind footstepCue(SurfaceKind surface) noexcept
{
    switch (surface)
    {
    case SurfaceKind::GrassDirt:
        return CueKind::FootstepGrassDirt;
    case SurfaceKind::Stone:
        return CueKind::FootstepStone;
    case SurfaceKind::Wood:
        return CueKind::FootstepWood;
    case SurfaceKind::Sand:
        return CueKind::FootstepSand;
    case SurfaceKind::Unknown:
        break;
    }
    return CueKind::None;
}

inline CueKind animalCue(AnimalSpecies species) noexcept
{
    switch (species)
    {
    case AnimalSpecies::Sheep:
        return CueKind::AnimalSheep;
    case AnimalSpecies::Rabbit:
        return CueKind::AnimalRabbit;
    case AnimalSpecies::WetlandBird:
        return CueKind::AnimalWetlandBird;
    case AnimalSpecies::Unknown:
        break;
    }
    return CueKind::None;
}

inline const char *cueId(CueKind kind, std::uint8_t variant = 0) noexcept
{
    switch (kind)
    {
    case CueKind::AmbientOpenLand:
        return "ambient.open";
    case CueKind::AmbientForest:
        return "ambient.forest";
    case CueKind::AmbientInlandWater:
        return "ambient.river";
    case CueKind::AmbientCoast:
        return "ambient.coast";
    case CueKind::FootstepGrassDirt:
        return variant % 2U == 0U ? "footstep.grass-dirt.1"
                                  : "footstep.grass-dirt.2";
    case CueKind::FootstepStone:
        return variant % 2U == 0U ? "footstep.stone.1"
                                  : "footstep.stone.2";
    case CueKind::FootstepWood:
        return variant % 2U == 0U ? "footstep.wood.1"
                                  : "footstep.wood.2";
    case CueKind::FootstepSand:
        return variant % 2U == 0U ? "footstep.sand.1"
                                  : "footstep.sand.2";
    case CueKind::AnimalSheep:
        return "animal.sheep";
    case CueKind::AnimalRabbit:
        return "animal.rabbit";
    case CueKind::AnimalWetlandBird:
        return "animal.marsh-bird";
    case CueKind::None:
        break;
    }
    return "";
}

inline const char *cueId(const Cue &cue) noexcept
{
    return cueId(cue.kind, cue.variant);
}

inline float ambientInterval(AmbientKind kind, float daylight) noexcept
{
    daylight = sanitizeDaylight(daylight);
    float daySeconds = 10.f;
    float nightSeconds = 15.f;
    switch (kind)
    {
    case AmbientKind::OpenLand:
        daySeconds = 10.f;
        nightSeconds = 15.f;
        break;
    case AmbientKind::Forest:
        daySeconds = 7.f;
        nightSeconds = 13.f;
        break;
    case AmbientKind::InlandWater:
        daySeconds = 8.f;
        nightSeconds = 11.f;
        break;
    case AmbientKind::Coast:
        daySeconds = 9.f;
        nightSeconds = 12.f;
        break;
    case AmbientKind::Count:
        break;
    }
    return nightSeconds + (daySeconds - nightSeconds) * daylight;
}

// Short environment samples are replayed with bounded overlap rather than
// owned as infinite backend loops. Daylight changes their density and level
// without changing which real region facts selected the layers.
inline float ambientReplayInterval(AmbientKind kind,
                                   float daylight) noexcept
{
    const float baseSeconds = kind == AmbientKind::OpenLand ? 1.45f : 2.25f;
    const float densityScale =
        1.08f - .16f * sanitizeDaylight(daylight);
    return baseSeconds * densityScale;
}

inline float ambientLayerGain(float weight, float daylight) noexcept
{
    const float daylightScale =
        .78f + .22f * sanitizeDaylight(daylight);
    return clamp01(weight) * daylightScale;
}

inline float animalCooldown(AnimalSpecies species, AnimalActivity activity,
                            float daylight) noexcept
{
    float seconds = 12.f;
    switch (activity)
    {
    case AnimalActivity::Flee:
        seconds = 4.f;
        break;
    case AnimalActivity::Forage:
        seconds = 7.f;
        break;
    case AnimalActivity::Wander:
        seconds = 9.f;
        break;
    case AnimalActivity::Rest:
        seconds = 12.f;
        break;
    }
    if (species == AnimalSpecies::Rabbit)
        seconds *= 1.2f;
    else if (species == AnimalSpecies::WetlandBird)
        seconds *= .8f;
    const float nightScale = 1.35f - .35f * sanitizeDaylight(daylight);
    return std::clamp(seconds * nightScale, 3.f, 18.f);
}

inline float animalPriority(AnimalActivity activity) noexcept
{
    switch (activity)
    {
    case AnimalActivity::Flee:
        return 4.f;
    case AnimalActivity::Forage:
        return 3.f;
    case AnimalActivity::Wander:
        return 2.f;
    case AnimalActivity::Rest:
        return 1.f;
    }
    return 0.f;
}

inline bool appendCue(Frame &frame, const Cue &cue) noexcept
{
    if (cue.kind == CueKind::None || frame.cueCount >= frame.cues.size())
        return false;
    frame.cues[frame.cueCount++] = cue;
    return true;
}

inline void resetStepTracking(State &state, const Vec3 &position,
                              bool positionValid) noexcept
{
    state.playerPositionInitialized = positionValid && finite(position);
    state.previousPlayerPosition =
        state.playerPositionInitialized ? position : Vec3{};
    state.accumulatedStepDistance = 0.f;
    state.accumulatedStepSurface = SurfaceKind::Unknown;
}

inline void initializeEpoch(
    State &state, const Input &input,
    const std::array<EnvironmentTarget, AmbientKindCount> &targets) noexcept
{
    state = {};
    state.worldEpoch = input.worldEpoch;
    state.epochInitialized = true;
    resetStepTracking(state, input.playerPosition,
                      input.playerPositionValid);
    for (std::size_t index = 0; index < targets.size(); ++index)
    {
        state.ambientWeights[index] = targets[index].weight;
        state.ambientHasPosition[index] = targets[index].hasPosition;
        state.ambientPositions[index] = targets[index].position;
    }
}

inline AnimalSlot *findAnimalSlot(State &state,
                                  std::uint64_t stableId) noexcept
{
    for (AnimalSlot &slot : state.animalSlots)
        if (slot.occupied && slot.stableId == stableId)
            return &slot;
    return nullptr;
}

inline AnimalSlot *allocateAnimalSlot(State &state) noexcept
{
    for (AnimalSlot &slot : state.animalSlots)
        if (!slot.occupied)
            return &slot;
    return nullptr;
}

inline float initialAnimalCooldown(std::uint64_t stableId,
                                   AnimalActivity activity) noexcept
{
    if (activity == AnimalActivity::Flee)
        return 0.f;
    const float phase = static_cast<float>((stableId * 2654435761ULL) % 250U) /
                        100.f;
    return 1.5f + phase;
}

inline void synchronizeAnimals(State &state, const Input &input, float delta,
                               bool advanceTimers) noexcept
{
    for (AnimalSlot &slot : state.animalSlots)
    {
        slot.seenThisFrame = false;
        if (slot.occupied && advanceTimers)
            slot.cooldownSeconds =
                std::max(0.f, slot.cooldownSeconds - delta);
    }

    const std::size_t count =
        std::min(input.animalCount, input.animals.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const AnimalSource &source = input.animals[index];
        if (source.stableId == 0 ||
            source.species == AnimalSpecies::Unknown ||
            !finite(source.position))
            continue;

        AnimalSlot *slot = findAnimalSlot(state, source.stableId);
        if (!slot)
        {
            slot = allocateAnimalSlot(state);
            if (!slot)
                continue;
            *slot = {};
            slot->occupied = true;
            slot->stableId = source.stableId;
            slot->species = source.species;
            slot->activity = source.activity;
            slot->cooldownSeconds =
                initialAnimalCooldown(source.stableId, source.activity);
        }
        else if (advanceTimers && slot->activity != source.activity)
        {
            if (source.activity == AnimalActivity::Flee)
                slot->cooldownSeconds = 0.f;
            else if (source.activity == AnimalActivity::Forage)
                slot->cooldownSeconds =
                    std::min(slot->cooldownSeconds, 1.f);
        }

        slot->species = source.species;
        slot->activity = source.activity;
        slot->position = source.position;
        slot->unseenSeconds = 0.f;
        slot->seenThisFrame = true;
    }

    for (AnimalSlot &slot : state.animalSlots)
    {
        if (!slot.occupied || slot.seenThisFrame)
            continue;
        if (advanceTimers)
            slot.unseenSeconds += delta;
        if (slot.unseenSeconds > AnimalForgetSeconds)
            slot = {};
    }
}

inline void appendFootstep(State &state, const Input &input,
                           Frame &frame) noexcept
{
    const bool canStep = !input.paused && !input.uiBlocked && input.grounded &&
                         !input.flying &&
                         footstepCue(input.supportSurface) != CueKind::None;
    if (!state.playerPositionInitialized || !finite(input.playerPosition))
    {
        resetStepTracking(state, input.playerPosition,
                          input.playerPositionValid);
        return;
    }

    const float moved = horizontalDistance(state.previousPlayerPosition,
                                           input.playerPosition);
    state.previousPlayerPosition = input.playerPosition;
    if (!canStep || !finite(moved) ||
        moved > MaximumTrustedHorizontalDisplacement)
    {
        state.accumulatedStepDistance = 0.f;
        state.accumulatedStepSurface = SurfaceKind::Unknown;
        return;
    }

    if (state.accumulatedStepSurface != input.supportSurface)
    {
        state.accumulatedStepDistance = 0.f;
        state.accumulatedStepSurface = input.supportSurface;
    }
    state.accumulatedStepDistance += moved;
    if (!finite(state.accumulatedStepDistance))
    {
        state.accumulatedStepDistance = 0.f;
        return;
    }
    if (state.accumulatedStepDistance < FootstepStrideMetres)
        return;

    state.accumulatedStepDistance =
        std::fmod(state.accumulatedStepDistance, FootstepStrideMetres);
    const std::uint8_t variant =
        static_cast<std::uint8_t>(state.footstepSequence++ % 2U);
    appendCue(frame, {footstepCue(input.supportSurface), 1.f, true,
                      input.playerPosition, 0, variant});
}

inline void appendAnimalCue(State &state, const Input &input,
                            Frame &frame) noexcept
{
    if (input.paused || state.animalGlobalCooldownSeconds > 0.f)
        return;

    AnimalSlot *candidate = nullptr;
    float candidatePriority = -1.f;
    float candidateDistance = std::numeric_limits<float>::infinity();
    for (AnimalSlot &slot : state.animalSlots)
    {
        if (!slot.occupied || !slot.seenThisFrame ||
            slot.cooldownSeconds > 0.f)
            continue;
        const float sourceDistance =
            distance(input.playerPosition, slot.position);
        if (!finite(sourceDistance) || sourceDistance > MaximumAnimalDistance)
            continue;
        const float priority = animalPriority(slot.activity);
        if (!candidate || priority > candidatePriority ||
            (priority == candidatePriority &&
             sourceDistance < candidateDistance) ||
            (priority == candidatePriority &&
             sourceDistance == candidateDistance &&
             slot.stableId < candidate->stableId))
        {
            candidate = &slot;
            candidatePriority = priority;
            candidateDistance = sourceDistance;
        }
    }
    if (!candidate)
        return;

    if (appendCue(frame,
                  {animalCue(candidate->species), 1.f, true,
                   candidate->position, candidate->stableId}))
    {
        candidate->cooldownSeconds = animalCooldown(
            candidate->species, candidate->activity, input.daylight);
        state.animalGlobalCooldownSeconds = AnimalGlobalCooldownSeconds;
    }
}

inline void appendAmbientCue(State &state, const Input &input,
                             Frame &frame) noexcept
{
    if (input.paused || state.ambientGlobalCooldownSeconds > 0.f)
        return;

    std::size_t candidate = AmbientKindCount;
    float candidateWeight = -1.f;
    for (std::size_t index = 0; index < AmbientKindCount; ++index)
    {
        if (state.ambientWeights[index] < MinimumCueLayerWeight ||
            state.ambientCountdownSeconds[index] > 0.f)
            continue;
        if (candidate == AmbientKindCount ||
            state.ambientWeights[index] > candidateWeight)
        {
            candidate = index;
            candidateWeight = state.ambientWeights[index];
        }
    }
    if (candidate == AmbientKindCount)
        return;

    const AmbientKind kind = static_cast<AmbientKind>(candidate);
    const float dayGain = .72f + .28f * sanitizeDaylight(input.daylight);
    if (appendCue(frame,
                  {ambientCue(kind), clamp01(candidateWeight * dayGain),
                   state.ambientHasPosition[candidate],
                   state.ambientPositions[candidate], 0}))
    {
        const float jitter =
            .9f + .05f * static_cast<float>(state.ambientSequence++ % 5U);
        state.ambientCountdownSeconds[candidate] =
            ambientInterval(kind, input.daylight) * jitter;
        state.ambientGlobalCooldownSeconds = AmbientGlobalCooldownSeconds;
    }
}

inline void buildAmbientLayers(const State &state, Frame &frame) noexcept
{
    std::array<bool, AmbientKindCount> selected{};
    for (std::size_t output = 0; output < MaxAmbientLayers; ++output)
    {
        std::size_t best = AmbientKindCount;
        float bestWeight = MinimumAudibleLayerWeight;
        for (std::size_t index = 0; index < AmbientKindCount; ++index)
        {
            if (!selected[index] && state.ambientWeights[index] > bestWeight)
            {
                best = index;
                bestWeight = state.ambientWeights[index];
            }
        }
        if (best == AmbientKindCount)
            break;
        selected[best] = true;
        frame.ambientLayers[frame.ambientLayerCount++] = {
            static_cast<AmbientKind>(best), bestWeight,
            state.ambientHasPosition[best], state.ambientPositions[best]};
    }
}

inline Frame update(State &state, const Input &input) noexcept
{
    Frame frame;
    if (!input.worldActive || !input.playerPositionValid ||
        !finite(input.playerPosition))
    {
        state = {};
        frame.resetThisFrame = true;
        return frame;
    }

    const auto targets = sanitizeEnvironmentTargets(input.environment);
    const float delta = sanitizeDelta(input.deltaSeconds);
    const bool epochChanged =
        !state.epochInitialized || state.worldEpoch != input.worldEpoch;
    if (epochChanged)
    {
        initializeEpoch(state, input, targets);
        frame.resetThisFrame = true;
    }
    else if (!input.paused)
    {
        const float alpha = delta <= 0.f
                                ? 0.f
                                : 1.f - std::exp(
                                      -delta / EnvironmentTransitionSeconds);
        for (std::size_t index = 0; index < AmbientKindCount; ++index)
        {
            if (targets[index].hasPosition && targets[index].weight > 0.f)
            {
                state.ambientHasPosition[index] = true;
                state.ambientPositions[index] = targets[index].position;
            }
            state.ambientWeights[index] +=
                (targets[index].weight - state.ambientWeights[index]) * alpha;
            state.ambientWeights[index] =
                clamp01(state.ambientWeights[index]);
            if (targets[index].weight <= 0.f &&
                state.ambientWeights[index] < MinimumAudibleLayerWeight)
            {
                state.ambientHasPosition[index] = false;
                state.ambientPositions[index] = {};
            }
            if (state.ambientWeights[index] >= MinimumCueLayerWeight)
                state.ambientCountdownSeconds[index] -= delta;
            if (!finite(state.ambientCountdownSeconds[index]))
                state.ambientCountdownSeconds[index] =
                    ambientInterval(static_cast<AmbientKind>(index),
                                    input.daylight);
        }
        state.ambientGlobalCooldownSeconds =
            std::max(0.f, state.ambientGlobalCooldownSeconds - delta);
        state.animalGlobalCooldownSeconds =
            std::max(0.f, state.animalGlobalCooldownSeconds - delta);
    }

    synchronizeAnimals(state, input, delta, !input.paused && !epochChanged);
    buildAmbientLayers(state, frame);

    if (epochChanged)
        return frame;

    appendFootstep(state, input, frame);
    appendAnimalCue(state, input, frame);
    appendAmbientCue(state, input, frame);
    return frame;
}
} // namespace AdventureAudioPresentation
