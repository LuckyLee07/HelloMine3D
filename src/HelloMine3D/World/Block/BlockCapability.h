#ifndef BLOCKCAPABILITY_H_INCLUDED
#define BLOCKCAPABILITY_H_INCLUDED

#include <array>
#include <optional>

#include "../../Item/Inventory.h"
#include "../../Maths/glm.h"

class Player;
class SmeltingRegistry;
class World;

enum class InventoryProviderKind
{
    None,
    Chest,
    Furnace
};

enum class MachineProcessorKind
{
    None,
    Furnace
};

struct BlockCapabilityDefinition
{
    const char *blockEntityType = nullptr;
    InventoryProviderKind inventoryProvider = InventoryProviderKind::None;
    MachineProcessorKind machineProcessor = MachineProcessorKind::None;
};

enum class InventorySlotRole
{
    General,
    Input,
    Fuel,
    Output
};

struct InventoryProviderSlotView
{
    InventorySlotState state;
    InventorySlotRole role = InventorySlotRole::General;
    bool insertable = false;
    bool extractable = false;
};

struct InventoryProviderView
{
    static constexpr int MaxSlots = 9;

    glm::ivec3 position{0};
    std::array<InventoryProviderSlotView, MaxSlots> slots{};
    int slotCount = 0;
    bool automaticInsertion = false;
};

struct MachineProcessorView
{
    glm::ivec3 position{0};
    int progressTicks = 0;
    int recipeDurationTicks = 0;
    int burnTicksRemaining = 0;
    int burnTicksTotal = 0;
};

class InventoryProvider
{
  public:
    static constexpr int AutomaticSlot = -1;

    std::optional<InventoryProviderView>
    view(World &world, const SmeltingRegistry &smelting) const;
    bool transferFromPlayer(World &world, Player &player,
                            int providerSlot, int playerSlot, int amount,
                            const SmeltingRegistry &smelting) const;
    bool transferToPlayer(World &world, Player &player,
                          int providerSlot, int amount,
                          const SmeltingRegistry &smelting) const;

    const glm::ivec3 &position() const noexcept { return m_position; }
    InventoryProviderKind kind() const noexcept { return m_kind; }

  private:
    friend class BlockCapabilityAccess;
    InventoryProvider(const glm::ivec3 &position,
                      InventoryProviderKind kind) noexcept
        : m_position(position), m_kind(kind)
    {
    }

    glm::ivec3 m_position{0};
    InventoryProviderKind m_kind = InventoryProviderKind::None;
};

class MachineProcessor
{
  public:
    std::optional<MachineProcessorView>
    view(World &world, const SmeltingRegistry &smelting) const;

    const glm::ivec3 &position() const noexcept { return m_position; }
    MachineProcessorKind kind() const noexcept { return m_kind; }

  private:
    friend class BlockCapabilityAccess;
    MachineProcessor(const glm::ivec3 &position,
                     MachineProcessorKind kind) noexcept
        : m_position(position), m_kind(kind)
    {
    }

    glm::ivec3 m_position{0};
    MachineProcessorKind m_kind = MachineProcessorKind::None;
};

struct BlockCapabilities
{
    std::optional<InventoryProvider> inventoryProvider;
    std::optional<MachineProcessor> machineProcessor;
};

class BlockCapabilityAccess
{
  public:
    static BlockCapabilities query(World &world,
                                   const glm::ivec3 &position);
};

#endif // BLOCKCAPABILITY_H_INCLUDED
