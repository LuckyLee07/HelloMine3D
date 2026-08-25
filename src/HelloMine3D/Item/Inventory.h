#ifndef INVENTORY_H_INCLUDED
#define INVENTORY_H_INCLUDED

#include <vector>
#include <cstdint>

#include "ItemStack.h"

struct InventorySlotState {
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;
    int durability = 0;

    bool operator==(const InventorySlotState &other) const noexcept
    {
        return materialId == other.materialId && amount == other.amount &&
               durability == other.durability;
    }

    bool operator!=(const InventorySlotState &other) const noexcept
    {
        return !(*this == other);
    }
};

class Inventory {
  public:
    enum class ToolDamageResult {
        NotTool,
        Damaged,
        Broken
    };

    explicit Inventory(int slotCount = 5);

    int addItem(const Material &material, int amount = 1,
                int durability = -1);
    bool removeFromSelected(int amount = 1);
    int removeFromSlot(int index, int amount);
    ToolDamageResult damageSelectedTool(int amount = 1);
    int capacityFor(const Material &material) const;
    int count(Material::ID materialId) const noexcept;
    std::uint64_t revision() const noexcept;
    bool canConsume(
        const std::vector<InventorySlotState> &consumed) const;
    bool consume(const std::vector<InventorySlotState> &consumed,
                 std::uint64_t expectedRevision);
    bool canExchange(const std::vector<InventorySlotState> &consumed,
                     const Material &produced,
                     int producedAmount) const;
    bool exchange(const std::vector<InventorySlotState> &consumed,
                  const Material &produced, int producedAmount,
                  std::uint64_t expectedRevision);

    ItemStack &getSelectedStack();
    const ItemStack &getSelectedStack() const;
    ItemStack &getSlot(int index);
    const ItemStack &getSlot(int index) const;

    int getSlotCount() const;
    int getSelectedSlot() const;
    void setSelectedSlot(int index);
    void selectNext();
    void selectPrevious();

    std::vector<InventorySlotState> getSaveState() const;
    void applySaveState(const std::vector<InventorySlotState> &slots,
                        int selectedSlot);

  private:
    void ensureUsableSlots();
    void selectNextOccupiedSlot();

    std::vector<ItemStack> m_slots;
    int m_selectedSlot = 0;
    std::uint64_t m_revision = 1;
};

#endif // INVENTORY_H_INCLUDED
