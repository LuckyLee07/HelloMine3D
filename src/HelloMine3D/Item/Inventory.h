#ifndef INVENTORY_H_INCLUDED
#define INVENTORY_H_INCLUDED

#include <vector>

#include "ItemStack.h"

struct InventorySlotState {
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;
};

class Inventory {
  public:
    explicit Inventory(int slotCount = 5);

    int addItem(const Material &material, int amount = 1);
    bool removeFromSelected(int amount = 1);

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

    std::vector<ItemStack> m_slots;
    int m_selectedSlot = 0;
};

#endif // INVENTORY_H_INCLUDED
