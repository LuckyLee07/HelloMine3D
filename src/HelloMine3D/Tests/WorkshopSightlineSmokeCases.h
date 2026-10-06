#pragma once

#include "../World/Generation/Structures/LandmarkWorkshop.h"
#include "../World/Storage/ChunkStorageData.h"

#include <map>
#include <memory>

namespace {
void caseWorkshopSightline()
{
    check("WORKSHOP31/strict-new-default-and-save-boundary",
          HighlandWorkshopSightlineTerrainGenerationVersion == 31 &&
          CurrentTerrainGenerationVersion == 31 &&
          RockLandmarkTerrainGenerationVersion == 30 &&
          WorkshopCourtyardTerrainGenerationVersion == 29 &&
          ClassicOverWorldGenerator().getGenerationVersion() == 31 &&
          WorldSaveFormatVersion == 12);

    bool exactBlueprint = true, legacyBlueprint = true, mirrored = true;
    for (const auto style : {LandmarkApproach::Style::Open,
                             LandmarkApproach::Style::Forest,
                             LandmarkApproach::Style::Riverbank,
                             LandmarkApproach::Style::Highland}) {
        for (const int side : {-1, 1}) {
            LandmarkWorkshop::Site site{true, style, side, -40, 5, 134};
            mirrored &= site.worldX(0) != site.worldX(2) &&
                site.machineX() == -39 && site.machineZ() == 7 &&
                site.worldX(2) == (side < 0 ? -40 : -38);
            int changes = 0;
            for (int a = 0; a < 3; ++a)
                for (int d = 0; d < 3; ++d)
                    for (int h = 0; h < 4; ++h) {
                        const auto before = LandmarkWorkshop::blockAt(style, a, d, h, true);
                        const auto after = LandmarkWorkshop::blockAt(style, a, d, h, true, true);
                        const bool corner = style == LandmarkApproach::Style::Highland &&
                            a == 2 && d == 0 && h == 1;
                        if (before != after) ++changes;
                        exactBlueprint &= corner
                            ? before == BlockId::Stone && after == BlockId::Air
                            : before == after;
                        legacyBlueprint &= LandmarkWorkshop::blockAt(style, a, d, h) ==
                            LandmarkWorkshop::blockAt(style, a, d, h, false, true);
                    }
            exactBlueprint &= changes == (style == LandmarkApproach::Style::Highland ? 1 : 0);
        }
    }
    check("WORKSHOP31/both-mirrors-only-highland-front-corner-cleared", exactBlueprint && mirrored);
    check("WORKSHOP31/legacy-covered-workshop-ignores-front-clearance", legacyBlueprint);

    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player owner;
    World sampleWorld(camera, config, owner, freshSaveDirectory("workshop31_samples"), false, 0);
    struct Selected {
        StructurePlanSnapshot plan;
        LandmarkWorkshop::Site site;
    };
    std::array<Selected, 3> selected{};
    ClassicOverWorldGenerator selector(42, RockLandmarkTerrainGenerationVersion);
    for (int radius = 0; radius <= 24; ++radius) {
        for (int cx = -radius; cx <= radius; ++cx)
            for (int cz = -radius; cz <= radius; ++cz) {
                if (std::max(std::abs(cx), std::abs(cz)) != radius) continue;
                for (const auto type : {StructureType::Waystone, StructureType::Ruin,
                                        StructureType::RaiderCamp}) {
                    const auto plan = selector.getStructurePlanForCell(type, cx, cz);
                    const auto site = LandmarkWorkshop::select(plan,
                        [&selector](int x, int z) { return selector.getSurfaceHeightAtWorld(x, z); },
                        [&selector](int x, int z) { return selector.getBiomeAtWorld(x, z); });
                    if (!site.valid) continue;
                    const int index = site.style == LandmarkApproach::Style::Forest ? 0
                        : site.style == LandmarkApproach::Style::Riverbank ? 1 : 2;
                    if (selected[index].site.valid) continue;
                    const int chunkX = WorldCoordinates::floorDiv(site.minimumX, CHUNK_SIZE);
                    const int chunkZ = WorldCoordinates::floorDiv(site.minimumZ, CHUNK_SIZE);
                    Chunk candidate(sampleWorld, {chunkX, chunkZ}, false);
                    selector.generateTerrainFor(candidate);
                    const glm::ivec3 machine{site.machineX() - chunkX * CHUNK_SIZE,
                                            site.baseY + 1, site.machineZ() - chunkZ * CHUNK_SIZE};
                    if (candidate.getBlock(machine.x, machine.y, machine.z) ==
                            LandmarkWorkshop::machine(site.style) &&
                        candidate.findBlockEntity(machine) != nullptr)
                        selected[index] = {plan, site};
                }
            }
        if (std::all_of(selected.begin(), selected.end(),
                [](const auto &entry) { return entry.site.valid; })) break;
    }
    const bool found = std::all_of(selected.begin(), selected.end(),
        [](const auto &entry) { return entry.site.valid; });
    check("WORKSHOP31/three-real-seed42-regional-workshops", found);
    if (!found) {
        clearDeterministicEnv();
        setEnv("HELLOMINE3D_SEED", "");
        return;
    }
    const auto &highland = selected[2];
    check("WORKSHOP31/frozen-real-occlusion-site",
          highland.plan.anchor == glm::ivec3(-36, 135, 11) &&
          highland.site.minimumX == -40 && highland.site.minimumZ == 5 &&
          highland.site.baseY == 134 && highland.site.machineX() == -39 &&
          highland.site.machineZ() == 7);

    const std::array<std::string, 3> entityTypes{{ChestContainer::BlockEntityType,
        FurnaceContainer::BlockEntityType, CrusherContainer::BlockEntityType}};
    const std::array<std::string, 3> emptyPayloads{{
        ContainerInventory(ChestContainer::SlotCount).serialize(),
        FurnaceContainer::serialize(FurnaceState{}), CrusherContainer::serialize(CrusherState{})}};
    for (int index = 0; index < 3; ++index) {
        const auto &item = selected[index];
        const auto &site = item.site;
        const auto &plan = item.plan;
        const std::string label = "WORKSHOP31/style-" + std::to_string(index);
        ClassicOverWorldGenerator previous(42, RockLandmarkTerrainGenerationVersion);
        ClassicOverWorldGenerator current(42, HighlandWorkshopSightlineTerrainGenerationVersion);
        ClassicOverWorldGenerator reversed(42, HighlandWorkshopSightlineTerrainGenerationVersion);
        const auto currentPlan = current.getStructurePlanForCell(plan.key.type, plan.key.cellX, plan.key.cellZ);
        const auto currentSite = LandmarkWorkshop::select(currentPlan,
            [&current](int x, int z) { return current.getSurfaceHeightAtWorld(x, z); },
            [&current](int x, int z) { return current.getBiomeAtWorld(x, z); });
        check(label + "-candidate-site-layout-loot-held",
              currentPlan.valid && currentPlan.key == plan.key && currentPlan.anchor == plan.anchor &&
              currentPlan.selectionHash == plan.selectionHash && currentSite.valid &&
              currentSite.minimumX == site.minimumX && currentSite.minimumZ == site.minimumZ &&
              currentSite.baseY == site.baseY && currentSite.side == site.side &&
              currentSite.style == site.style &&
              LandmarkExpedition::layoutFor(currentPlan) == LandmarkExpedition::layoutFor(plan) &&
              structureLootForPlan(currentPlan, ExplorationRewards::CurrentVersion).entries ==
                  structureLootForPlan(plan, ExplorationRewards::CurrentVersion).entries);
        auto area = plan.footprint;
        area.minimumX = std::min(area.minimumX, site.minimumX);
        area.maximumX = std::max(area.maximumX, site.minimumX + 2);
        area.minimumZ = std::min(area.minimumZ, site.minimumZ);
        area.maximumZ = std::max(area.maximumZ, site.minimumZ + 2);
        std::vector<glm::ivec2> locations;
        for (int cx = WorldCoordinates::floorDiv(area.minimumX, CHUNK_SIZE);
             cx <= WorldCoordinates::floorDiv(area.maximumX, CHUNK_SIZE); ++cx)
            for (int cz = WorldCoordinates::floorDiv(area.minimumZ, CHUNK_SIZE);
                 cz <= WorldCoordinates::floorDiv(area.maximumZ, CHUNK_SIZE); ++cz)
                locations.emplace_back(cx, cz);
        using ChunkKey = std::pair<int, int>;
        std::map<ChunkKey, std::unique_ptr<Chunk>> before, after, reverse;
        for (const auto location : locations) {
            const ChunkKey key{location.x, location.y};
            before[key] = std::make_unique<Chunk>(sampleWorld, location, false);
            after[key] = std::make_unique<Chunk>(sampleWorld, location, false);
            previous.generateTerrainFor(*before[key]);
            current.generateTerrainFor(*after[key]);
        }
        for (auto it = locations.rbegin(); it != locations.rend(); ++it) {
            const ChunkKey key{it->x, it->y};
            reverse[key] = std::make_unique<Chunk>(sampleWorld, *it, false);
            reversed.generateTerrainFor(*reverse[key]);
        }
        const glm::ivec3 corner{site.worldX(2), site.baseY + 1, site.worldZ(0)};
        const glm::ivec3 machine{site.machineX(), site.baseY + 1, site.machineZ()};
        bool exactChanges = true, reverseExact = true, entitiesHeld = true;
        int differences = 0, matchingMachines = 0;
        for (const auto &entry : after) {
            const auto &now = *entry.second;
            const auto &old = *before.at(entry.first);
            const auto &backwards = *reverse.at(entry.first);
            for (int x = 0; x < CHUNK_SIZE; ++x)
                for (int z = 0; z < CHUNK_SIZE; ++z)
                    for (int y = 0; y < 256; ++y) {
                        const auto a = now.getBlock(x, y, z), b = old.getBlock(x, y, z);
                        reverseExact &= a == backwards.getBlock(x, y, z);
                        if (a == b) continue;
                        ++differences;
                        const glm::ivec3 position{entry.first.first * CHUNK_SIZE + x,
                            y, entry.first.second * CHUNK_SIZE + z};
                        exactChanges &= index == 2 && position == corner &&
                            b == ChunkBlock(BlockId::Stone, 0) && a == ChunkBlock(BlockId::Air, 0);
                    }
            const auto &records = now.getBlockEntities();
            const auto &oldRecords = old.getBlockEntities();
            const auto &reverseRecords = backwards.getBlockEntities();
            entitiesHeld &= records.size() == oldRecords.size();
            reverseExact &= records.size() == reverseRecords.size();
            for (std::size_t r = 0; r < records.size(); ++r) {
                if (r < oldRecords.size()) entitiesHeld &= sameBlockEntityRecord(records[r], oldRecords[r]);
                if (r < reverseRecords.size()) reverseExact &= sameBlockEntityRecord(records[r], reverseRecords[r]);
                const glm::ivec3 position{entry.first.first * CHUNK_SIZE + records[r].position.x,
                    records[r].position.y, entry.first.second * CHUNK_SIZE + records[r].position.z};
                if (position == machine) {
                    ++matchingMachines;
                    entitiesHeld &= records[r].type == entityTypes[index] && records[r].payload == emptyPayloads[index];
                }
            }
        }
        check(label + "-complete-chunk-id-metadata-only-approved-corner-diff",
              exactChanges && differences == (index == 2 ? 1 : 0),
              "chunks=" + std::to_string(after.size()) + " differingBlocks=" + std::to_string(differences));
        check(label + "-complete-reverse-id-metadata-entity-payload-held", reverseExact);
        check(label + "-all-entity-payloads-held-one-empty-machine", entitiesHeld && matchingMachines == 1);
        const auto blockAt = [&after](int x, int y, int z) {
            const int cx = WorldCoordinates::floorDiv(x, CHUNK_SIZE);
            const int cz = WorldCoordinates::floorDiv(z, CHUNK_SIZE);
            return after.at({cx, cz})->getBlock(x - cx * CHUNK_SIZE, y, z - cz * CHUNK_SIZE);
        };
        bool supportedRoute = true;
        for (int a = 0; a < 3; ++a)
            for (int d = 0; d < 3; ++d)
                supportedRoute &= blockAt(site.worldX(a), site.baseY, site.worldZ(d)).getData().isCollidable;
        for (int d = 0; d < 2; ++d)
            for (int h = 1; h <= 2; ++h)
                supportedRoute &= blockAt(site.worldX(1), site.baseY + h, site.worldZ(d)) == BlockId::Air;
        supportedRoute &= blockAt(machine.x, machine.y, machine.z) == LandmarkWorkshop::machine(site.style);
        check(label + "-nine-supported-floor-cells-two-clear-blocks-to-machine", supportedRoute);
        if (index == 2) {
            const int cx = WorldCoordinates::floorDiv(corner.x, CHUNK_SIZE);
            const int cz = WorldCoordinates::floorDiv(corner.z, CHUNK_SIZE);
            check(label + "-actual-v30-occluding-corner-negative",
                  before.at({cx, cz})->getBlock(corner.x - cx * CHUNK_SIZE, corner.y,
                      corner.z - cz * CHUNK_SIZE) == BlockId::Stone &&
                  blockAt(corner.x, corner.y, corner.z) == BlockId::Air);
        }

        const auto directory = freshSaveDirectory("workshop31_lifecycle_" + std::to_string(index));
        bool lifecycle = initializeTerrainIdentity(directory, "workshop31-" + std::to_string(index), 31, 42);
        std::string savedMachinePayload;
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(WorldCoordinates::floorDiv(machine.x, CHUNK_SIZE),
                                              WorldCoordinates::floorDiv(machine.z, CHUNK_SIZE));
            const auto record = world.getBlockEntity(machine);
            lifecycle &= record && record->type == entityTypes[index] && record->payload == emptyPayloads[index];
            lifecycle &= index == 0 ? ChestContainer::open(world, player, machine)
                : index == 1 ? FurnaceContainer::open(world, player, machine, runtimeSmeltingRegistry())
                             : CrusherContainer::open(world, player, machine);
            if (index == 2) {
                player.addItem(Material::COBBLESTONE_BLOCK, 1);
                int inputSlot = -1;
                for (int slot = 0; slot < player.getInventorySlotCount(); ++slot)
                    if (player.getInventorySlot(slot).getMaterial().id == Material::ID::Cobblestone) inputSlot = slot;
                bool processed = inputSlot >= 0 && CrusherContainer::transferFromPlayer(
                    world, player, CrusherSlot::Input, inputSlot, 1);
                processed &= CrusherContainer::supplyManualPower(world, player, machine);
                processed &= CrusherContainer::supplyManualPower(world, player, machine);
                for (int tick = 0; tick < CrusherContainer::MaxCrankTicks; ++tick)
                    CrusherContainer::tickOne(world, machine);
                const auto result = CrusherContainer::view(world, player);
                processed &= result && result->state.input.amount == 0 &&
                    result->state.output.materialId == Material::ID::Sand && result->state.output.amount == 1;
                processed &= CrusherContainer::transferToPlayer(world, player, CrusherSlot::Output, 1);
                bool received = false;
                for (int slot = 0; slot < player.getInventorySlotCount(); ++slot)
                    received |= player.getInventorySlot(slot).getMaterial().id == Material::ID::Sand &&
                        player.getInventorySlot(slot).getNumInStack() == 1;
                check(label + "-real-generated-crusher-processes-and-returns-sand", processed && received);
                world.setBlock(corner.x, corner.y, corner.z, BlockId::OakPlank);
            }
            player.closeContainer();
            const auto afterUse = world.getBlockEntity(machine);
            if (afterUse) savedMachinePayload = afterUse->payload;
            lifecycle &= afterUse.has_value() && world.save();
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(WorldCoordinates::floorDiv(machine.x, CHUNK_SIZE),
                                              WorldCoordinates::floorDiv(machine.z, CHUNK_SIZE));
            const auto record = world.getBlockEntity(machine);
            lifecycle &= record && record->type == entityTypes[index] && record->payload == savedMachinePayload;
            if (index == 2) {
                lifecycle &= world.getBlock(corner.x, corner.y, corner.z) == BlockId::OakPlank;
                world.setBlock(corner.x, corner.y, corner.z, BlockId::Air);
            }
            world.setBlock(machine.x, machine.y, machine.z, BlockId::Air);
            lifecycle &= !world.getBlockEntity(machine) && world.save();
        }
        {
            Player player;
            World world(camera, config, player, directory, false, 0);
            world.getChunkManager().loadChunk(WorldCoordinates::floorDiv(machine.x, CHUNK_SIZE),
                                              WorldCoordinates::floorDiv(machine.z, CHUNK_SIZE));
            lifecycle &= world.getBlock(machine.x, machine.y, machine.z) == BlockId::Air && !world.getBlockEntity(machine);
            if (index == 2) lifecycle &= world.getBlock(corner.x, corner.y, corner.z) == BlockId::Air;
        }
        WorldSaveData identity;
        check(label + "-machine-payload-edits-break-save-reopen-no-refill",
              lifecycle && WorldSave(directory).load(identity) && identity.terrainGenerationVersion == 31);
    }

