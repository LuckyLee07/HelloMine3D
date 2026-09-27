#pragma once

#include <map>

namespace {

struct AdventureUndergroundFixture {
    int seed;
    int cellX;
    int cellZ;
    int anchorX;
    int anchorY;
    int anchorZ;
    int directionX;
    int directionZ;
    int endY;
    std::uint64_t v22AnchorChunkHash;
    std::uint64_t v22RegionHash;
};

struct AdventureUndergroundGeneratedChunk {
    glm::ivec2 location{0};
    std::unique_ptr<Chunk> chunk;
};

std::vector<glm::ivec2> adventureUndergroundLocations(
    const CaveGenerator::AdventureUndergroundPlan &plan)
{
    const int poolX = plan.chamberX + plan.poolDirectionX * 5;
    const int poolZ = plan.chamberZ + plan.poolDirectionZ * 5;
    int minimumX = std::min({plan.entrance.anchorX, plan.chamberX - 10,
                             plan.destinationX - 6, poolX - 4});
    int maximumX = std::max({plan.entrance.anchorX, plan.chamberX + 10,
                             plan.destinationX + 6, poolX + 4});
    int minimumZ = std::min({plan.entrance.anchorZ, plan.chamberZ - 10,
                             plan.destinationZ - 6, poolZ - 4});
    int maximumZ = std::max({plan.entrance.anchorZ, plan.chamberZ + 10,
                             plan.destinationZ + 6, poolZ + 4});
    std::vector<glm::ivec2> locations;
    for (int chunkX = WorldCoordinates::floorDiv(minimumX, CHUNK_SIZE);
         chunkX <= WorldCoordinates::floorDiv(maximumX, CHUNK_SIZE);
         ++chunkX) {
        for (int chunkZ = WorldCoordinates::floorDiv(minimumZ, CHUNK_SIZE);
             chunkZ <= WorldCoordinates::floorDiv(maximumZ, CHUNK_SIZE);
             ++chunkZ) {
            locations.emplace_back(chunkX, chunkZ);
        }
    }
    return locations;
}

std::vector<AdventureUndergroundGeneratedChunk>
generateAdventureUndergroundChunks(
    World &world, ClassicOverWorldGenerator &generator,
    std::vector<glm::ivec2> locations, bool reverse)
{
    if (reverse) {
        std::reverse(locations.begin(), locations.end());
    }
    std::vector<AdventureUndergroundGeneratedChunk> generated;
    for (const glm::ivec2 &location : locations) {
        auto chunk = std::make_unique<Chunk>(world, location, false);
        generator.generateTerrainFor(*chunk);
        generated.push_back({location, std::move(chunk)});
    }
    return generated;
}

const Chunk *adventureUndergroundChunkAt(
    const std::vector<AdventureUndergroundGeneratedChunk> &chunks,
    int worldX, int worldZ)
{
    const glm::ivec2 location{
        WorldCoordinates::floorDiv(worldX, CHUNK_SIZE),
        WorldCoordinates::floorDiv(worldZ, CHUNK_SIZE)};
    const auto found = std::find_if(
        chunks.begin(), chunks.end(), [&location](const auto &entry) {
            return entry.location == location;
        });
    return found == chunks.end() ? nullptr : found->chunk.get();
}

BlockId adventureUndergroundBlock(
    const std::vector<AdventureUndergroundGeneratedChunk> &chunks,
    int worldX, int y, int worldZ)
{
    const Chunk *chunk = adventureUndergroundChunkAt(
        chunks, worldX, worldZ);
    if (chunk == nullptr) {
        return BlockId::Air;
    }
    return static_cast<BlockId>(chunk->getBlock(
        WorldCoordinates::floorMod(worldX, CHUNK_SIZE), y,
        WorldCoordinates::floorMod(worldZ, CHUNK_SIZE)).id);
}

std::string adventureUndergroundEntitySignature(const Chunk &chunk)
{
    std::vector<std::string> records;
    for (const auto &record : chunk.getBlockEntities()) {
        records.push_back(std::to_string(record.position.x) + "," +
            std::to_string(record.position.y) + "," +
            std::to_string(record.position.z) + ":" + record.type + ":" +
            record.payload);
    }
    std::sort(records.begin(), records.end());
    std::string signature;
    for (const auto &record : records) {
        signature += record;
        signature.push_back('\n');
    }
    return signature;
}

std::uint64_t adventureUndergroundRegionHash(
    const std::vector<AdventureUndergroundGeneratedChunk> &chunks)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto append = [&hash](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (value >> (byte * 8)) & 0xffull;
            hash *= 1099511628211ull;
        }
    };
    for (const auto &entry : chunks) {
        append(static_cast<std::uint64_t>(
            static_cast<std::int64_t>(entry.location.x)));
        append(static_cast<std::uint64_t>(
            static_cast<std::int64_t>(entry.location.y)));
        append(TerrainSurvey::blockHash(*entry.chunk));
    }
    return hash;
}

bool sameAdventureUndergroundPlan(
    const CaveGenerator::AdventureUndergroundPlan &left,
    const CaveGenerator::AdventureUndergroundPlan &right)
{
    return left.valid == right.valid &&
        left.entrance.valid == right.entrance.valid &&
        left.entrance.cellX == right.entrance.cellX &&
        left.entrance.cellZ == right.entrance.cellZ &&
        left.entrance.anchorX == right.entrance.anchorX &&
        left.entrance.anchorY == right.entrance.anchorY &&
        left.entrance.anchorZ == right.entrance.anchorZ &&
        left.entrance.directionX == right.entrance.directionX &&
        left.entrance.directionZ == right.entrance.directionZ &&
        left.entrance.endY == right.entrance.endY &&
        left.layout == right.layout &&
        left.chamberX == right.chamberX &&
        left.chamberAirY == right.chamberAirY &&
        left.chamberZ == right.chamberZ &&
        left.riftDirectionX == right.riftDirectionX &&
        left.riftDirectionZ == right.riftDirectionZ &&
        left.riftEndAirY == right.riftEndAirY &&
        left.destinationX == right.destinationX &&
        left.destinationZ == right.destinationZ &&
        left.poolDirectionX == right.poolDirectionX &&
        left.poolDirectionZ == right.poolDirectionZ &&
        left.stableKey == right.stableKey;
}

struct AdventureUndergroundProjectionEquivalence {
    bool equivalent = true;
    std::size_t comparedChunks = 0;
    std::size_t changedChunks = 0;
    std::size_t publicWrites = 0;
    glm::ivec2 firstMismatch{0};
    std::uint64_t productionHash = 0;
    std::uint64_t publicHash = 0;
    bool entityMismatch = false;
};

AdventureUndergroundProjectionEquivalence
compareAdventureUndergroundProductionWithPublicProjection(
    World &world, int seed,
    const std::vector<AdventureUndergroundGeneratedChunk> &production)
{
    AdventureUndergroundProjectionEquivalence result;
    ClassicOverWorldGenerator legacy(
        seed, LandmarkWorkshopTerrainGenerationVersion);
    ClassicOverWorldGenerator exactSampler(
        seed, AdventureUndergroundTerrainGenerationVersion);
    CaveGenerator publicProjection(
        seed, AdventureUndergroundTerrainGenerationVersion);
    const auto surface = [&exactSampler](int x, int z) {
        return exactSampler.getSurfaceHeightAtWorld(x, z);
    };
    const auto biome = [&exactSampler](int x, int z) {
        return exactSampler.getBiomeAtWorld(x, z);
    };

    for (const auto &expected : production) {
        Chunk reference(world, expected.location, false);
        legacy.generateTerrainFor(reference);
        const std::size_t changed =
            publicProjection.projectAdventureUnderground(
                reference, surface, biome);
        result.publicWrites += changed;
        result.changedChunks += changed != 0 ? 1u : 0u;
        ++result.comparedChunks;

        const std::uint64_t productionHash =
            TerrainSurvey::blockHash(*expected.chunk);
        const std::uint64_t publicHash =
            TerrainSurvey::blockHash(reference);
        const bool sameEntities =
            adventureUndergroundEntitySignature(*expected.chunk) ==
            adventureUndergroundEntitySignature(reference);
        if ((productionHash != publicHash || !sameEntities) &&
            result.equivalent) {
            result.equivalent = false;
            result.firstMismatch = expected.location;
            result.productionHash = productionHash;
            result.publicHash = publicHash;
            result.entityMismatch = !sameEntities;
        }
    }
    return result;
}

