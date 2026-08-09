#include "BlockInteractionSystem.h"

#include "../../Item/Material.h"
#include "../../Player/Player.h"
#include "../../Sandbox/Events/BlockEvents.h"
#include "../../Sandbox/Events/PlayerEvents.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockDefinition.h"
#include "../World.h"

namespace {
bool isReplaceable(BlockId id)
{
    return id == BlockId::Air || id == BlockId::Water;
}
} // namespace

bool BlockInteractionSystem::breakBlock(World &world, Player &player,
                                        const glm::vec3 &location)
{
    const int x = World::toBlockCoord(location.x);
    const int y = World::toBlockCoord(location.y);
    const int z = World::toBlockCoord(location.z);
    const glm::ivec3 blockPosition{x, y, z};

    auto block = world.getBlock(x, y, z);
    const auto blockId = static_cast<BlockId>(block.id);
    if (blockId == BlockId::Air || blockId == BlockId::Water) {
        return false;
    }

    const auto &definition = BlockDatabase::get().getDefinition(blockId);
    if (definition.defaultDrop != Material::ID::Nothing) {
        if (player.addItem(Material::toMaterial(definition.defaultDrop))) {
            world.getEventBus().publish(PlayerInventoryChangedEvent(
                DefaultPlayerActorId, definition.defaultDrop, 1,
                "block_break"));
        }
        else {
            world.spawnItemEntity(
                definition.defaultDrop, 1,
                glm::vec3(static_cast<float>(x) + 0.5f,
                          static_cast<float>(y) + 0.5f,
                          static_cast<float>(z) + 0.5f),
                glm::vec3(0.f, 2.5f, 0.f));
        }
    }

    world.setBlock(x, y, z, BlockId::Air);
    world.getEventBus().publish(BlockBreakEvent(blockPosition, blockId));
    world.getEventBus().publish(
        BlockChangedEvent(blockPosition, blockId, BlockId::Air));
    return true;
}

bool BlockInteractionSystem::placeBlock(World &world, Player &player,
                                        const glm::vec3 &location)
{
    const int x = World::toBlockCoord(location.x);
    const int y = World::toBlockCoord(location.y);
    const int z = World::toBlockCoord(location.z);
    const glm::ivec3 blockPosition{x, y, z};

    auto &stack = player.getHeldItems();
    const auto &material = stack.getMaterial();
    if (!material.isBlock || material.id == Material::ID::Nothing ||
        stack.getNumInStack() <= 0) {
        return false;
    }

    const auto existingBlock = static_cast<BlockId>(world.getBlock(x, y, z).id);
    if (!isReplaceable(existingBlock)) {
        return false;
    }

    const auto placedBlock = material.toBlockID();
    world.setBlock(x, y, z, placedBlock);
    player.removeHeldItem();
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, material.id, -1, "block_place"));
    world.getEventBus().publish(BlockPlaceEvent(blockPosition, placedBlock));
    world.getEventBus().publish(
        BlockChangedEvent(blockPosition, existingBlock, placedBlock));
    return true;
}

bool BlockInteractionSystem::useBlock(World &world, Player &player,
                                      const glm::vec3 &location)
{
    const int x = World::toBlockCoord(location.x);
    const int y = World::toBlockCoord(location.y);
    const int z = World::toBlockCoord(location.z);
    const auto blockId = static_cast<BlockId>(world.getBlock(x, y, z).id);
    if (blockId == BlockId::Air || blockId == BlockId::Water) {
        return false;
    }

    (void)player;
    world.getEventBus().publish(BlockUseEvent({x, y, z}, blockId));
    return true;
}