    const auto &oldSite = highland.site;
    const int cx = WorldCoordinates::floorDiv(oldSite.minimumX, CHUNK_SIZE);
    const int cz = WorldCoordinates::floorDiv(oldSite.minimumZ, CHUNK_SIZE);
    const glm::ivec3 oldCorner{oldSite.worldX(2), oldSite.baseY + 1, oldSite.worldZ(0)};
    const auto oldDirectory = freshSaveDirectory("workshop31_old30_unknown_chunk");
    const auto unknownPath = ChunkStorageData(ResourcePaths::join(oldDirectory, "chunks")).chunkPath(cx, cz);
    bool oldFuture = initializeTerrainIdentity(oldDirectory, "workshop31-old30", 30, 42);
    {
        Player player;
        World world(camera, config, player, oldDirectory, false, 0);
        world.getChunkManager().loadChunk(0, 0);
        world.setBlock(1, 180, 1, BlockId::OakPlank);
        oldFuture &= world.getChunkManager().findChunk(cx, cz) == nullptr &&
            !std::filesystem::exists(unknownPath) && world.save();
    }
    oldFuture &= !std::filesystem::exists(unknownPath);
    {
        Player player;
        World world(camera, config, player, oldDirectory, false, 0);
        auto &manager = world.getChunkManager();
        oldFuture &= manager.getTerrainGenerationVersion() == 30 && manager.findChunk(cx, cz) == nullptr &&
            !std::filesystem::exists(unknownPath);
        ClassicOverWorldGenerator previous(42, RockLandmarkTerrainGenerationVersion);
        Chunk reference(world, {cx, cz}, false);
        previous.generateTerrainFor(reference);
        manager.loadChunk(cx, cz);
        const auto &actual = manager.getChunk(cx, cz);
        bool exact = true;
        for (int x = 0; x < CHUNK_SIZE; ++x)
            for (int z = 0; z < CHUNK_SIZE; ++z)
                for (int y = 0; y < 256; ++y) exact &= actual.getBlock(x, y, z) == reference.getBlock(x, y, z);
        const auto &actualRecords = actual.getBlockEntities(), &expectedRecords = reference.getBlockEntities();
        exact &= actualRecords.size() == expectedRecords.size();
        for (std::size_t r = 0; r < actualRecords.size() && r < expectedRecords.size(); ++r)
            exact &= sameBlockEntityRecord(actualRecords[r], expectedRecords[r]);
        oldFuture &= exact && world.getBlock(oldCorner.x, oldCorner.y, oldCorner.z) == ChunkBlock(BlockId::Stone, 0);
        manager.loadChunk(0, 0);
        oldFuture &= world.getBlock(1, 180, 1) == BlockId::OakPlank;
    }
    WorldSaveData oldIdentity;
    check("WORKSHOP31/v30-reopen-first-unknown-workshop-full-bytes-retain-corner-and-player-edit",
          oldFuture && WorldSave(oldDirectory).load(oldIdentity) && oldIdentity.terrainGenerationVersion == 30);

    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "42");
    const auto newDirectory = freshSaveDirectory("workshop31_new_default");
    bool newIdentity = false;
    {
        Player player;
        World world(camera, config, player, newDirectory, false, 0);
        const auto spawn = world.getPlayerSpawnPoint();
        const int x = World::toBlockCoord(spawn.x);
        const int y = World::toBlockCoord(spawn.y);
        const int z = World::toBlockCoord(spawn.z);
        const auto floor = world.getBlock(x, y - 2, z);
        check("WORKSHOP31/new-default-safe-real-spawn-without-position-override",
              world.getChunkManager().getTerrainGenerationVersion() == 31 &&
              floor != BlockId::Water && floor.getData().isCollidable &&
              world.getBlock(x, y - 1, z) == BlockId::Air &&
              world.getBlock(x, y, z) == BlockId::Air,
              vecToString(spawn));
        newIdentity = world.getChunkManager().getTerrainGenerationVersion() == 31 && world.save();
    }
    {
        Player player;
        World world(camera, config, player, newDirectory, false, 0);
        newIdentity &= world.getChunkManager().getTerrainGenerationVersion() == 31;
    }
    WorldSaveData newMetadata;
    check("WORKSHOP31/new-default-save-reopen-stays-exactly31",
          newIdentity && WorldSave(newDirectory).load(newMetadata) && newMetadata.terrainGenerationVersion == 31);
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
} // namespace