int adventureUndergroundDirectionBit(int x, int z)
{
    if (x == 1 && z == 0) return 1;
    if (x == -1 && z == 0) return 2;
    if (x == 0 && z == 1) return 4;
    if (x == 0 && z == -1) return 8;
    return 0;
}

// Engineering replay only: use real generated blocks and Player physics, with
// synthetic controls. This is not an ordinary-input exploration acceptance run.
void checkAdventureUndergroundPlayerReturn(
    int seed, const CaveGenerator::AdventureUndergroundPlan &plan)
{
    std::vector<glm::ivec3> route;
    for (int step = 0; step <= 29; ++step) {
        route.push_back({
            plan.entrance.anchorX + plan.entrance.directionX * step,
            plan.entrance.anchorY - std::min(step, 24) * 3 / 4,
            plan.entrance.anchorZ + plan.entrance.directionZ * step});
    }
    for (int step = 1; step <= 29; ++step) {
        route.push_back({
            plan.chamberX + plan.riftDirectionX * step,
            plan.chamberAirY - (plan.chamberAirY - plan.riftEndAirY) *
                std::max(0, step - 6) / 23,
            plan.chamberZ + plan.riftDirectionZ * step});
    }
    const auto directory = freshSaveDirectory(
        "underground_player_return_" + std::to_string(seed));
    if (!initializeTerrainIdentity(directory, "underground-player-return",
            AdventureUndergroundTerrainGenerationVersion, seed)) {
        check("ADVENTURE-UNDERGROUND/player-route-world", false);
        return;
    }
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    World world(camera, config, player, directory, false, 0);
    for (const auto &location : adventureUndergroundLocations(plan)) {
        world.getChunkManager().loadChunk(location.x, location.y);
    }
    auto start = player.getSaveState();
    start.position = glm::vec3(route.front()) + glm::vec3(0.5f, 1.f, 0.5f);
    player.applySaveState(start);
    player.box.update(player.position);
    player.update(0.05f, world);

    glm::ivec3 targetCell = route.front();
    int ticks = 0;
    const auto followRoute = [&](bool allowJump) {
        bool arrived = true;
        ticks = 0;
        for (const auto &cell : route) {
            targetCell = cell;
            const glm::vec3 target = glm::vec3(cell) + glm::vec3(0.5f, 1.f, 0.5f);
            arrived = false;
            for (int tick = 0; tick < 120; ++tick) {
                const glm::vec2 offset(target.x - player.position.x,
                                       target.z - player.position.z);
                const float distance = glm::length(offset);
                if (distance < 0.2f && player.isOnGround() &&
                    std::abs(player.position.y - target.y) < 0.01f) {
                    arrived = true;
                    break;
                }
                PlayerInputState input;
                input.moveForward = distance >= 0.2f;
                if (input.moveForward) {
                    input.lookDelta.x = glm::degrees(std::atan2(offset.x, -offset.y)) -
                        player.rotation.y;
                }
                input.jumpPressed = allowJump && player.isOnGround() &&
                    target.y > player.position.y + 0.1f && distance < 1.2f;
                player.applyInput(input);
                player.update(0.05f, world);
                ++ticks;
            }
            if (!arrived) break;
        }
        return arrived;
    };
    const auto details = [&]() {
        return "target=" + vecToString(glm::vec3(targetCell)) +
            " actual=" + vecToString(player.position) +
            " ticks=" + std::to_string(ticks);
    };
    for (bool returning : {false, true}) {
        if (returning) {
            std::reverse(route.begin(), route.end());
            // Negative control on the same terrain, then restore only this
            // test player's destination pose before the positive return leg.
            const auto destination = player.getSaveState();
            const bool blockedWithoutJump = !followRoute(false);
            check("ADVENTURE-UNDERGROUND/return-requires-jump-" +
                      std::to_string(seed), blockedWithoutJump, details());
            player.applySaveState(destination);
            player.box.update(player.position);
            player.update(0.05f, world);
        }
        const bool arrived = followRoute(true);
        check(std::string("ADVENTURE-UNDERGROUND/player-physics-") +
                  (returning ? "return-" : "entry-") + std::to_string(seed),
              arrived, details());
        if (!arrived) break;
    }
}

