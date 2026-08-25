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

int Inventory::addItem(const Material &material, int amount, int durability)
{
    if (material.id == Material::ID::Nothing || amount <= 0) {
        return 0;
    }

    int remaining = amount;
    for (auto &slot : m_slots) {
        if (slot.getMaterial().id == material.id && !slot.isEmpty()) {
            remaining = slot.add(remaining);
            if (remaining == 0) {
                ++m_revision;
                return amount;
            }
        }
    }

    for (auto &slot : m_slots) {
        if (slot.isEmpty()) {
            slot = ItemStack(material, remaining, durability);
            remaining -= slot.getNumInStack();
            if (remaining == 0) {
                ++m_revision;
                return amount;
            }
        }
    }

    const int added = amount - remaining;
    if (added > 0) {
        ++m_revision;
    }
    return added;
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
    ++m_revision;
    return true;
}

Inventory::ToolDamageResult Inventory::damageSelectedTool(int amount)
{
    if (amount <= 0) {
        return ToolDamageResult::NotTool;
    }
    ItemStack &stack = getSelectedStack();
    if (!stack.isDamageable()) {
        return ToolDamageResult::NotTool;
    }
    const bool broken = stack.damage(amount);
    ++m_revision;
    if (broken) {
        selectNextOccupiedSlot();
        return ToolDamageResult::Broken;
    }
    return ToolDamageResult::Damaged;
}

int Inventory::removeFromSlot(int index, int amount)
{
    if (index < 0 || index >= static_cast<int>(m_slots.size()) ||
        amount <= 0) {
        return 0;
    }

    ItemStack &stack = m_slots[index];
    const int removed = std::min(amount, stack.getNumInStack());
    stack.remove(removed);
    if (removed > 0) {
        ++m_revision;
    }
    return removed;
}

int Inventory::capacityFor(const Material &material) const
{
    if (material.id == Material::ID::Nothing) {
        return 0;
    }

    int capacity = 0;
    for (const ItemStack &slot : m_slots) {
        if (slot.isEmpty()) {
            capacity += material.maxStackSize;
        }
        else if (slot.getMaterial().id == material.id) {
            capacity += material.maxStackSize - slot.getNumInStack();
        }
    }
    return capacity;
}

int Inventory::count(Material::ID materialId) const noexcept
{
    int total = 0;
    for (const ItemStack &slot : m_slots) {
        if (!slot.isEmpty() && slot.getMaterial().id == materialId) {
            total += slot.getNumInStack();
        }
    }
    return total;
}

std::uint64_t Inventory::revision() const noexcept
{
    return m_revision;
}

bool Inventory::canConsume(
    const std::vector<InventorySlotState> &consumed) const
{
    std::vector<ItemStack> candidate = m_slots;
    for (const InventorySlotState &requirement : consumed) {
        if (requirement.materialId == Material::ID::Nothing ||
            requirement.amount <= 0) {
            return false;
        }
        int remaining = requirement.amount;
        for (ItemStack &slot : candidate) {
            if (slot.isEmpty() ||
                slot.getMaterial().id != requirement.materialId) {
                continue;
            }
            const int removed =
                std::min(remaining, slot.getNumInStack());
            slot.remove(removed);
            remaining -= removed;
            if (remaining == 0) {
                break;
            }
        }
        if (remaining != 0) {
            return false;
        }
    }
    return !consumed.empty();
}

bool Inventory::consume(
    const std::vector<InventorySlotState> &consumed,
    std::uint64_t expectedRevision)
{
    if (expectedRevision != m_revision || !canConsume(consumed)) {
        return false;
    }
    std::vector<ItemStack> candidate = m_slots;
    for (const InventorySlotState &requirement : consumed) {
        int remaining = requirement.amount;
        for (ItemStack &slot : candidate) {
            if (slot.isEmpty() ||
                slot.getMaterial().id != requirement.materialId) {
                continue;
            }
            const int removed =
                std::min(remaining, slot.getNumInStack());
            slot.remove(removed);
            remaining -= removed;
            if (remaining == 0) {
                break;
            }
        }
    }
    m_slots.swap(candidate);
    ++m_revision;
    return true;
}

