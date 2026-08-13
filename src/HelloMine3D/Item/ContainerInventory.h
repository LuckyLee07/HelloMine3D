#ifndef CONTAINERINVENTORY_H_INCLUDED
#define CONTAINERINVENTORY_H_INCLUDED

#include <cstddef>
#include <string>
#include <vector>

#include "Inventory.h"

class ContainerInventory {
  public:
    static constexpr int MaxSlots = 54;

    explicit ContainerInventory(int slotCount = 9);

    int getSlotCount() const noexcept;
    const InventorySlotState &getSlot(int index) const;
    int addItem(const Material &material, int amount);
    int removeFromSlot(int index, int amount);
    int count(Material::ID materialId) const noexcept;

    std::string serialize() const;
    static bool deserialize(const std::string &payload,
                            ContainerInventory &inventory,
                            std::string *error = nullptr);

  private:
    std::vector<InventorySlotState> m_slots;
};

#endif // CONTAINERINVENTORY_H_INCLUDED
