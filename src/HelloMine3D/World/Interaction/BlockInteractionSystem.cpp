#include "BlockInteractionSystem.h"

#include "../../Item/Material.h"
#include "../../Item/ToolRegistry.h"
#include "../../Player/Player.h"
#include "../../Sandbox/Events/BlockEvents.h"
#include "../../Sandbox/Events/PlayerEvents.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockBehavior.h"
#include "../Block/BlockDefinition.h"
#include "../World.h"

#include <algorithm>

namespace {
bool isReplaceable(BlockId id)
{
    return id == BlockId::Air || id == BlockId::Water;
}
} // namespace

BlockMiningEvaluation BlockInteractionSystem::evaluateMining(
    BlockId blockId, const ItemStack &heldItem)
{
    const BlockDefinition &block =
        BlockDatabase::get().getDefinition(blockId);
    BlockMiningEvaluation result;
    const ToolDefinition *tool =
        runtimeToolRegistry().find(heldItem.getMaterial().id);
    result.matchingClass =
        block.miningClass == MiningClass::None ||
        (tool != nullptr && tool->miningClass == block.miningClass);
    result.meetsTier =
        block.requiredToolTier == 0 ||
        (result.matchingClass && tool != nullptr &&
         tool->tier >= block.requiredToolTier);
    if (tool != nullptr && block.miningClass != MiningClass::None &&
        tool->miningClass == block.miningClass) {
        result.speedMultiplier = tool->speedMultiplier;
    }
    result.requiredSeconds = std::max(
        0.05f, block.hardness / result.speedMultiplier);
    result.dropAllowed = block.wrongToolDrops ||
                         (result.matchingClass && result.meetsTier);
    return result;
}

bool BlockInteractionSystem::breakBlock(World &world, Player &player,
                                        const glm::vec3 &location)
{
    if (player.hasOpenContainer() || player.hasOpenCrafting()) {
        return false;
    }
    const int x = World::toBlockCoord(location.x);
    const int y = World::toBlockCoord(location.y);
    const int z = World::toBlockCoord(location.z);
    const glm::ivec3 blockPosition{x, y, z};

    const ChunkBlock block = world.getBlock(x, y, z);
    const auto blockId = static_cast<BlockId>(block.id);
    if (blockId == BlockId::Air || blockId == BlockId::Water) {
        return false;
    }

    const auto &definition = BlockDatabase::get().getDefinition(blockId);
    const BlockMiningEvaluation mining =
        evaluateMining(blockId, player.getHeldItems());
    const Material::ID drop =
        mining.dropAllowed
            ? definition.behavior->getDrop(definition, block)
            : Material::ID::Nothing;
    const Inventory::ToolDamageResult toolDamage =
        player.damageHeldTool();
    if (drop != Material::ID::Nothing) {
        if (player.addItem(Material::toMaterial(drop))) {
            world.getEventBus().publish(PlayerInventoryChangedEvent(
                DefaultPlayerActorId, drop, 1, "block_break"));
        }
        else {
            world.spawnItemEntity(
                drop, 1,
                glm::vec3(static_cast<float>(x) + 0.5f,
                          static_cast<float>(y) + 0.5f,
                          static_cast<float>(z) + 0.5f),
                glm::vec3(0.f, 2.5f, 0.f));
        }
    }

    definition.behavior->onBroken(world, player, blockPosition, block);
    world.setBlock(x, y, z, BlockId::Air);
    world.getEventBus().publish(BlockBreakEvent(blockPosition, blockId));
    world.getEventBus().publish(
        BlockChangedEvent(blockPosition, blockId, BlockId::Air));
    if (toolDamage != Inventory::ToolDamageResult::NotTool) {
        world.getEventBus().publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, Material::ID::Nothing, 0,
            toolDamage == Inventory::ToolDamageResult::Broken
                ? "tool_broken"
                : "tool_damaged"));
    }
    return true;
}

bool BlockInteractionSystem::placeBlock(World &world, Player &player,
                                        const glm::vec3 &location)
{
    if (player.hasOpenContainer() || player.hasOpenCrafting()) {
        return false;
    }
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

    const ChunkBlock existingBlock = world.getBlock(x, y, z);
    const auto existingBlockId = static_cast<BlockId>(existingBlock.id);
    if (!isReplaceable(existingBlockId)) {
        return false;
    }

    const auto placedBlock = material.toBlockID();
    const auto &definition =
        BlockDatabase::get().getDefinition(placedBlock);
    if (!definition.behavior->canPlace(world, player, blockPosition,
                                       existingBlock)) {
        return false;
    }

    world.setBlock(x, y, z, placedBlock);
    const ChunkBlock placedChunkBlock(placedBlock);
    definition.behavior->onPlaced(world, player, blockPosition,
                                  existingBlock, placedChunkBlock);
    player.removeHeldItem();
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, material.id, -1, "block_place"));
    world.getEventBus().publish(BlockPlaceEvent(blockPosition, placedBlock));
    world.getEventBus().publish(
        BlockChangedEvent(blockPosition, existingBlockId, placedBlock));
    return true;
}

bool BlockInteractionSystem::useBlock(World &world, Player &player,
                                      const glm::vec3 &location)
{
    const int x = World::toBlockCoord(location.x);
    const int y = World::toBlockCoord(location.y);
    const int z = World::toBlockCoord(location.z);
    const ChunkBlock block = world.getBlock(x, y, z);
    const auto blockId = static_cast<BlockId>(block.id);
    if (blockId == BlockId::Air || blockId == BlockId::Water) {
        return false;
    }

    const auto &definition = BlockDatabase::get().getDefinition(blockId);
    definition.behavior->onUse(world, player, {x, y, z}, block);
    world.getEventBus().publish(BlockUseEvent({x, y, z}, blockId));
    return true;
}