bool Inventory::canExchange(
    const std::vector<InventorySlotState> &consumed,
    const Material &produced, int producedAmount) const
{
    if (produced.id == Material::ID::Nothing || producedAmount <= 0) {
        return false;
    }
    std::vector<ItemStack> candidate = m_slots;
    for (const InventorySlotState &requirement : consumed) {
        if (requirement.materialId == Material::ID::Nothing ||
            requirement.amount <= 0) {
            return false;
        }
        int remaining = requirement.amount;
        for (ItemStack &slot : candidate) {
            if (slot.isEmpty() ||
                slot.getMaterial().id != requirement.materialId) {
                continue;
            }
            const int removed =
                std::min(remaining, slot.getNumInStack());
            slot.remove(removed);
            remaining -= removed;
            if (remaining == 0) {
                break;
            }
        }
        if (remaining != 0) {
            return false;
        }
    }

    int remainingOutput = producedAmount;
    for (ItemStack &slot : candidate) {
        if (!slot.isEmpty() && slot.getMaterial().id == produced.id) {
            remainingOutput = slot.add(remainingOutput);
            if (remainingOutput == 0) {
                return true;
            }
        }
    }
    for (ItemStack &slot : candidate) {
        if (slot.isEmpty()) {
            slot = ItemStack(produced, 0);
            remainingOutput = slot.add(remainingOutput);
            if (remainingOutput == 0) {
                return true;
            }
        }
    }
    return false;
}

bool Inventory::exchange(
    const std::vector<InventorySlotState> &consumed,
    const Material &produced, int producedAmount,
    std::uint64_t expectedRevision)
{
    if (expectedRevision != m_revision ||
        !canExchange(consumed, produced, producedAmount)) {
        return false;
    }
    std::vector<ItemStack> candidate = m_slots;
    for (const InventorySlotState &requirement : consumed) {
        int remaining = requirement.amount;
        for (ItemStack &slot : candidate) {
            if (slot.isEmpty() ||
                slot.getMaterial().id != requirement.materialId) {
                continue;
            }
            const int removed =
                std::min(remaining, slot.getNumInStack());
            slot.remove(removed);
            remaining -= removed;
            if (remaining == 0) {
                break;
            }
        }
    }
    int remainingOutput = producedAmount;
    for (ItemStack &slot : candidate) {
        if (!slot.isEmpty() && slot.getMaterial().id == produced.id) {
            remainingOutput = slot.add(remainingOutput);
        }
    }
    for (ItemStack &slot : candidate) {
        if (remainingOutput == 0) {
            break;
        }
        if (slot.isEmpty()) {
            slot = ItemStack(produced, 0);
            remainingOutput = slot.add(remainingOutput);
        }
    }
    if (remainingOutput != 0) {
        return false;
    }
    m_slots.swap(candidate);
    ++m_revision;
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
        state.push_back({slot.getMaterial().id, slot.getNumInStack(),
                         slot.getDurability()});
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
                                     slot.amount, slot.durability);
            }
            else {
                m_slots.emplace_back(Material::NOTHING, 0);
            }
        }
    }

    ensureUsableSlots();
    setSelectedSlot(selectedSlot);
    ++m_revision;
}

void Inventory::selectNextOccupiedSlot()
{
    if (m_slots.empty()) {
        return;
    }
    const int start = m_selectedSlot;
    for (int offset = 1; offset <= static_cast<int>(m_slots.size());
         ++offset) {
        const int candidate =
            (start + offset) % static_cast<int>(m_slots.size());
        if (!m_slots[static_cast<std::size_t>(candidate)].isEmpty()) {
            m_selectedSlot = candidate;
            return;
        }
    }
    m_selectedSlot = start;
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
