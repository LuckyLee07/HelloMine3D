#include "ChestContainer.h"

#include "../../Player/Player.h"
#include "../../Sandbox/Events/PlayerEvents.h"
#include "../World.h"
#include "BlockEntity.h"
#include "BlockId.h"

#include <algorithm>

namespace {
std::optional<ChestContainerView>
readChest(World &world, const glm::ivec3 &position)
{
    const std::optional<BlockEntityRecord> record =
        world.getBlockEntity(position);
    if (!record || record->type != ChestContainer::BlockEntityType ||
        static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
            BlockId::Chest) {
        return std::nullopt;
    }

    ContainerInventory inventory(ChestContainer::SlotCount);
    if (!ContainerInventory::deserialize(record->payload, inventory)) {
        return std::nullopt;
    }
    return ChestContainerView{position, std::move(inventory)};
}
} // namespace

bool ChestContainer::initialize(World &world, const glm::ivec3 &position)
{
    if (static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
        BlockId::Chest) {
        return false;
    }
    ContainerInventory inventory(SlotCount);
    return world.createBlockEntity(position, BlockEntityType,
                                   inventory.serialize());
}

bool ChestContainer::open(World &world, Player &player,
                          const glm::ivec3 &position)
{
    if (!readChest(world, position)) {
        return false;
    }
    player.openContainer(position);
    return true;
}

void ChestContainer::close(Player &player) noexcept
{
    player.closeContainer();
}

std::optional<ChestContainerView>
ChestContainer::view(World &world, const Player &player)
{
    if (!player.getOpenContainer()) {
        return std::nullopt;
    }
    return readChest(world, *player.getOpenContainer());
}

bool ChestContainer::transferFromPlayer(World &world, Player &player,
                                        int playerSlot, int amount)
{
    std::optional<ChestContainerView> chest = view(world, player);
    if (!chest || playerSlot < 0 ||
        playerSlot >= player.getInventorySlotCount() || amount <= 0) {
        return false;
    }

    const ItemStack &source = player.getInventorySlot(playerSlot);
    if (source.isEmpty() || source.getMaterial().isTool) {
        return false;
    }
    const Material::ID materialId = source.getMaterial().id;
    const int requested = std::min(amount, source.getNumInStack());
    const int moved = chest->inventory.addItem(source.getMaterial(), requested);
    if (moved <= 0 ||
        !world.updateBlockEntity(chest->position,
                                 chest->inventory.serialize())) {
        return false;
    }
    if (player.removeInventoryItem(playerSlot, moved) != moved) {
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, materialId, -moved, "chest_store"));
    return true;
}

bool ChestContainer::transferToPlayer(World &world, Player &player,
                                      int containerSlot, int amount)
{
    std::optional<ChestContainerView> chest = view(world, player);
    if (!chest || amount <= 0) {
        return false;
    }
    const InventorySlotState source =
        chest->inventory.getSlot(containerSlot);
    if (source.amount <= 0 || source.materialId == Material::ID::Nothing) {
        return false;
    }

    const Material &material = Material::toMaterial(source.materialId);
    const int moved = std::min(
        {amount, source.amount, player.getInventoryCapacity(material)});
    if (moved <= 0) {
        return false;
    }
    chest->inventory.removeFromSlot(containerSlot, moved);
    if (!world.updateBlockEntity(chest->position,
                                 chest->inventory.serialize()) ||
        player.addItem(material, moved) != moved) {
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, source.materialId, moved, "chest_take"));
    return true;
}

int ChestContainer::spillContents(World &world,
                                  const glm::ivec3 &position)
{
    const std::optional<BlockEntityRecord> removed =
        world.removeBlockEntity(position);
    if (!removed || removed->type != BlockEntityType) {
        return 0;
    }

    ContainerInventory inventory(SlotCount);
    if (!ContainerInventory::deserialize(removed->payload, inventory)) {
        return 0;
    }

    int entityCount = 0;
    const glm::vec3 dropPosition(
        static_cast<float>(position.x) + 0.5f,
        static_cast<float>(position.y) + 0.8f,
        static_cast<float>(position.z) + 0.5f);
    for (int slot = 0; slot < inventory.getSlotCount(); ++slot) {
        const InventorySlotState &stack = inventory.getSlot(slot);
        if (stack.amount <= 0 || stack.materialId == Material::ID::Nothing) {
            continue;
        }
        if (world.spawnItemEntity(stack.materialId, stack.amount,
                                  dropPosition,
                                  glm::vec3(0.f, 2.5f, 0.f)) !=
            InvalidActorId) {
            ++entityCount;
        }
    }
    return entityCount;
}
