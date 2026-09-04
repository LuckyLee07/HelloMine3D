#include "BlockCapability.h"

#include "BlockDatabase.h"
#include "ChestContainer.h"
#include "FurnaceContainer.h"
#include "../World.h"
#include "../../Item/ContainerInventory.h"
#include "../../Item/SmeltingRegistry.h"
#include "../../Player/Player.h"

static_assert(InventoryProviderView::MaxSlots >= ChestContainer::SlotCount,
              "Capability view must hold every existing Chest slot.");

namespace {
bool samePosition(const glm::ivec3 &left, const glm::ivec3 &right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool isOpenBy(const Player &player, const glm::ivec3 &position)
{
    return player.getOpenContainer().has_value() &&
           samePosition(*player.getOpenContainer(), position);
}

bool matchesCurrentProvider(World &world, const glm::ivec3 &position,
                            InventoryProviderKind kind)
{
    const BlockCapabilities current =
        BlockCapabilityAccess::query(world, position);
    return current.inventoryProvider.has_value() &&
           current.inventoryProvider->kind() == kind;
}

bool matchesCurrentProcessor(World &world, const glm::ivec3 &position,
                             MachineProcessorKind kind)
{
    const BlockCapabilities current =
        BlockCapabilityAccess::query(world, position);
    return current.machineProcessor.has_value() &&
           current.machineProcessor->kind() == kind;
}
} // namespace

BlockCapabilities BlockCapabilityAccess::query(
    World &world, const glm::ivec3 &position)
{
    BlockCapabilities result;
    const ChunkBlock block =
        world.getBlock(position.x, position.y, position.z);
    const BlockDefinition &definition = BlockDatabase::get().getDefinition(
        static_cast<BlockId>(block.id));
    const BlockCapabilityDefinition &declared = definition.capabilities;
    if (declared.blockEntityType == nullptr) {
        return result;
    }

    const std::optional<BlockEntityRecord> record =
        world.getBlockEntity(position);
    if (!record || record->type != declared.blockEntityType) {
        return result;
    }

    if (declared.inventoryProvider != InventoryProviderKind::None) {
        result.inventoryProvider =
            InventoryProvider(position, declared.inventoryProvider);
    }
    if (declared.machineProcessor != MachineProcessorKind::None) {
        result.machineProcessor =
            MachineProcessor(position, declared.machineProcessor);
    }
    return result;
}

std::optional<InventoryProviderView> InventoryProvider::view(
    World &world, const SmeltingRegistry &smelting) const
{
    if (!matchesCurrentProvider(world, m_position, m_kind)) {
        return std::nullopt;
    }
    const std::optional<BlockEntityRecord> record =
        world.getBlockEntity(m_position);
    if (!record) {
        return std::nullopt;
    }

    InventoryProviderView result;
    result.position = m_position;
    if (m_kind == InventoryProviderKind::Chest) {
        ContainerInventory inventory(ChestContainer::SlotCount);
        if (!ContainerInventory::deserialize(record->payload, inventory)) {
            return std::nullopt;
        }
        result.slotCount = inventory.getSlotCount();
        result.automaticInsertion = true;
        for (int slot = 0; slot < result.slotCount; ++slot) {
            result.slots[slot].state = inventory.getSlot(slot);
            result.slots[slot].role = InventorySlotRole::General;
            result.slots[slot].insertable = true;
            result.slots[slot].extractable = true;
        }
        return result;
    }

    if (m_kind == InventoryProviderKind::Furnace) {
        FurnaceState state;
        if (!FurnaceContainer::deserialize(record->payload, smelting, state)) {
            return std::nullopt;
        }
        result.slotCount = 3;
        result.slots[0] = {state.input, InventorySlotRole::Input, true, true};
        result.slots[1] = {state.fuel, InventorySlotRole::Fuel, true, true};
        result.slots[2] = {state.output, InventorySlotRole::Output, false, true};
        return result;
    }

    return std::nullopt;
}

bool InventoryProvider::transferFromPlayer(
    World &world, Player &player, int providerSlot, int playerSlot,
    int amount, const SmeltingRegistry &smelting) const
{
    if (!isOpenBy(player, m_position) ||
        !matchesCurrentProvider(world, m_position, m_kind)) {
        return false;
    }
    if (m_kind == InventoryProviderKind::Chest) {
        return providerSlot == AutomaticSlot &&
               ChestContainer::transferFromPlayer(
                   world, player, playerSlot, amount);
    }
    if (m_kind == InventoryProviderKind::Furnace) {
        if (providerSlot < static_cast<int>(FurnaceSlot::Input) ||
            providerSlot > static_cast<int>(FurnaceSlot::Output)) {
            return false;
        }
        return FurnaceContainer::transferFromPlayer(
            world, player, static_cast<FurnaceSlot>(providerSlot),
            playerSlot, amount, smelting);
    }
    return false;
}

bool InventoryProvider::transferToPlayer(
    World &world, Player &player, int providerSlot, int amount,
    const SmeltingRegistry &smelting) const
{
    if (!isOpenBy(player, m_position) || providerSlot < 0 ||
        !matchesCurrentProvider(world, m_position, m_kind)) {
        return false;
    }
    if (m_kind == InventoryProviderKind::Chest) {
        return providerSlot < ChestContainer::SlotCount &&
               ChestContainer::transferToPlayer(
                   world, player, providerSlot, amount);
    }
    if (m_kind == InventoryProviderKind::Furnace && providerSlot <= 2) {
        return FurnaceContainer::transferToPlayer(
            world, player, static_cast<FurnaceSlot>(providerSlot), amount,
            smelting);
    }
    return false;
}

std::optional<MachineProcessorView> MachineProcessor::view(
    World &world, const SmeltingRegistry &smelting) const
{
    if (m_kind != MachineProcessorKind::Furnace ||
        !matchesCurrentProcessor(world, m_position, m_kind)) {
        return std::nullopt;
    }
    const std::optional<BlockEntityRecord> record =
        world.getBlockEntity(m_position);
    FurnaceState state;
    if (!record || !FurnaceContainer::deserialize(
                       record->payload, smelting, state)) {
        return std::nullopt;
    }
    const SmeltingRecipeDefinition *recipe =
        smelting.findRecipe(state.input.materialId);
    return MachineProcessorView{
        m_position,
        state.progressTicks,
        recipe != nullptr ? recipe->durationTicks : 0,
        state.burnTicksRemaining,
        state.burnTicksTotal};
}
