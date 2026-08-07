#include "Inventory.h"

#include <algorithm>

Inventory::Inventory(int slotCount)
{
    const int count = std::max(1, slotCount);
    m_slots.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_slots.emplace_back(Material::NOTHING, 0);
    }
}

int Inventory::addItem(const Material &material, int amount)
{
    if (material.id == Material::ID::Nothing || amount <= 0) {
        return 0;
    }

    int remaining = amount;
    for (auto &slot : m_slots) {
        if (slot.getMaterial().id == material.id && !slot.isEmpty()) {
            remaining = slot.add(remaining);
            if (remaining == 0) {
                return amount;
            }
        }
    }

    for (auto &slot : m_slots) {
        if (slot.isEmpty()) {
            slot = ItemStack(material, 0);
            remaining = slot.add(remaining);
            if (remaining == 0) {
                return amount;
            }
        }
    }

    return amount - remaining;
}

bool Inventory::removeFromSelected(int amount)
{
    if (amount <= 0) {
        return false;
    }

    ItemStack &stack = getSelectedStack();
    if (stack.getNumInStack() < amount) {
        return false;
    }

    stack.remove(amount);
    return true;
}

ItemStack &Inventory::getSelectedStack()
{
    ensureUsableSlots();
    return m_slots[m_selectedSlot];
}

const ItemStack &Inventory::getSelectedStack() const
{
    return m_slots[m_selectedSlot];
}

ItemStack &Inventory::getSlot(int index)
{
    ensureUsableSlots();
    if (index < 0 || index >= static_cast<int>(m_slots.size())) {
        return m_slots[m_selectedSlot];
    }

    return m_slots[index];
}

const ItemStack &Inventory::getSlot(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_slots.size())) {
        return m_slots[m_selectedSlot];
    }

    return m_slots[index];
}

int Inventory::getSlotCount() const
{
    return static_cast<int>(m_slots.size());
}

int Inventory::getSelectedSlot() const
{
    return m_selectedSlot;
}

void Inventory::setSelectedSlot(int index)
{
    ensureUsableSlots();
    if (index < 0) {
        m_selectedSlot = 0;
    }
    else if (index >= static_cast<int>(m_slots.size())) {
        m_selectedSlot = static_cast<int>(m_slots.size()) - 1;
    }
    else {
        m_selectedSlot = index;
    }
}

void Inventory::selectNext()
{
    ensureUsableSlots();
    m_selectedSlot++;
    if (m_selectedSlot >= static_cast<int>(m_slots.size())) {
        m_selectedSlot = 0;
    }
}

void Inventory::selectPrevious()
{
    ensureUsableSlots();
    m_selectedSlot--;
    if (m_selectedSlot < 0) {
        m_selectedSlot = static_cast<int>(m_slots.size()) - 1;
    }
}

std::vector<InventorySlotState> Inventory::getSaveState() const
{
    std::vector<InventorySlotState> state;
    state.reserve(m_slots.size());
    for (const auto &slot : m_slots) {
        state.push_back({slot.getMaterial().id, slot.getNumInStack()});
    }

    return state;
}

void Inventory::applySaveState(const std::vector<InventorySlotState> &slots,
                               int selectedSlot)
{
    if (!slots.empty()) {
        m_slots.clear();
        m_slots.reserve(slots.size());
        for (const auto &slot : slots) {
            if (slot.amount > 0 && slot.materialId != Material::ID::Nothing) {
                m_slots.emplace_back(Material::toMaterial(slot.materialId),
                                     slot.amount);
            }
            else {
                m_slots.emplace_back(Material::NOTHING, 0);
            }
        }
    }

    ensureUsableSlots();
    setSelectedSlot(selectedSlot);
}

void Inventory::ensureUsableSlots()
{
    if (m_slots.empty()) {
        m_slots.emplace_back(Material::NOTHING, 0);
        m_selectedSlot = 0;
    }

    if (m_selectedSlot < 0) {
        m_selectedSlot = 0;
    }
    if (m_selectedSlot >= static_cast<int>(m_slots.size())) {
        m_selectedSlot = static_cast<int>(m_slots.size()) - 1;
    }
}