void caseAdventureUndergroundV23()
{
    check("ADVENTURE-UNDERGROUND/v23-appends-without-save-bump",
          LandmarkWorkshopTerrainGenerationVersion == 22 &&
          AdventureUndergroundTerrainGenerationVersion == 23 &&
          CurrentTerrainGenerationVersion == 23 &&
          WorldSaveFormatVersion == 12);

    const std::array<AdventureUndergroundFixture, 3> fixtures{{
        {42, 11, 2, 1139, 120, 237, 0, 1, 102,
         2779878355977114307ull, 11015563584882817809ull},
        {20260807, -5, -5, -465, 89, -431, 0, -1, 71,
         12791051408071790227ull, 3150697917179740512ull},
        {239701883, 3, -6, 364, 95, -507, 1, 0, 77,
         3083864523038289622ull, 16527031545561352210ull},
    }};

    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World sampleWorld(camera, config, owner,
        freshSaveDirectory("adventure_underground_samples"), false, 0);

    std::array<CaveGenerator::AdventureUndergroundPlan, 3> plans{};
    std::array<bool, 2> layouts{{false, false}};
    std::array<bool, 2> equivalenceLayouts{{false, false}};
    int equivalenceDirections = 0;
    bool equivalenceNegativeCoordinates = false;
    bool equivalenceCrossesChunkBoundary = false;
    for (std::size_t fixtureIndex = 0;
         fixtureIndex < fixtures.size(); ++fixtureIndex) {
        const AdventureUndergroundFixture &fixture = fixtures[fixtureIndex];
        ClassicOverWorldGenerator v22(
            fixture.seed, LandmarkWorkshopTerrainGenerationVersion);
        CaveGenerator caveV22(
            fixture.seed, LandmarkWorkshopTerrainGenerationVersion);
        const auto v22Surface = [&v22](int x, int z) {
            return v22.getSurfaceHeightAtWorld(x, z);
        };
        const auto v22Biome = [&v22](int x, int z) {
            return v22.getBiomeAtWorld(x, z);
        };
        const auto oldEntrance = caveV22.getNaturalEntranceForCell(
            fixture.cellX, fixture.cellZ, v22Surface, v22Biome);
        const bool frozenEntrance = oldEntrance.valid &&
            oldEntrance.anchorX == fixture.anchorX &&
            oldEntrance.anchorY == fixture.anchorY &&
            oldEntrance.anchorZ == fixture.anchorZ &&
            oldEntrance.directionX == fixture.directionX &&
            oldEntrance.directionZ == fixture.directionZ &&
            oldEntrance.endY == fixture.endY;
        check("ADVENTURE-UNDERGROUND/v22-frozen-entrance-" +
                  std::to_string(fixture.seed),
              frozenEntrance,
              "cell=" + std::to_string(fixture.cellX) + "," +
                  std::to_string(fixture.cellZ) + " anchor=" +
                  std::to_string(oldEntrance.anchorX) + "," +
                  std::to_string(oldEntrance.anchorY) + "," +
                  std::to_string(oldEntrance.anchorZ));

        Chunk legacyChunk(sampleWorld,
            {WorldCoordinates::floorDiv(fixture.anchorX, CHUNK_SIZE),
             WorldCoordinates::floorDiv(fixture.anchorZ, CHUNK_SIZE)},
            false);
        v22.generateTerrainFor(legacyChunk);
        const std::uint64_t legacyHash =
            TerrainSurvey::blockHash(legacyChunk);
        std::cout << "[UNDERGROUND_V22] seed=" << fixture.seed
                  << " chunk=" << legacyChunk.getLocation().x << ','
                  << legacyChunk.getLocation().y << " hash="
                  << legacyHash << '\n';
        check("ADVENTURE-UNDERGROUND/v22-frozen-chunk-" +
                  std::to_string(fixture.seed),
              legacyHash == fixture.v22AnchorChunkHash,
              "hash=" + std::to_string(legacyHash));

        ClassicOverWorldGenerator v23(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        CaveGenerator caveV23(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        const auto surface = [&v23](int x, int z) {
            return v23.getSurfaceHeightAtWorld(x, z);
        };
        const auto biome = [&v23](int x, int z) {
            return v23.getBiomeAtWorld(x, z);
        };
        const auto plan = caveV23.getAdventureUndergroundPlanForCell(
            fixture.cellX, fixture.cellZ, surface, biome);
        CaveGenerator repeatedPlanner(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        const auto repeatedPlan =
            repeatedPlanner.getAdventureUndergroundPlanForCell(
                fixture.cellX, fixture.cellZ, surface, biome);
        plans[fixtureIndex] = plan;
        if (plan.valid) {
            layouts[static_cast<std::size_t>(plan.layout)] = true;
        }
        check("ADVENTURE-UNDERGROUND/v23-plan-" +
                  std::to_string(fixture.seed),
              plan.valid &&
                  plan.entrance.anchorX == fixture.anchorX &&
                  plan.entrance.anchorZ == fixture.anchorZ &&
                  plan.chamberAirY >= 8 && plan.riftEndAirY >= 8,
              "layout=" + std::to_string(static_cast<int>(plan.layout)) +
                  " chamber=" + std::to_string(plan.chamberX) + "," +
                  std::to_string(plan.chamberAirY) + "," +
                  std::to_string(plan.chamberZ) + " destination=" +
                  std::to_string(plan.destinationX) + "," +
                  std::to_string(plan.riftEndAirY) + "," +
                  std::to_string(plan.destinationZ));
        check("ADVENTURE-UNDERGROUND/repeated-plan-" +
                  std::to_string(fixture.seed),
              sameAdventureUndergroundPlan(plan, repeatedPlan));
        int entranceSurfaceQueries = 0;
        int entranceBiomeQueries = 0;
        int planSurfaceQueries = 0;
        int planBiomeQueries = 0;
        caveV23.getNaturalEntranceForCell(
            fixture.cellX, fixture.cellZ,
            [&v23, &entranceSurfaceQueries](int x, int z) {
                ++entranceSurfaceQueries;
                return v23.getSurfaceHeightAtWorld(x, z);
            },
            [&v23, &entranceBiomeQueries](int x, int z) {
                ++entranceBiomeQueries;
                return v23.getBiomeAtWorld(x, z);
            });
        caveV23.getAdventureUndergroundPlanForCell(
            fixture.cellX, fixture.cellZ,
            [&v23, &planSurfaceQueries](int x, int z) {
                ++planSurfaceQueries;
                return v23.getSurfaceHeightAtWorld(x, z);
            },
            [&v23, &planBiomeQueries](int x, int z) {
                ++planBiomeQueries;
                return v23.getBiomeAtWorld(x, z);
            });
        const int additionalSurfaceQueries =
            planSurfaceQueries - entranceSurfaceQueries;
        const int additionalBiomeQueries =
            planBiomeQueries - entranceBiomeQueries;
        check("ADVENTURE-UNDERGROUND/bounded-certified-plan-queries-" +
                  std::to_string(fixture.seed),
              additionalSurfaceQueries > 0 &&
                  additionalSurfaceQueries <= 1024 &&
                  additionalBiomeQueries >= 0 &&
                  additionalBiomeQueries <= 256,
              "surface=" + std::to_string(additionalSurfaceQueries) +
                  " biome=" + std::to_string(additionalBiomeQueries));
        if (!plan.valid) {
            continue;
        }

        const auto locations = adventureUndergroundLocations(plan);
        ClassicOverWorldGenerator legacyRegionGenerator(
            fixture.seed, LandmarkWorkshopTerrainGenerationVersion);
        auto legacyRegion = generateAdventureUndergroundChunks(
            sampleWorld, legacyRegionGenerator, locations, false);
        const std::uint64_t legacyRegionHash =
            adventureUndergroundRegionHash(legacyRegion);
        std::cout << "[UNDERGROUND_V22_REGION] seed=" << fixture.seed
                  << " chunks=" << locations.size() << " hash="
                  << legacyRegionHash << '\n';
        check("ADVENTURE-UNDERGROUND/v22-frozen-region-" +
                  std::to_string(fixture.seed),
              legacyRegionHash == fixture.v22RegionHash,
              "hash=" + std::to_string(legacyRegionHash));
        auto forward = generateAdventureUndergroundChunks(
            sampleWorld, v23, locations, false);
        const AdventureUndergroundProjectionEquivalence equivalence =
            compareAdventureUndergroundProductionWithPublicProjection(
                sampleWorld, fixture.seed, forward);
        const bool equivalentProjection =
            equivalence.equivalent &&
            equivalence.comparedChunks == forward.size() &&
            equivalence.changedChunks > 0 &&
            equivalence.publicWrites > 0;
        check("ADVENTURE-UNDERGROUND/production-public-equivalence-" +
                  std::to_string(fixture.seed),
              equivalentProjection,
              "chunks=" + std::to_string(equivalence.comparedChunks) +
                  " changed=" +
                  std::to_string(equivalence.changedChunks) +
                  " writes=" +
                  std::to_string(equivalence.publicWrites) +
                  " mismatch=" +
                  std::to_string(equivalence.firstMismatch.x) + "," +
                  std::to_string(equivalence.firstMismatch.y) +
                  " hashes=" +
                  std::to_string(equivalence.productionHash) + "/" +
                  std::to_string(equivalence.publicHash) +
                  " entities=" +
                  std::to_string(equivalence.entityMismatch ? 1 : 0));
        if (equivalentProjection) {
            equivalenceLayouts[static_cast<std::size_t>(plan.layout)] = true;
            equivalenceDirections |= adventureUndergroundDirectionBit(
                plan.entrance.directionX, plan.entrance.directionZ);
            equivalenceDirections |= adventureUndergroundDirectionBit(
                plan.riftDirectionX, plan.riftDirectionZ);
            equivalenceNegativeCoordinates =
                equivalenceNegativeCoordinates ||
                std::any_of(locations.begin(), locations.end(),
                            [](const glm::ivec2 &location) {
                                return location.x < 0 || location.y < 0;
                            });
            equivalenceCrossesChunkBoundary =
                equivalenceCrossesChunkBoundary ||
                equivalence.changedChunks > 1;
        }
        ClassicOverWorldGenerator repeated(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        auto reverse = generateAdventureUndergroundChunks(
            sampleWorld, repeated, locations, true);
        bool sameProjection = forward.size() == reverse.size();
        for (const auto &entry : forward) {
            const auto found = std::find_if(
                reverse.begin(), reverse.end(), [&entry](const auto &other) {
                    return other.location == entry.location;
                });
            sameProjection = sameProjection && found != reverse.end() &&
                TerrainSurvey::blockHash(*entry.chunk) ==
                    TerrainSurvey::blockHash(*found->chunk) &&
                adventureUndergroundEntitySignature(*entry.chunk) ==
                    adventureUndergroundEntitySignature(*found->chunk);
        }
        check("ADVENTURE-UNDERGROUND/reverse-chunk-order-" +
                  std::to_string(fixture.seed), sameProjection,
              "chunks=" + std::to_string(locations.size()));

        const glm::ivec2 parallelLocation = locations.front();
        Chunk parallelLeftChunk(sampleWorld, parallelLocation, false);
        Chunk parallelRightChunk(sampleWorld, parallelLocation, false);
        ClassicOverWorldGenerator parallelLeftGenerator(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        ClassicOverWorldGenerator parallelRightGenerator(
            fixture.seed, AdventureUndergroundTerrainGenerationVersion);
        std::thread parallelLeft([&]() {
            parallelLeftGenerator.generateTerrainFor(parallelLeftChunk);
        });
        std::thread parallelRight([&]() {
            parallelRightGenerator.generateTerrainFor(parallelRightChunk);
        });
        parallelLeft.join();
        parallelRight.join();
        const auto expectedParallel = std::find_if(
            forward.begin(), forward.end(),
            [&parallelLocation](const auto &entry) {
                return entry.location == parallelLocation;
            });
        const bool parallelProjection =
            expectedParallel != forward.end() &&
            TerrainSurvey::blockHash(parallelLeftChunk) ==
                TerrainSurvey::blockHash(parallelRightChunk) &&
            TerrainSurvey::blockHash(parallelLeftChunk) ==
                TerrainSurvey::blockHash(*expectedParallel->chunk) &&
            adventureUndergroundEntitySignature(parallelLeftChunk) ==
                adventureUndergroundEntitySignature(parallelRightChunk) &&
            adventureUndergroundEntitySignature(parallelLeftChunk) ==
                adventureUndergroundEntitySignature(
                    *expectedParallel->chunk);
        check("ADVENTURE-UNDERGROUND/parallel-independent-generator-" +
                  std::to_string(fixture.seed),
              parallelProjection,
              "chunk=" + std::to_string(parallelLocation.x) + "," +
                  std::to_string(parallelLocation.y));

        int airWrites = 0;
        int waterWrites = 0;
        int structureWrites = 0;
        int removedLegacyWater = 0;
        int addedEntities = 0;
        bool coveredWrites = true;
        for (const auto &entry : forward) {
            const auto old = std::find_if(
                legacyRegion.begin(), legacyRegion.end(),
                [&entry](const auto &other) {
                    return other.location == entry.location;
                });
            if (old == legacyRegion.end()) {
                continue;
            }
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    for (int y = 0; y < 256; ++y) {
                        const BlockId before = static_cast<BlockId>(
                            old->chunk->getBlock(x, y, z).id);
                        const BlockId after = static_cast<BlockId>(
                            entry.chunk->getBlock(x, y, z).id);
                        if (before == after) {
                            continue;
                        }
                        const int worldX = entry.location.x *
                            CHUNK_SIZE + x;
                        const int worldZ = entry.location.y *
                            CHUNK_SIZE + z;
                        coveredWrites = coveredWrites && y >= 8 &&
                            y <= v23.getSurfaceHeightAtWorld(
                                worldX, worldZ) - 5;
                        airWrites += after == BlockId::Air;
                        waterWrites += after == BlockId::Water;
                        structureWrites += after != BlockId::Air &&
                            after != BlockId::Water;
                        removedLegacyWater += before == BlockId::Water &&
                            after != BlockId::Water;
                    }
                }
            }
            addedEntities += static_cast<int>(
                entry.chunk->getBlockEntities().size()) -
                static_cast<int>(old->chunk->getBlockEntities().size());
        }
        const int totalWrites = airWrites + waterWrites + structureWrites;
        check("ADVENTURE-UNDERGROUND/bounded-unique-writes-" +
                  std::to_string(fixture.seed),
              airWrites <=
                      CaveGenerator::AdventureMaximumAirWritesPerPlan &&
                  waterWrites <=
                      CaveGenerator::AdventureMaximumWaterPerPlan &&
                  structureWrites <=
                      CaveGenerator::AdventureMaximumStructureBlocksPerPlan &&
                  totalWrites <=
                      CaveGenerator::AdventureMaximumWritesPerPlan &&
                  addedEntities >= 0 && addedEntities <= 1,
              "air/water/structure/entities=" +
                  std::to_string(airWrites) + '/' +
                  std::to_string(waterWrites) + '/' +
                  std::to_string(structureWrites) + '/' +
                  std::to_string(addedEntities));
        check("ADVENTURE-UNDERGROUND/preserves-existing-water-" +
                  std::to_string(fixture.seed),
              removedLegacyWater == 0,
              "removed=" + std::to_string(removedLegacyWater));
        check("ADVENTURE-UNDERGROUND/all-writes-covered-" +
                  std::to_string(fixture.seed), coveredWrites);

        int chamberAir = 0;
        for (int along = -CaveGenerator::AdventureChamberAlongRadius;
             along <= CaveGenerator::AdventureChamberAlongRadius; ++along) {
            for (int lateral =
                     -CaveGenerator::AdventureChamberPerpendicularRadius;
                 lateral <=
                     CaveGenerator::AdventureChamberPerpendicularRadius;
                 ++lateral) {
                const int worldX = plan.chamberX +
                    plan.entrance.directionX * along -
                    plan.entrance.directionZ * lateral;
                const int worldZ = plan.chamberZ +
                    plan.entrance.directionZ * along +
                    plan.entrance.directionX * lateral;
                for (int y = plan.chamberAirY - 2;
                     y <= plan.chamberAirY + 8; ++y) {
                    chamberAir += adventureUndergroundBlock(
                        forward, worldX, y, worldZ) == BlockId::Air;
                }
            }
        }
        check("ADVENTURE-UNDERGROUND/large-chamber-" +
                  std::to_string(fixture.seed),
              chamberAir >= 800,
              "air=" + std::to_string(chamberAir));

        const int riftPerpendicularX = -plan.riftDirectionZ;
        const int riftPerpendicularZ = plan.riftDirectionX;
        const int riftDrop = plan.chamberAirY - plan.riftEndAirY;
        bool rift = true;
        bool dryReturn = true;
        bool completeRoute = true;
        int previousRouteY = plan.entrance.anchorY;
        const int entrancePerpendicularX =
            -plan.entrance.directionZ;
        const int entrancePerpendicularZ =
            plan.entrance.directionX;
        const auto checkFooting = [&](int worldX, int airY, int worldZ) {
            const BlockId floor = adventureUndergroundBlock(
                forward, worldX, airY - 1, worldZ);
            bool passable = floor != BlockId::Air &&
                floor != BlockId::Water;
            for (int y = airY; y <= airY + 2; ++y) {
                passable = passable && adventureUndergroundBlock(
                    forward, worldX, y, worldZ) == BlockId::Air;
            }
            return passable;
        };
        for (int step = 0; step <= CaveGenerator::EntranceTunnelLength;
             ++step) {
            const int airY = plan.entrance.anchorY - step * 3 / 4;
            completeRoute = completeRoute &&
                std::abs(airY - previousRouteY) <= 1;
            previousRouteY = airY;
            for (int lateral = -1; lateral <= 1; ++lateral) {
                completeRoute = completeRoute && checkFooting(
                    plan.entrance.anchorX +
                        plan.entrance.directionX * step +
                        entrancePerpendicularX * lateral,
                    airY,
                    plan.entrance.anchorZ +
                        plan.entrance.directionZ * step +
                        entrancePerpendicularZ * lateral);
            }
        }
        for (int step = CaveGenerator::EntranceTunnelLength;
             step <= 29; ++step) {
            completeRoute = completeRoute &&
                std::abs(plan.chamberAirY - previousRouteY) <= 1;
            previousRouteY = plan.chamberAirY;
            for (int lateral = -1; lateral <= 1; ++lateral) {
                completeRoute = completeRoute && checkFooting(
                    plan.entrance.anchorX +
                        plan.entrance.directionX * step +
                        entrancePerpendicularX * lateral,
                    plan.chamberAirY,
                    plan.entrance.anchorZ +
                        plan.entrance.directionZ * step +
                        entrancePerpendicularZ * lateral);
            }
        }
        for (int step = 0; step < 6; ++step) {
            for (int lateral = -1; lateral <= 1; ++lateral) {
                completeRoute = completeRoute && checkFooting(
                    plan.chamberX + plan.riftDirectionX * step +
                        riftPerpendicularX * lateral,
                    plan.chamberAirY,
                    plan.chamberZ + plan.riftDirectionZ * step +
                        riftPerpendicularZ * lateral);
            }
        }
        int previousAirY = plan.chamberAirY;
        for (int step = 0; step < CaveGenerator::AdventureRiftLength;
             ++step) {
            const int airY = plan.chamberAirY -
                riftDrop * step /
                    (CaveGenerator::AdventureRiftLength - 1);
            const int distance = 6 + step;
            rift &= std::abs(airY - previousAirY) <= 1;
            previousAirY = airY;
            for (int lateral = -1; lateral <= 1; ++lateral) {
                const int worldX = plan.chamberX +
                    plan.riftDirectionX * distance +
                    riftPerpendicularX * lateral;
                const int worldZ = plan.chamberZ +
                    plan.riftDirectionZ * distance +
                    riftPerpendicularZ * lateral;
                const BlockId floor = adventureUndergroundBlock(
                    forward, worldX, airY - 1, worldZ);
                rift = rift && floor != BlockId::Air &&
                    floor != BlockId::Water &&
                    adventureUndergroundBlock(
                        forward, worldX, airY + 8, worldZ) == BlockId::Air;
                for (int y = airY; y <= airY + 2; ++y) {
                    const BlockId block = adventureUndergroundBlock(
                        forward, worldX, y, worldZ);
                    dryReturn = dryReturn && block == BlockId::Air;
                }
                completeRoute = completeRoute && checkFooting(
                    worldX, airY, worldZ);
            }
        }
        check("ADVENTURE-UNDERGROUND/high-narrow-rift-" +
                  std::to_string(fixture.seed), rift);
        check("ADVENTURE-UNDERGROUND/three-wide-dry-return-" +
                  std::to_string(fixture.seed), dryReturn);
        check("ADVENTURE-UNDERGROUND/complete-entry-return-route-" +
                  std::to_string(fixture.seed), completeRoute);
        checkAdventureUndergroundPlayerReturn(fixture.seed, plan);

        const int poolX = plan.chamberX + plan.poolDirectionX * 5;
        const int poolZ = plan.chamberZ + plan.poolDirectionZ * 5;
        int waterBlocks = 0;
        bool poolSealed = true;
        bool poolBankClosed = true;
        for (int x = -CaveGenerator::AdventurePoolRadius;
             x <= CaveGenerator::AdventurePoolRadius; ++x) {
            for (int z = -CaveGenerator::AdventurePoolRadius;
                 z <= CaveGenerator::AdventurePoolRadius; ++z) {
                waterBlocks += adventureUndergroundBlock(
                    forward, poolX + x, plan.chamberAirY - 2,
                    poolZ + z) == BlockId::Water;
                if (x * x + z * z <=
                        CaveGenerator::AdventurePoolRadius *
                            CaveGenerator::AdventurePoolRadius) {
                    poolSealed = poolSealed &&
                        adventureUndergroundBlock(
                            forward, poolX + x,
                            plan.chamberAirY - 3,
                            poolZ + z) != BlockId::Air &&
                        adventureUndergroundBlock(
                            forward, poolX + x,
                            plan.chamberAirY - 1,
                            poolZ + z) == BlockId::Air;
                }
            }
        }
        for (int x = -CaveGenerator::AdventurePoolRadius - 1;
             x <= CaveGenerator::AdventurePoolRadius + 1; ++x) {
            for (int z = -CaveGenerator::AdventurePoolRadius - 1;
                 z <= CaveGenerator::AdventurePoolRadius + 1; ++z) {
                const int radiusSquared = x * x + z * z;
                if (radiusSquared <=
                        CaveGenerator::AdventurePoolRadius *
                            CaveGenerator::AdventurePoolRadius ||
                    radiusSquared >
                        (CaveGenerator::AdventurePoolRadius + 1) *
                            (CaveGenerator::AdventurePoolRadius + 1)) {
                    continue;
                }
                const BlockId bank = adventureUndergroundBlock(
                    forward, poolX + x, plan.chamberAirY - 2,
                    poolZ + z);
                poolBankClosed = poolBankClosed &&
                    bank != BlockId::Air && bank != BlockId::Water;
            }
        }
        check("ADVENTURE-UNDERGROUND/bounded-shallow-water-" +
                  std::to_string(fixture.seed),
              waterBlocks >= 20 &&
                  waterBlocks <=
                      CaveGenerator::AdventureMaximumWaterPerPlan,
              "water=" + std::to_string(waterBlocks));
        check("ADVENTURE-UNDERGROUND/pool-below-route-and-sealed-" +
                  std::to_string(fixture.seed),
              poolSealed && poolBankClosed && waterBlocks == 29);

        int oak = 0, coal = 0, iron = 0, chest = 0;
        int moss = 0, silt = 0, gravel = 0, stone = 0;
        bool exposedResources = true;
        int floorEdges = 0;
        int sameFloorEdges = 0;
        for (int along = -CaveGenerator::AdventureDestinationRadius;
             along <= CaveGenerator::AdventureDestinationRadius; ++along) {
            for (int lateral = -CaveGenerator::AdventureDestinationRadius;
                 lateral <= CaveGenerator::AdventureDestinationRadius;
                 ++lateral) {
                for (int y = plan.riftEndAirY - 1;
                     y <= plan.riftEndAirY + 5; ++y) {
                    const BlockId block = adventureUndergroundBlock(
                        forward,
                        plan.destinationX + plan.riftDirectionX * along +
                            riftPerpendicularX * lateral,
                        y,
                        plan.destinationZ + plan.riftDirectionZ * along +
                            riftPerpendicularZ * lateral);
                    oak += block == BlockId::OakBark ||
                        block == BlockId::OakPlank;
                    coal += block == BlockId::CoalOre;
                    iron += block == BlockId::IronOre;
                    chest += block == BlockId::Chest;
                    moss += block == BlockId::MossStone;
                    silt += block == BlockId::Silt;
                    gravel += block == BlockId::Gravel;
                    stone += block == BlockId::Stone;
                }
            }
        }
        if (plan.layout ==
                CaveGenerator::AdventureUndergroundLayout::MossCellar) {
            int resourceIndex = 0;
            for (int level = 1; level <= 2; ++level) {
                for (int along = -2; along <= 2; ++along) {
                    if (resourceIndex >= 9) {
                        continue;
                    }
                    const BlockId expected = resourceIndex++ < 6
                        ? BlockId::CoalOre : BlockId::IronOre;
                    const int worldX = plan.destinationX +
                        plan.riftDirectionX * along +
                        riftPerpendicularX * 5;
                    const int worldZ = plan.destinationZ +
                        plan.riftDirectionZ * along +
                        riftPerpendicularZ * 5;
                    const int y = plan.riftEndAirY + level;
                    const BlockId actual = adventureUndergroundBlock(
                        forward, worldX, y, worldZ);
                    bool exposed = false;
                    for (const glm::ivec3 &delta : {
                             glm::ivec3{1,0,0}, {-1,0,0},
                             {0,1,0}, {0,-1,0},
                             {0,0,1}, {0,0,-1}}) {
                        exposed = exposed || adventureUndergroundBlock(
                            forward, worldX + delta.x, y + delta.y,
                            worldZ + delta.z) == BlockId::Air;
                    }
                    exposedResources = exposedResources &&
                        actual == expected && exposed;
                }
            }
            for (int along = -CaveGenerator::AdventureDestinationRadius;
                 along <= CaveGenerator::AdventureDestinationRadius;
                 ++along) {
                for (int lateral =
                         -CaveGenerator::AdventureDestinationRadius;
                     lateral <=
                         CaveGenerator::AdventureDestinationRadius;
                     ++lateral) {
                    if (along * along + lateral * lateral >
                            CaveGenerator::AdventureDestinationRadius *
                                CaveGenerator::AdventureDestinationRadius) {
                        continue;
                    }
                    const BlockId current = adventureUndergroundBlock(
                        forward,
                        plan.destinationX +
                            plan.riftDirectionX * along +
                            riftPerpendicularX * lateral,
                        plan.riftEndAirY - 1,
                        plan.destinationZ +
                            plan.riftDirectionZ * along +
                            riftPerpendicularZ * lateral);
                    for (const glm::ivec2 &next : {
                             glm::ivec2{along + 1, lateral},
                             glm::ivec2{along, lateral + 1}}) {
                        if (next.x * next.x + next.y * next.y >
                                CaveGenerator::AdventureDestinationRadius *
                                    CaveGenerator::AdventureDestinationRadius) {
                            continue;
                        }
                        ++floorEdges;
                        sameFloorEdges += current ==
                            adventureUndergroundBlock(
                                forward,
                                plan.destinationX +
                                    plan.riftDirectionX * next.x +
                                    riftPerpendicularX * next.y,
                                plan.riftEndAirY - 1,
                                plan.destinationZ +
                                    plan.riftDirectionZ * next.x +
                                    riftPerpendicularZ * next.y);
                    }
                }
            }
        }
        const bool destination = plan.layout ==
                CaveGenerator::AdventureUndergroundLayout::MinerCache
            ? oak >= 30 && chest == 1
            : coal >= 6 && iron >= 3 &&
                chest == 0 &&
                moss > 0 && silt > 0 && gravel > 0 && stone > 0 &&
                exposedResources && floorEdges > 0 &&
                sameFloorEdges * 100 >= floorEdges * 35;
        check("ADVENTURE-UNDERGROUND/destination-layout-" +
                  std::to_string(fixture.seed), destination,
              "oak=" + std::to_string(oak) + " coal=" +
                  std::to_string(coal) + " iron=" +
                  std::to_string(iron) + " chest=" +
                  std::to_string(chest) + " materials=" +
                  std::to_string(moss) + "/" + std::to_string(silt) +
                  "/" + std::to_string(gravel) + "/" +
                  std::to_string(stone) + " exposed=" +
                  std::to_string(exposedResources ? 1 : 0) +
                  " same-edges=" + std::to_string(sameFloorEdges) +
                  "/" + std::to_string(floorEdges));
        int outcropDirectionX = -plan.poolDirectionX;
        int outcropDirectionZ = -plan.poolDirectionZ;
        if (outcropDirectionX == plan.riftDirectionX &&
            outcropDirectionZ == plan.riftDirectionZ) {
            outcropDirectionX = plan.entrance.directionX;
            outcropDirectionZ = plan.entrance.directionZ;
        }
        const bool alongChamber =
            std::abs(outcropDirectionX) ==
                std::abs(plan.entrance.directionX) &&
            std::abs(outcropDirectionZ) ==
                std::abs(plan.entrance.directionZ);
        const int outcropDistance = alongChamber
            ? CaveGenerator::AdventureChamberAlongRadius
            : CaveGenerator::AdventureChamberPerpendicularRadius;
        const int outcropX = plan.chamberX +
            outcropDirectionX * outcropDistance;
        const int outcropZ = plan.chamberZ +
            outcropDirectionZ * outcropDistance;
        bool outcropAttached = true;
        for (const auto sample : {
                 std::pair<int, BlockId>{plan.chamberAirY + 3,
                                         BlockId::CoalOre},
                 {plan.chamberAirY + 2, BlockId::IronOre}}) {
            const BlockId actual = adventureUndergroundBlock(
                forward, outcropX, sample.first, outcropZ);
            bool touchesAir = false;
            bool touchesWall = false;
            for (const glm::ivec3 &delta : {
                     glm::ivec3{1,0,0}, {-1,0,0},
                     {0,1,0}, {0,-1,0},
                     {0,0,1}, {0,0,-1}}) {
                const BlockId adjacent = adventureUndergroundBlock(
                    forward, outcropX + delta.x,
                    sample.first + delta.y, outcropZ + delta.z);
                touchesAir = touchesAir || adjacent == BlockId::Air;
                touchesWall = touchesWall ||
                    (adjacent != BlockId::Air &&
                     adjacent != BlockId::Water &&
                     adjacent != BlockId::CoalOre &&
                     adjacent != BlockId::IronOre);
            }
            outcropAttached = outcropAttached &&
                actual == sample.second && touchesAir && touchesWall;
        }
        check("ADVENTURE-UNDERGROUND/chamber-ore-outcrop-" +
                  std::to_string(fixture.seed),
              outcropAttached);

        if (plan.layout ==
                CaveGenerator::AdventureUndergroundLayout::MinerCache) {
            const int chestX = plan.destinationX +
                plan.riftDirectionX * 2 + riftPerpendicularX * 3;
            const int chestZ = plan.destinationZ +
                plan.riftDirectionZ * 2 + riftPerpendicularZ * 3;
            const Chunk *owningChunk = adventureUndergroundChunkAt(
                forward, chestX, chestZ);
            const BlockEntityRecord *entity = owningChunk == nullptr
                ? nullptr
                : owningChunk->findBlockEntity({
                      WorldCoordinates::floorMod(chestX, CHUNK_SIZE),
                      plan.riftEndAirY,
                      WorldCoordinates::floorMod(chestZ, CHUNK_SIZE)});
            ContainerInventory inventory(ChestContainer::SlotCount);
            const bool loot = entity != nullptr &&
                entity->type == ChestContainer::BlockEntityType &&
                ContainerInventory::deserialize(
                    entity->payload, inventory) &&
                inventory.count(Material::ID::Torch) == 4 &&
                inventory.count(Material::ID::OakPlank) == 4 &&
                inventory.count(Material::ID::Bread) == 2;
            check("ADVENTURE-UNDERGROUND/legal-cache-loot-" +
                      std::to_string(fixture.seed), loot);
        }
    }
    check("ADVENTURE-UNDERGROUND/two-destination-layouts",
          layouts[0] && layouts[1]);
    const int equivalenceDirectionCount =
        static_cast<int>((equivalenceDirections & 1) != 0) +
        static_cast<int>((equivalenceDirections & 2) != 0) +
        static_cast<int>((equivalenceDirections & 4) != 0) +
        static_cast<int>((equivalenceDirections & 8) != 0);
    check("ADVENTURE-UNDERGROUND/production-public-equivalence-coverage",
          equivalenceLayouts[0] && equivalenceLayouts[1] &&
              equivalenceDirectionCount >= 3 &&
              equivalenceNegativeCoordinates &&
              equivalenceCrossesChunkBoundary,
          "layouts=" +
              std::to_string(equivalenceLayouts[0] ? 1 : 0) + "/" +
              std::to_string(equivalenceLayouts[1] ? 1 : 0) +
              " directions=" +
              std::to_string(equivalenceDirectionCount) +
              " negative=" +
              std::to_string(equivalenceNegativeCoordinates ? 1 : 0) +
              " cross-chunk=" +
              std::to_string(equivalenceCrossesChunkBoundary ? 1 : 0));

    {
        constexpr int CachePressureChunks = 65;
        static_assert(CachePressureChunks * 4 >
                          CaveGenerator::AdventurePlanCacheCapacity,
                      "cache pressure must exceed plan capacity");
        ClassicOverWorldGenerator cacheGenerator(
            42, AdventureUndergroundTerrainGenerationVersion);
        std::array<glm::ivec2, CachePressureChunks> cacheLocations{};
        std::array<std::uint64_t, CachePressureChunks> cacheHashes{};
        std::array<std::string, CachePressureChunks> cacheEntities{};
        for (int index = 0; index < CachePressureChunks; ++index) {
            cacheLocations[static_cast<std::size_t>(index)] = {
                600 + index * 24, -800 - index * 24};
            Chunk chunk(sampleWorld,
                        cacheLocations[static_cast<std::size_t>(index)],
                        false);
            cacheGenerator.generateTerrainFor(chunk);
            cacheHashes[static_cast<std::size_t>(index)] =
                TerrainSurvey::blockHash(chunk);
            cacheEntities[static_cast<std::size_t>(index)] =
                adventureUndergroundEntitySignature(chunk);
        }
        bool stableAfterCachePressure = true;
        for (int index = 0; index < CachePressureChunks; ++index) {
            Chunk chunk(sampleWorld,
                        cacheLocations[static_cast<std::size_t>(index)],
                        false);
            cacheGenerator.generateTerrainFor(chunk);
            stableAfterCachePressure = stableAfterCachePressure &&
                TerrainSurvey::blockHash(chunk) ==
                    cacheHashes[static_cast<std::size_t>(index)] &&
                adventureUndergroundEntitySignature(chunk) ==
                    cacheEntities[static_cast<std::size_t>(index)];
        }
        check("ADVENTURE-UNDERGROUND/cache-capacity-pressure-deterministic",
              stableAfterCachePressure,
              "chunks=" + std::to_string(CachePressureChunks) +
                  " minimum-cells=" +
                  std::to_string(CachePressureChunks * 4) +
                  " capacity=" + std::to_string(
                      CaveGenerator::AdventurePlanCacheCapacity));

        bool pressuredEquivalent = plans[0].valid;
        AdventureUndergroundProjectionEquivalence pressuredEquivalence;
        if (plans[0].valid) {
            const auto pressuredLocations =
                adventureUndergroundLocations(plans[0]);
            const auto pressuredProduction =
                generateAdventureUndergroundChunks(
                    sampleWorld, cacheGenerator,
                    pressuredLocations, true);
            pressuredEquivalence =
                compareAdventureUndergroundProductionWithPublicProjection(
                    sampleWorld, fixtures[0].seed,
                    pressuredProduction);
            pressuredEquivalent = pressuredEquivalence.equivalent &&
                pressuredEquivalence.comparedChunks ==
                    pressuredProduction.size() &&
                pressuredEquivalence.changedChunks > 0 &&
                pressuredEquivalence.publicWrites > 0;
        }
        check("ADVENTURE-UNDERGROUND/cache-pressure-public-equivalence",
              pressuredEquivalent,
              "chunks=" +
                  std::to_string(
                      pressuredEquivalence.comparedChunks) +
                  " changed=" +
                  std::to_string(
                      pressuredEquivalence.changedChunks) +
                  " writes=" +
                  std::to_string(pressuredEquivalence.publicWrites) +
                  " mismatch=" +
                  std::to_string(
                      pressuredEquivalence.firstMismatch.x) + "," +
                  std::to_string(
                      pressuredEquivalence.firstMismatch.y) +
                  " hashes=" +
                  std::to_string(
                      pressuredEquivalence.productionHash) + "/" +
                  std::to_string(pressuredEquivalence.publicHash) +
                  " entities=" +
                  std::to_string(
                      pressuredEquivalence.entityMismatch ? 1 : 0));
    }

    const auto mine = std::find_if(
        plans.begin(), plans.end(), [](const auto &plan) {
            return plan.valid && plan.layout ==
                CaveGenerator::AdventureUndergroundLayout::MinerCache;
        });
    bool lifecycle = mine != plans.end();
    if (lifecycle) {
        const std::size_t index = static_cast<std::size_t>(
            std::distance(plans.begin(), mine));
        const int riftPerpendicularX = -mine->riftDirectionZ;
        const int riftPerpendicularZ = mine->riftDirectionX;
        const glm::ivec3 chestPosition{
            mine->destinationX + mine->riftDirectionX * 2 +
                riftPerpendicularX * 3,
            mine->riftEndAirY,
            mine->destinationZ + mine->riftDirectionZ * 2 +
                riftPerpendicularZ * 3};
        const auto hasExpectedLoot = [](const auto &entity) {
            if (!entity || entity->type !=
                    ChestContainer::BlockEntityType) {
                return false;
            }
            ContainerInventory inventory(ChestContainer::SlotCount);
            return ContainerInventory::deserialize(
                       entity->payload, inventory) &&
                inventory.count(Material::ID::Torch) == 4 &&
                inventory.count(Material::ID::OakPlank) == 4 &&
                inventory.count(Material::ID::Bread) == 2;
        };
        const auto directory = freshSaveDirectory(
            "adventure_underground_v23_lifecycle");
        lifecycle = initializeTerrainIdentity(
            directory, "adventure-underground-v23-lifecycle",
            AdventureUndergroundTerrainGenerationVersion,
            fixtures[index].seed);
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            const int chunkX = WorldCoordinates::floorDiv(
                chestPosition.x, CHUNK_SIZE);
            const int chunkZ = WorldCoordinates::floorDiv(
                chestPosition.z, CHUNK_SIZE);
            world.getChunkManager().loadChunk(chunkX, chunkZ);
            const auto entity = world.getBlockEntity(chestPosition);
            const int initialTorch = player.getInventoryCount(
                Material::ID::Torch);
            const int initialPlank = player.getInventoryCount(
                Material::ID::OakPlank);
            const int initialBread = player.getInventoryCount(
                Material::ID::Bread);
            bool transferred = lifecycle && hasExpectedLoot(entity) &&
                ChestContainer::open(world, player, chestPosition);
            for (int slot = 0;
                 slot < ChestContainer::SlotCount && transferred; ++slot) {
                const auto view = ChestContainer::view(world, player);
                if (!view) {
                    transferred = false;
                    break;
                }
                const InventorySlotState item =
                    view->inventory.getSlot(slot);
                if (item.amount > 0) {
                    transferred = ChestContainer::transferToPlayer(
                        world, player, slot, item.amount);
                }
            }
            const auto emptied = ChestContainer::view(world, player);
            bool empty = emptied.has_value();
            for (int slot = 0;
                 slot < ChestContainer::SlotCount && empty; ++slot) {
                empty = emptied->inventory.getSlot(slot).amount == 0;
            }
            const bool playerReceivedLoot =
                player.getInventoryCount(Material::ID::Torch) ==
                    initialTorch + 4 &&
                player.getInventoryCount(Material::ID::OakPlank) ==
                    initialPlank + 4 &&
                player.getInventoryCount(Material::ID::Bread) ==
                    initialBread + 2;
            ChestContainer::close(player);
            const bool savedEmpty = world.save();
            const bool unloaded =
                world.getChunkManager().unloadChunk(chunkX, chunkZ);
            const bool absentAfterUnload =
                !world.getChunkManager().chunkLoadedAt(chunkX, chunkZ);
            world.getChunkManager().loadChunk(chunkX, chunkZ);
            const auto reloadedEntity =
                world.getBlockEntity(chestPosition);
            ContainerInventory reloadedInventory(
                ChestContainer::SlotCount);
            bool reloadedEmpty = reloadedEntity &&
                reloadedEntity->type == ChestContainer::BlockEntityType &&
                ContainerInventory::deserialize(
                    reloadedEntity->payload, reloadedInventory);
            for (int slot = 0;
                 slot < ChestContainer::SlotCount && reloadedEmpty; ++slot) {
                reloadedEmpty =
                    reloadedInventory.getSlot(slot).amount == 0;
            }
            lifecycle = lifecycle && transferred && empty &&
                playerReceivedLoot && savedEmpty && unloaded &&
                absentAfterUnload && reloadedEmpty;
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(chestPosition.x, CHUNK_SIZE),
                WorldCoordinates::floorDiv(chestPosition.z, CHUNK_SIZE));
            const auto entity = world.getBlockEntity(chestPosition);
            ContainerInventory inventory(ChestContainer::SlotCount);
            bool empty = entity &&
                entity->type == ChestContainer::BlockEntityType &&
                ContainerInventory::deserialize(
                    entity->payload, inventory);
            for (int slot = 0;
                 slot < ChestContainer::SlotCount && empty; ++slot) {
                empty = inventory.getSlot(slot).amount == 0;
            }
            lifecycle = lifecycle && empty;
            world.setBlock(chestPosition.x, chestPosition.y,
                           chestPosition.z, BlockId::Air);
            lifecycle = lifecycle &&
                !world.getBlockEntity(chestPosition) && world.save();
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(chestPosition.x, CHUNK_SIZE),
                WorldCoordinates::floorDiv(chestPosition.z, CHUNK_SIZE));
            lifecycle = lifecycle &&
                world.getBlock(chestPosition.x, chestPosition.y,
                               chestPosition.z) == BlockId::Air &&
                !world.getBlockEntity(chestPosition);
        }
    }
    check("ADVENTURE-UNDERGROUND/cache-transfer-unload-reopen-no-respawn",
          lifecycle);

    const auto entrance = std::find_if(
        plans.begin(), plans.end(), [](const auto &plan) {
            return plan.valid && plan.entrance.valid;
        });
    bool entranceMarker = entrance != plans.end();
    std::uint32_t entranceMarkerId = 0;
    int entranceObservedX = 0;
    int entranceObservedZ = 0;
    int entranceMarkerX = 0;
    int entranceMarkerZ = 0;
    std::string entranceDirectory;
    if (entranceMarker) {
        const std::size_t index = static_cast<std::size_t>(
            std::distance(plans.begin(), entrance));
        entranceObservedX = WorldCoordinates::floorDiv(
            entrance->entrance.anchorX, ExplorationAtlas::MetresPerCell) *
            ExplorationAtlas::MetresPerCell;
        entranceObservedZ = WorldCoordinates::floorDiv(
            entrance->entrance.anchorZ, ExplorationAtlas::MetresPerCell) *
            ExplorationAtlas::MetresPerCell;
        entranceMarkerX = entrance->entrance.anchorX;
        entranceMarkerZ = entrance->entrance.anchorZ;
        entranceDirectory = freshSaveDirectory(
            "adventure_underground_v23_entrance_marker");
        entranceMarker = initializeTerrainIdentity(
            entranceDirectory, "adventure-underground-v23-entrance-marker",
            AdventureUndergroundTerrainGenerationVersion,
            fixtures[index].seed);
        {
            Player player;
            World world(camera, config, player, entranceDirectory, false, 0);
            world.getChunkManager().loadChunk(
                WorldCoordinates::floorDiv(entranceObservedX, CHUNK_SIZE),
                WorldCoordinates::floorDiv(entranceObservedZ, CHUNK_SIZE));
            const auto observed = world.observeSurfaceMap(
                {{entranceObservedX, entranceObservedZ}});
            const auto surface = world.exploredSurfaceAt(
                entranceMarkerX, entranceMarkerZ);
            entranceMarker = entranceMarker &&
                observed.size() == 1 && observed.front().known &&
                surface.has_value() &&
                std::abs(entranceObservedX - entranceMarkerX) <
                    ExplorationAtlas::MetresPerCell &&
                std::abs(entranceObservedZ - entranceMarkerZ) <
                    ExplorationAtlas::MetresPerCell &&
                world.createExplorationMarker(
                    entranceMarkerX, entranceMarkerZ, "洞穴入口",
                    ExplorationMarkers::Kind::Note, &entranceMarkerId) ==
                    ExplorationMarkers::Result::Created &&
                world.trackExplorationMarker(entranceMarkerId) ==
                    ExplorationMarkers::Result::Changed &&
                world.save();
        }
        {
            Player player;
            World world(camera, config, player, entranceDirectory, false, 0);
            const auto markers = world.explorationMarkers();
            const auto tracked = world.trackedExplorationMarker();
            entranceMarker = entranceMarker &&
                markers.size() == 1 &&
                markers.front().id == entranceMarkerId &&
                markers.front().worldX == entranceMarkerX &&
                markers.front().worldZ == entranceMarkerZ &&
                markers.front().name == "洞穴入口" &&
                markers.front().kind == ExplorationMarkers::Kind::Note &&
                tracked.has_value() &&
                tracked->id == entranceMarkerId &&
                tracked->worldX == entranceMarkerX &&
                tracked->worldZ == entranceMarkerZ &&
                tracked->name == "洞穴入口" &&
                world.exploredSurfaceAt(
                    entranceMarkerX, entranceMarkerZ).has_value();
        }
    }
    check("ADVENTURE-UNDERGROUND/entrance-marker-save-reopen-tracked",
          entranceMarker);

    const auto defaultDirectory = freshSaveDirectory(
        "adventure_underground_v23_default");
    bool defaultVersion = false;
    {
        Player player;
        World world(camera, config, player, defaultDirectory, false, 0);
        defaultVersion = world.getChunkManager()
            .getTerrainGenerationVersion() ==
                AdventureUndergroundTerrainGenerationVersion &&
            world.save();
    }
    {
        Player player;
        World world(camera, config, player, defaultDirectory, false, 0);
        defaultVersion = defaultVersion && world.getChunkManager()
            .getTerrainGenerationVersion() ==
                AdventureUndergroundTerrainGenerationVersion;
    }
    check("ADVENTURE-UNDERGROUND/default-v23-save-reopen",
          defaultVersion);

    WorldSave defaultSave(defaultDirectory);
    WorldSaveData currentData;
    const bool currentLoaded = defaultSave.load(currentData);
    WorldSaveData futureData = currentData;
    futureData.terrainGenerationVersion =
        CurrentTerrainGenerationVersion + 1;
    WorldSaveData preservedData;
    check("ADVENTURE-UNDERGROUND/future-v24-rejected-without-overwrite",
          currentLoaded &&
              currentData.terrainGenerationVersion ==
                  AdventureUndergroundTerrainGenerationVersion &&
              !defaultSave.save(futureData) &&
              defaultSave.load(preservedData) &&
              preservedData.terrainGenerationVersion ==
                  AdventureUndergroundTerrainGenerationVersion);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}

} // namespace
