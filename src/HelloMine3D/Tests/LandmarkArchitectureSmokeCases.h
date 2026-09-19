#ifndef HELLOMINE3D_LANDMARK_ARCHITECTURE_SMOKE_H
#define HELLOMINE3D_LANDMARK_ARCHITECTURE_SMOKE_H

#include "../World/Generation/Structures/LandmarkArchitecture.h"
#include <map>
#include <queue>
#include <tuple>

namespace {
void caseLandmarkArchitectureV14()
{
    check("E8/version-is-separate-from-save-format",
          LandmarkArchitectureTerrainGenerationVersion == 14 && CurrentTerrainGenerationVersion == 14);
    setEnv("HELLOMINE3D_SEED", "0");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig(); Camera camera(config); Player player;
    World world(camera, config, player, freshSaveDirectory("e8_projection"), false, 0);
    int crossing = 0;
    StructurePlanSnapshot persistencePlan;
    for (int seed : TerrainSurvey::Seeds) {
        ClassicOverWorldGenerator generator(seed, 14), reverse(seed, 14), old(seed, 13);
        std::array<StructurePlanSnapshot, 3> selected{};
        for (int radius = 0; radius <= 32; ++radius) {
            for (int cx = -radius; cx <= radius; ++cx) for (int cz = -radius; cz <= radius; ++cz) {
                if (std::max(std::abs(cx), std::abs(cz)) != radius) { continue; }
                for (int type = 0; type < 3; ++type) if (!selected[type].valid) {
                    selected[type] = generator.getStructurePlanForCell(static_cast<StructureType>(type), cx, cz);
                }
            }
            if (std::all_of(selected.begin(), selected.end(), [](const auto &p) { return p.valid; })) { break; }
        }
        if (seed == 0) { persistencePlan = selected[1]; }
        const auto label = "E8/seed-" + std::to_string(seed);
        check(label + "-all-three-sites", std::all_of(selected.begin(), selected.end(), [](const auto &p) { return p.valid; }));
        bool bounded = true, grounded = true, entry = true, geometry = true, connected = true;
        bool ordered = true, reservation = true, rewards = true, terrain = true;
        for (int x = -128; x <= 128; x += 7) for (int z = -128; z <= 128; z += 7) {
            terrain &= generator.getSurfaceHeightAtWorld(x, z) == old.getSurfaceHeightAtWorld(x, z) &&
                       generator.getBiomeAtWorld(x, z) == old.getBiomeAtWorld(x, z);
        }
        for (const auto &plan : selected) {
            if (!plan.valid) { continue; }
            const auto &f = plan.footprint;
            const int type = static_cast<int>(plan.key.type);
            bounded &= f.width() * f.height() * f.depth() == static_cast<int>(plan.plannedBlockCount) &&
                plan.plannedBlockCount == (type == 0 ? 225u : (type == 1 ? 729u : 792u));
            int low = 256, high = 0;
            for (int x = f.minimumX; x <= f.maximumX; ++x) for (int z = f.minimumZ; z <= f.maximumZ; ++z) {
                const int h = generator.getSurfaceHeightAtWorld(x, z);
                low = std::min(low, h); high = std::max(high, h);
            }
            grounded &= low >= WATER_LEVEL + 4 && high - low <= 2 && plan.anchor.y == high && f.minimumY <= low;
            entry &= std::abs(generator.getSurfaceHeightAtWorld(plan.anchor.x, f.minimumZ - 1) - plan.anchor.y) <= 1;
            std::map<std::pair<int, int>, std::unique_ptr<Chunk>> chunks;
            std::map<std::pair<int, int>, std::uint64_t> hashes;
            for (int cx = WorldCoordinates::floorDiv(f.minimumX, CHUNK_SIZE); cx <= WorldCoordinates::floorDiv(f.maximumX, CHUNK_SIZE); ++cx) {
                for (int cz = WorldCoordinates::floorDiv(f.minimumZ, CHUNK_SIZE); cz <= WorldCoordinates::floorDiv(f.maximumZ, CHUNK_SIZE); ++cz) {
                    auto chunk = std::make_unique<Chunk>(world, glm::ivec2(cx, cz));
                    generator.generateTerrainFor(*chunk);
                    hashes[{cx, cz}] = TerrainSurvey::blockHash(*chunk);
                    chunks[{cx, cz}] = std::move(chunk);
                }
            }
            crossing += chunks.size() > 1;
            for (auto it = hashes.rbegin(); it != hashes.rend(); ++it) {
                Chunk chunk(world, {it->first.first, it->first.second}); reverse.generateTerrainFor(chunk);
                ordered &= TerrainSurvey::blockHash(chunk) == it->second;
            }
            const auto block = [&](int x, int y, int z) {
                const auto &c = chunks.at({WorldCoordinates::floorDiv(x, CHUNK_SIZE), WorldCoordinates::floorDiv(z, CHUNK_SIZE)});
                return static_cast<BlockId>(c->getBlock(WorldCoordinates::floorMod(x, CHUNK_SIZE), y, WorldCoordinates::floorMod(z, CHUNK_SIZE)).id);
            };
            std::array<int, static_cast<int>(BlockId::NUM_TYPES)> counts{};
            using Point = std::tuple<int, int, int>;
            std::set<Point> solids, reached; std::queue<Point> pending;
            for (int x = f.minimumX; x <= f.maximumX; ++x) for (int z = f.minimumZ; z <= f.maximumZ; ++z) {
                for (int y = f.minimumY; y <= f.maximumY; ++y) {
                    const auto id = block(x, y, z); ++counts[static_cast<int>(id)];
                    if (id != BlockId::Air) {
                        const Point point{x,y,z}; solids.insert(point);
                        if (y == f.minimumY) { reached.insert(point); pending.push(point); }
                    }
                }
                for (int y = f.maximumY + 1; y <= plan.anchor.y + 18; ++y) {
                    reservation &= block(x, y, z) != BlockId::OakLeaf && block(x, y, z) != BlockId::OakBark;
                }
            }
            while (!pending.empty()) {
                const auto p = pending.front(); pending.pop();
                for (const glm::ivec3 delta : {glm::ivec3(1,0,0), {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}) {
                    const Point next{std::get<0>(p)+delta.x,std::get<1>(p)+delta.y,std::get<2>(p)+delta.z};
                    if (solids.count(next) && reached.insert(next).second) { pending.push(next); }
                }
            }
            connected &= reached.size() == solids.size();
            geometry &= counts[static_cast<int>(BlockId::Chest)] == (type == 0 ? 0 : 1) &&
                counts[static_cast<int>(BlockId::WaystoneCore)] == (type == 0 ? 1 : 0) &&
                counts[static_cast<int>(BlockId::IronOre)] == (type == 0 ? 1 : (type == 1 ? 4 : 0)) &&
                counts[static_cast<int>(BlockId::CoalOre)] == (type == 0 ? 4 : (type == 1 ? 0 : 1)) &&
                counts[static_cast<int>(BlockId::Glass)] == (type == 0 ? 1 : (type == 1 ? 4 : 0));
            // Follow the actual entrance from the low outer step to the chest's
            // approach square. Two empty blocks are required over each footfall.
            const int endZ = type == 0 ? f.minimumZ : (type == 1 ? plan.anchor.z - 1 : plan.anchor.z);
            int previous = plan.anchor.y;
            for (int z = f.minimumZ; z <= endZ; ++z) {
                const int floor = plan.anchor.y + (z == f.minimumZ ? 0 : 1);
                entry &= block(plan.anchor.x, floor, z) != BlockId::Air && std::abs(floor-previous) <= 1 &&
                    block(plan.anchor.x, floor+1, z) == BlockId::Air && block(plan.anchor.x, floor+2, z) == BlockId::Air;
                previous = floor;
            }
            auto legacyPlan = plan; legacyPlan.key.terrainGenerationVersion = 5;
            rewards &= structureLootForPlan(plan, 1).entries == structureLootForPlan(legacyPlan, 1).entries;
            if (plan.hasChest) {
                const auto &c = chunks.at({WorldCoordinates::floorDiv(plan.chestPosition.x,CHUNK_SIZE),WorldCoordinates::floorDiv(plan.chestPosition.z,CHUNK_SIZE)});
                const auto records = c->getBlockEntities();
                ContainerInventory contents(ChestContainer::SlotCount);
                rewards &= records.size() == 1 &&
                    records.front().type == ChestContainer::BlockEntityType &&
                    ContainerInventory::deserialize(records.front().payload, contents) &&
                    inventoryMatchesLoot(contents, structureLootForPlan(plan,1)) &&
                    block(plan.chestPosition.x,plan.chestPosition.y,plan.chestPosition.z) == BlockId::Chest;
            }
        }
        check(label+"-v13-terrain-preserved",terrain);
        check(label+"-bounded-grounded-footprints",bounded && grounded);
        check(label+"-connected-solids-and-reward-counts",connected && geometry);
        check(label+"-two-block-entry-clearance",entry);
        check(label+"-reverse-projection-and-tree-clearance",ordered && reservation);
        check(label+"-initial-chest-rewards",rewards);
    }
    check("E8/cross-chunk-sites-covered",crossing >= 8, "sites="+std::to_string(crossing));
    ClassicOverWorldGenerator generator(42,14);
    bool bounded = false;
    try { generator.getStructurePlansForChunk(0,0,10); } catch (const std::out_of_range &) { bounded = true; }
    check("E8/halo-rejects-unbounded-query",bounded);
    for (int type = 0; type < 3; ++type) {
        int samples = 0;
        const auto kind = static_cast<StructureType>(type);
        const DeterministicStructurePlanner cliff(42, 14,
            [&samples](int x, int z) {
                ++samples;
                return ((x + z) & 1) ? 80 : 120;
            },
            [type](int, int) {
                return type == 1 ? TerrainBiome::LightForest : TerrainBiome::Grassland;
            });
        bool exercised = false;
        for (int x = 0; x < 16 && !exercised; ++x) {
            if (cliff.selectedStructureTypeForCell(x, 0) != kind) { continue; }
            const auto plan = cliff.planForCell(kind, x, 0);
            exercised = !plan.valid && samples > 0;
        }
        // Eight centre samples plus two footprint columns per rejected
        // candidate suffice to prove that this checkerboard is too steep.
        check("E8/cliff-rejection-bounds-height-work-" + std::to_string(type),
              exercised && samples <= 24, "samples=" + std::to_string(samples));
    }
    const auto directory = freshSaveDirectory("e8_new_world_reopen");
    bool persisted = false; std::uint64_t hash = 0;
    {
        Player owner; World created(camera,config,owner,directory,false,0);
        auto &chunks = created.getChunkManager(); chunks.loadChunk(-1,-1);
        created.setBlock(-2,190,-2,BlockId::OakPlank);
        hash = TerrainSurvey::blockHash(chunks.getChunk(-1,-1));
        persisted = chunks.getTerrainGenerationVersion() == 14 && created.save();
    }
    {
        Player owner; World reopened(camera,config,owner,directory,false,0);
        reopened.getChunkManager().loadChunk(-1,-1);
        persisted &= reopened.getChunkManager().getTerrainGenerationVersion() == 14 &&
            reopened.getBlock(-2,190,-2) == BlockId::OakPlank &&
            hash == TerrainSurvey::blockHash(reopened.getChunkManager().getChunk(-1,-1));
    }
    check("E8/default-v14-save-edit-reopen",persisted);
    const auto chestDirectory = freshSaveDirectory("e8_loot_persistence");
    bool chestSaved = persistencePlan.valid && initializeTerrainIdentity(
        chestDirectory, "e8-loot-v14", 14, 0);
    const glm::ivec3 damage{persistencePlan.anchor.x - 2, persistencePlan.anchor.y + 3,
                            persistencePlan.anchor.z - 3};
    setEnv("HELLOMINE3D_PLAYER_POSITION", vecToString(glm::vec3(
        persistencePlan.chestPosition.x, persistencePlan.chestPosition.y + 2,
        persistencePlan.chestPosition.z)));
    {
        Player owner; World created(camera,config,owner,chestDirectory,false,0);
        loadStructureFootprint(created,persistencePlan);
        bool emptied = ChestContainer::open(created,owner,persistencePlan.chestPosition);
        for (int slot = 0; slot < ChestContainer::SlotCount && emptied; ++slot) {
            const auto view = ChestContainer::view(created,owner);
            if (!view) { emptied = false; break; }
            const auto item = view->inventory.getSlot(slot);
            if (item.amount > 0) { emptied &= ChestContainer::transferToPlayer(created,owner,slot,item.amount); }
        }
        ChestContainer::close(owner);
        created.setBlock(damage.x,damage.y,damage.z,BlockId::Air);
        chestSaved &= emptied && created.save();
    }
    {
        Player owner; World reopened(camera,config,owner,chestDirectory,false,0);
        loadStructureFootprint(reopened,persistencePlan);
        const auto record = reopened.getBlockEntity(persistencePlan.chestPosition);
        ContainerInventory contents(ChestContainer::SlotCount);
        bool empty = record && ContainerInventory::deserialize(record->payload,contents);
        for (int slot = 0; slot < ChestContainer::SlotCount && empty; ++slot) {
            empty &= contents.getSlot(slot).amount == 0;
        }
        chestSaved &= empty && reopened.getBlock(damage.x,damage.y,damage.z) == BlockId::Air &&
            reopened.getChunkManager().getTerrainGenerationVersion() == 14;
    }
    check("E8/emptied-chest-and-damaged-portal-survive-reopen",chestSaved);
    clearDeterministicEnv(); setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
#endif
