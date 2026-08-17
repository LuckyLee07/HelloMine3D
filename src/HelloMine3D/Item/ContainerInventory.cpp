#include "ContainerInventory.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace {
bool parseInt(const std::string &text, int &value)
{
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}
} // namespace

ContainerInventory::ContainerInventory(int slotCount)
    : m_slots(static_cast<std::size_t>(
          std::max(1, std::min(MaxSlots, slotCount))))
{
}

int ContainerInventory::getSlotCount() const noexcept
{
    return static_cast<int>(m_slots.size());
}

const InventorySlotState &ContainerInventory::getSlot(int index) const
{
    static const InventorySlotState empty;
    if (index < 0 || index >= getSlotCount()) {
        return empty;
    }
    return m_slots[static_cast<std::size_t>(index)];
}

int ContainerInventory::addItem(const Material &material, int amount)
{
    if (material.id == Material::ID::Nothing || material.isTool ||
        amount <= 0) {
        return 0;
    }

    int remaining = amount;
    for (InventorySlotState &slot : m_slots) {
        if (slot.materialId != material.id || slot.amount <= 0) {
            continue;
        }
        const int added =
            std::min(remaining, material.maxStackSize - slot.amount);
        slot.amount += added;
        remaining -= added;
        if (remaining == 0) {
            return amount;
        }
    }
    for (InventorySlotState &slot : m_slots) {
        if (slot.amount > 0) {
            continue;
        }
        const int added = std::min(remaining, material.maxStackSize);
        slot.materialId = material.id;
        slot.amount = added;
        remaining -= added;
        if (remaining == 0) {
            break;
        }
    }
    return amount - remaining;
}

int ContainerInventory::removeFromSlot(int index, int amount)
{
    if (index < 0 || index >= getSlotCount() || amount <= 0) {
        return 0;
    }
    InventorySlotState &slot = m_slots[static_cast<std::size_t>(index)];
    const int removed = std::min(amount, slot.amount);
    slot.amount -= removed;
    if (slot.amount == 0) {
        slot.materialId = Material::ID::Nothing;
    }
    return removed;
}

int ContainerInventory::count(Material::ID materialId) const noexcept
{
    int total = 0;
    for (const InventorySlotState &slot : m_slots) {
        if (slot.materialId == materialId) {
            total += slot.amount;
        }
    }
    return total;
}

std::string ContainerInventory::serialize() const
{
    std::ostringstream output;
    output << "v1|" << m_slots.size() << '|';
    for (std::size_t index = 0; index < m_slots.size(); ++index) {
        if (index > 0) {
            output << ';';
        }
        output << static_cast<int>(m_slots[index].materialId) << ','
               << m_slots[index].amount;
    }
    return output.str();
}

bool ContainerInventory::deserialize(const std::string &payload,
                                     ContainerInventory &inventory,
                                     std::string *error)
{
    auto reject = [error](const std::string &reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };

    const std::size_t first = payload.find('|');
    const std::size_t second = payload.find('|', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        payload.substr(0, first) != "v1") {
        return reject("unsupported container payload version");
    }

    int slotCount = 0;
    if (!parseInt(payload.substr(first + 1, second - first - 1), slotCount) ||
        slotCount <= 0 || slotCount > MaxSlots) {
        return reject("invalid container slot count");
    }

    ContainerInventory parsed(slotCount);
    std::istringstream entries(payload.substr(second + 1));
    std::string entry;
    int index = 0;
    while (std::getline(entries, entry, ';')) {
        if (index >= slotCount) {
            return reject("too many container slots");
        }
        const std::size_t comma = entry.find(',');
        int materialValue = 0;
        int amount = 0;
        if (comma == std::string::npos ||
            !parseInt(entry.substr(0, comma), materialValue) ||
            !parseInt(entry.substr(comma + 1), amount) ||
            materialValue < static_cast<int>(Material::ID::Nothing) ||
            materialValue >= static_cast<int>(Material::ID::Count) ||
            amount < 0) {
            return reject("invalid container slot");
        }

        const auto materialId = static_cast<Material::ID>(materialValue);
        const Material &material = Material::toMaterial(materialId);
        if ((amount == 0 && materialId != Material::ID::Nothing) ||
            (amount > 0 &&
             (materialId == Material::ID::Nothing ||
              amount > material.maxStackSize || material.isTool))) {
            return reject("container stack violates material limits");
        }
        parsed.m_slots[static_cast<std::size_t>(index)] =
            {materialId, amount};
        ++index;
    }
    if (index != slotCount) {
        return reject("container slot count does not match payload");
    }

    inventory = std::move(parsed);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}
