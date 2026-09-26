#include "HelloMine3D/Presentation/AdventureAudioPresentation.h"
#include "HelloMine3D/Presentation/AdventureAudioWorldAdapter.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

namespace A = AdventureAudioPresentation;
namespace W = AdventureAudioWorldAdapter;

void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool close(float first, float second, float epsilon = .001f)
{
    return std::abs(first - second) <= epsilon;
}

bool hasCue(const A::Frame &frame, A::CueKind kind)
{
    for (std::size_t index = 0; index < frame.cueCount; ++index)
        if (frame.cues[index].kind == kind)
            return true;
    return false;
}

const A::Cue *findCue(const A::Frame &frame, A::CueKind kind)
{
    for (std::size_t index = 0; index < frame.cueCount; ++index)
        if (frame.cues[index].kind == kind)
            return &frame.cues[index];
    return nullptr;
}

A::Input baseInput()
{
    A::Input input;
    input.deltaSeconds = .1f;
    input.worldEpoch = 7;
    input.worldActive = true;
    input.playerPositionValid = true;
    input.playerPosition = {0.f, 2.f, 0.f};
    input.grounded = true;
    input.supportSurface = A::SurfaceKind::GrassDirt;
    input.daylight = 1.f;
    return input;
}

int main()
{
    int passed = 0;

    {
        require(W::surfaceKind(BlockId::Grass) == A::SurfaceKind::GrassDirt &&
                    W::surfaceKind(BlockId::MossStone) == A::SurfaceKind::Stone &&
                    W::surfaceKind(BlockId::OakPlank) == A::SurfaceKind::Wood &&
                    W::surfaceKind(BlockId::Gravel) == A::SurfaceKind::Sand &&
                    W::surfaceKind(BlockId::Water) == A::SurfaceKind::Unknown,
                "actual support blocks must map to the four footstep families");
        ++passed;
    }

    {
        require(W::animalSpecies("hellomine:meadow_sheep") ==
                        A::AnimalSpecies::Sheep &&
                    W::animalSpecies("hellomine:forest_rabbit") ==
                        A::AnimalSpecies::Rabbit &&
                    W::animalSpecies("hellomine:marsh_bird") ==
                        A::AnimalSpecies::WetlandBird &&
                    W::animalSpecies("hellomine:stalker") ==
                        A::AnimalSpecies::Unknown &&
                    W::animalActivity(3) == A::AnimalActivity::Flee &&
                    W::animalActivity(99) == A::AnimalActivity::Rest,
                "only real wildlife snapshots may enter the animal sound schedule");
        ++passed;
    }

    {
        W::EnvironmentAccumulator coast;
        coast.add({TerrainBiome::Grassland, true, 64, BlockId::Grass,
                   {0.f, 65.f, 0.f}, 2.f});
        coast.add({TerrainBiome::Ocean, true, 62, BlockId::Water,
                   {12.f, 63.f, 0.f}, 1.f});
        const auto coastTargets = coast.targets();
        require(coastTargets[static_cast<std::size_t>(A::AmbientKind::OpenLand)]
                            .weight == 1.f &&
                    coastTargets[static_cast<std::size_t>(A::AmbientKind::Coast)]
                            .weight > .5f &&
                    coastTargets[static_cast<std::size_t>(A::AmbientKind::Coast)]
                            .hasPosition &&
                    coastTargets[static_cast<std::size_t>(
                                     A::AmbientKind::InlandWater)]
                            .weight == 0.f,
                "resident shoreline probes must produce a positioned coast layer");

        W::EnvironmentAccumulator river;
        river.add({TerrainBiome::LightForest, true, 70, BlockId::ForestFloor,
                   {0.f, 71.f, 0.f}, 2.f});
        river.add({TerrainBiome::Grassland, true, 68, BlockId::Water,
                   {8.f, 69.f, 0.f}, 1.f});
        const auto riverTargets = river.targets();
        require(riverTargets[static_cast<std::size_t>(A::AmbientKind::Forest)]
                            .weight > .99f &&
                    riverTargets[static_cast<std::size_t>(
                                     A::AmbientKind::InlandWater)]
                            .weight > .5f &&
                    riverTargets[static_cast<std::size_t>(A::AmbientKind::Coast)]
                            .weight == 0.f,
                "resident inland water and forest biome facts must stay distinct");

        W::EnvironmentAccumulator openOcean;
        openOcean.add({TerrainBiome::Ocean, true, 60, BlockId::Water,
                       {0.f, 61.f, 0.f}, 3.f});
        openOcean.add({TerrainBiome::Ocean, true, 60, BlockId::Water,
                       {8.f, 61.f, 0.f}, 1.f});
        const auto openOceanTargets = openOcean.targets();
        require(openOceanTargets[static_cast<std::size_t>(
                                     A::AmbientKind::Coast)]
                            .weight == 0.f,
                "open ocean without nearby land must not masquerade as coast");
        ++passed;
    }

    {
        require(std::strcmp(A::cueId(A::CueKind::AmbientOpenLand),
                            "ambient.open") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::AmbientForest),
                                "ambient.forest") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::AmbientInlandWater),
                                "ambient.river") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::AmbientCoast),
                                "ambient.coast") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::FootstepSand, 0),
                                "footstep.sand.1") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::FootstepSand, 1),
                                "footstep.sand.2") == 0 &&
                    std::strcmp(A::cueId(A::CueKind::AnimalWetlandBird),
                                "animal.marsh-bird") == 0,
                "cue ids must match the frozen B9 resource names");
        ++passed;
    }

    {
        std::array<A::EnvironmentTarget, A::AmbientKindCount> raw{};
        raw[0].weight = .8f;
        raw[1].weight = .8f;
        raw[2].weight = .5f;
        raw[3].weight = .9f;
        raw[3].hasPosition = true;
        raw[3].position.x = std::numeric_limits<float>::quiet_NaN();
        const auto targets = A::sanitizeEnvironmentTargets(raw);
        require(close(targets[0].weight, .5f) &&
                    close(targets[1].weight, .5f) &&
                    close(targets[2].weight, 0.f) &&
                    close(targets[3].weight, .9f) &&
                    !targets[3].hasPosition,
                "targets must normalize land, choose one water type and reject invalid positions");
        ++passed;
    }

    {
        A::State state;
        state.ambientWeights = {.9f, .8f, .7f, .6f};
        A::Frame frame;
        A::buildAmbientLayers(state, frame);
        require(frame.ambientLayerCount == A::MaxAmbientLayers &&
                    frame.ambientLayers[0].kind == A::AmbientKind::OpenLand &&
                    frame.ambientLayers[2].kind == A::AmbientKind::InlandWater,
                "environment mix must retain only the strongest three bounded layers");
        ++passed;
    }

    {
        require(A::ambientReplayInterval(A::AmbientKind::Forest, 0.f) >
                    A::ambientReplayInterval(A::AmbientKind::Forest, 1.f) &&
                    A::ambientLayerGain(1.f, 0.f) <
                        A::ambientLayerGain(1.f, 1.f) &&
                    A::ambientLayerGain(2.f, 1.f) <= 1.f,
                "daylight must change bounded environment density and gain");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.environment[0].weight = 1.f;
        (void)A::update(state, input);
        input.environment[0].weight = 0.f;
        input.environment[1].weight = 1.f;
        float previousForest = 0.f;
        for (int index = 0; index < 8; ++index)
        {
            const auto frame = A::update(state, input);
            require(state.ambientWeights[1] >= previousForest &&
                        frame.ambientLayerCount <= A::MaxAmbientLayers,
                    "environment changes must be smooth and remain bounded");
            previousForest = state.ambientWeights[1];
        }
        require(state.ambientWeights[0] > 0.f &&
                    state.ambientWeights[1] > 0.f,
                "region transition must cross-fade rather than snap");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.animalCount = 2;
        input.animals[0] = {11, A::AnimalSpecies::Sheep,
                            A::AnimalActivity::Flee, {33.f, 2.f, 0.f}};
        input.animals[1] = {12, A::AnimalSpecies::Rabbit,
                            A::AnimalActivity::Forage, {5.f, 2.f, 0.f}};
        (void)A::update(state, input);
        for (auto &slot : state.animalSlots)
            if (slot.occupied)
                slot.cooldownSeconds = 0.f;
        const auto frame = A::update(state, input);
        require(frame.cueCount == 1 &&
                    frame.cues[0].stableSourceId == 12 &&
                    close(frame.cues[0].gain, 1.f),
                "an inaudible urgent source must not silence a nearby real animal");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.environment[1].weight = 1.f;
        (void)A::update(state, input);
        state.ambientCountdownSeconds[1] = 0.f;
        input.environment[1].hasPosition = true;
        input.environment[1].position = {3.f, 4.f, 5.f};
        const auto frame = A::update(state, input);
        const A::Cue *cue = findCue(frame, A::CueKind::AmbientForest);
        require(cue && cue->spatial && close(cue->position.z, 5.f) &&
                    cue->gain > 0.f && cue->gain <= 1.f &&
                    state.ambientGlobalCooldownSeconds > 0.f,
                "active regions must emit low-density positioned ambient cues");
        ++passed;
    }

    {
        require(A::ambientInterval(A::AmbientKind::Forest, 1.f) <
                    A::ambientInterval(A::AmbientKind::Forest, 0.f) &&
                    A::animalCooldown(A::AnimalSpecies::WetlandBird,
                                      A::AnimalActivity::Forage, 1.f) <
                        A::animalCooldown(A::AnimalSpecies::WetlandBird,
                                          A::AnimalActivity::Forage, 0.f),
                "daylight must adjust ambient and wildlife density without changing world state");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.environment[0].weight = 1.f;
        (void)A::update(state, input);
        const float weight = state.ambientWeights[0];
        const float countdown = state.ambientCountdownSeconds[0];
        input.environment[0].weight = 0.f;
        input.environment[1].weight = 1.f;
        input.paused = true;
        input.deltaSeconds = 99.f;
        const auto frame = A::update(state, input);
        require(close(state.ambientWeights[0], weight) &&
                    close(state.ambientCountdownSeconds[0], countdown) &&
                    frame.cueCount == 0,
                "pause must freeze fades and cue clocks without catch-up");
        ++passed;
    }

    for (const auto pair : {
             std::pair<A::SurfaceKind, A::CueKind>{
                 A::SurfaceKind::GrassDirt, A::CueKind::FootstepGrassDirt},
             {A::SurfaceKind::Stone, A::CueKind::FootstepStone},
             {A::SurfaceKind::Wood, A::CueKind::FootstepWood},
             {A::SurfaceKind::Sand, A::CueKind::FootstepSand}})
    {
        A::State state;
        A::Input input = baseInput();
        input.supportSurface = pair.first;
        (void)A::update(state, input);
        input.playerPosition.x = .85f;
        require(!hasCue(A::update(state, input), pair.second),
                "partial stride must not emit a footstep");
        input.playerPosition.x = 1.70f;
        const auto first = A::update(state, input);
        const A::Cue *firstCue = findCue(first, pair.second);
        require(firstCue && firstCue->spatial && firstCue->variant == 0 &&
                    std::strstr(A::cueId(*firstCue), ".1") != nullptr,
                "actual horizontal stride must emit the matching material cue");
        input.playerPosition.x = 2.55f;
        (void)A::update(state, input);
        input.playerPosition.x = 3.40f;
        const auto second = A::update(state, input);
        const A::Cue *secondCue = findCue(second, pair.second);
        require(secondCue && secondCue->variant == 1 &&
                    std::strstr(A::cueId(*secondCue), ".2") != nullptr,
                "successive footsteps must alternate bounded sample variants");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        (void)A::update(state, input);
        input.playerPosition.x = 1.f;
        (void)A::update(state, input);
        input.paused = true;
        input.playerPosition.x = 2.f;
        require(A::update(state, input).cueCount == 0 &&
                    close(state.accumulatedStepDistance, 0.f),
                "pause must reset pending step distance");
        input.paused = false;
        input.playerPosition.x = 2.7f;
        require(!hasCue(A::update(state, input),
                        A::CueKind::FootstepGrassDirt),
                "resume must not replay distance travelled while paused");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        (void)A::update(state, input);
        input.playerPosition.x = 1.f;
        (void)A::update(state, input);
        input.uiBlocked = true;
        input.playerPosition.x = 2.f;
        (void)A::update(state, input);
        input.uiBlocked = false;
        input.playerPosition.x = 2.7f;
        require(!hasCue(A::update(state, input),
                        A::CueKind::FootstepGrassDirt),
                "UI capture must discard movement instead of queuing footsteps");
        ++passed;
    }

    {
        for (int condition = 0; condition < 2; ++condition)
        {
            A::State state;
            A::Input input = baseInput();
            (void)A::update(state, input);
            input.playerPosition.x = 1.f;
            (void)A::update(state, input);
            input.grounded = condition != 0;
            input.flying = condition == 0;
            if (condition != 0)
                input.grounded = false;
            input.playerPosition.x = 2.f;
            require(A::update(state, input).cueCount == 0 &&
                        close(state.accumulatedStepDistance, 0.f),
                    "airborne and flying movement must reset step accumulation");
        }
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        (void)A::update(state, input);
        input.playerPosition.x = 10.f;
        require(!hasCue(A::update(state, input),
                        A::CueKind::FootstepGrassDirt) &&
                    close(state.accumulatedStepDistance, 0.f),
                "large displacements must be treated as teleports");
        input.playerPosition.x = 11.f;
        require(!hasCue(A::update(state, input),
                        A::CueKind::FootstepGrassDirt),
                "post-teleport movement must start a fresh stride");
        input.worldEpoch++;
        input.playerPosition.x = 100.f;
        const auto reset = A::update(state, input);
        require(reset.resetThisFrame && reset.cueCount == 0 &&
                    close(state.accumulatedStepDistance, 0.f),
                "epoch changes must reset all transient movement state");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.animalCount = 1;
        input.animals[0] = {42, A::AnimalSpecies::Sheep,
                            A::AnimalActivity::Flee, {3.f, 2.f, 0.f}};
        require(A::update(state, input).cueCount == 0,
                "new epoch must establish animal baselines without sound");
        const auto frame = A::update(state, input);
        const A::Cue *cue = findCue(frame, A::CueKind::AnimalSheep);
        require(cue && cue->stableSourceId == 42 && cue->spatial &&
                    close(cue->position.x, 3.f),
                "real fleeing animal must emit one positioned stable-id cue");
        require(!hasCue(A::update(state, input), A::CueKind::AnimalSheep),
                "per-animal and global cooldowns must suppress repeats");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.animalCount = A::MaxAnimalSources;
        for (std::size_t index = 0; index < input.animalCount; ++index)
        {
            input.animals[index] = {
                static_cast<std::uint64_t>(100 - index),
                A::AnimalSpecies::WetlandBird, A::AnimalActivity::Flee,
                {2.f, 2.f, 0.f}};
        }
        (void)A::update(state, input);
        const auto frame = A::update(state, input);
        require(frame.cueCount == 1 &&
                    frame.cues[0].kind == A::CueKind::AnimalWetlandBird &&
                    frame.cues[0].stableSourceId == 77,
                "twenty-four simultaneous animals must yield at most one deterministic call");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.animalCount = 2;
        input.animals[0] = {1, A::AnimalSpecies::Rabbit,
                            A::AnimalActivity::Forage, {1.f, 2.f, 0.f}};
        input.animals[1] = {2, A::AnimalSpecies::Sheep,
                            A::AnimalActivity::Flee, {10.f, 2.f, 0.f}};
        (void)A::update(state, input);
        for (auto &slot : state.animalSlots)
            if (slot.occupied)
                slot.cooldownSeconds = 0.f;
        const auto frame = A::update(state, input);
        require(frame.cueCount == 1 &&
                    frame.cues[0].stableSourceId == 2,
                "real activity urgency must outrank proximity for animal calls");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.animalCount = 2;
        input.animals[0] = {0, A::AnimalSpecies::Sheep,
                            A::AnimalActivity::Flee, {1.f, 2.f, 0.f}};
        input.animals[1] = {9, A::AnimalSpecies::Rabbit,
                            A::AnimalActivity::Flee, {100.f, 2.f, 0.f}};
        (void)A::update(state, input);
        require(A::update(state, input).cueCount == 0,
                "invalid stable IDs and animals outside the audible bound must be ignored");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.environment[1].weight = 1.f;
        input.animalCount = 1;
        input.animals[0] = {5, A::AnimalSpecies::Rabbit,
                            A::AnimalActivity::Flee, {1.f, 2.f, 0.f}};
        (void)A::update(state, input);
        state.ambientCountdownSeconds[1] = 0.f;
        state.accumulatedStepDistance = 1.6f;
        state.accumulatedStepSurface = A::SurfaceKind::GrassDirt;
        input.playerPosition.x = .1f;
        const auto frame = A::update(state, input);
        require(frame.cueCount == A::MaxCuesPerFrame &&
                    hasCue(frame, A::CueKind::FootstepGrassDirt) &&
                    hasCue(frame, A::CueKind::AnimalRabbit) &&
                    hasCue(frame, A::CueKind::AmbientForest),
                "one footstep, one animal and one ambient cue must fit the fixed frame budget");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        input.environment[0].weight =
            std::numeric_limits<float>::quiet_NaN();
        input.environment[1].weight =
            std::numeric_limits<float>::infinity();
        input.daylight = std::numeric_limits<float>::quiet_NaN();
        input.deltaSeconds = std::numeric_limits<float>::infinity();
        const auto frame = A::update(state, input);
        require(frame.cueCount == 0 && frame.ambientLayerCount == 0 &&
                    close(A::sanitizeDaylight(input.daylight), .5f) &&
                    close(A::sanitizeDelta(input.deltaSeconds), 0.f),
                "non-finite presentation input must fail silent and remain finite");
        ++passed;
    }

    {
        A::State state;
        A::Input input = baseInput();
        (void)A::update(state, input);
        input.worldActive = false;
        const auto frame = A::update(state, input);
        require(frame.resetThisFrame && frame.cueCount == 0 &&
                    !state.epochInitialized &&
                    !state.playerPositionInitialized,
                "world detach must clear all transient presentation state");
        ++passed;
    }

    std::cout << "[ADVENTURE_AUDIO_PRESENTATION] " << passed << '/' << passed
              << " checks passed\n";
    return 0;
}
