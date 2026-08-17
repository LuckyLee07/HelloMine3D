#include "ItemStack.h"

#include "ToolRegistry.h"

#include <algorithm>

ItemStack::ItemStack(const Material &material, int amount, int durability)
    : m_pMaterial(&material)
    , m_numInStack(std::max(0, std::min(amount, material.maxStackSize)))
{
    const ToolDefinition *tool = runtimeToolRegistry().find(material.id);
    if (m_numInStack > 0 && tool != nullptr) {
        m_durability = durability > 0
                           ? std::min(durability, tool->maxDurability)
                           : tool->maxDurability;
    }
}

int ItemStack::add(int amount)
{
    m_numInStack += amount;

    if (m_numInStack > 0 && m_pMaterial->isTool && m_durability <= 0) {
        const ToolDefinition *tool =
            runtimeToolRegistry().find(m_pMaterial->id);
        m_durability = tool != nullptr ? tool->maxDurability : 0;
    }

    if (m_numInStack > m_pMaterial->maxStackSize) {
        int leftOver = m_numInStack - m_pMaterial->maxStackSize;
        m_numInStack = m_pMaterial->maxStackSize;
        return leftOver;
    }
    else {
        return 0;
    }
}

void ItemStack::remove()
{
    remove(1);
}

void ItemStack::remove(int amount)
{
    if (amount <= 0) {
        return;
    }

    m_numInStack -= amount;
    if (m_numInStack <= 0) {
        m_numInStack = 0;
        m_pMaterial = &Material::NOTHING;
        m_durability = 0;
    }
}

bool ItemStack::damage(int amount)
{
    if (amount <= 0 || !isDamageable()) {
        return false;
    }
    m_durability -= amount;
    if (m_durability > 0) {
        return false;
    }
    m_numInStack = 0;
    m_pMaterial = &Material::NOTHING;
    m_durability = 0;
    return true;
}

int ItemStack::getNumInStack() const
{
    return m_numInStack;
}

int ItemStack::getDurability() const
{
    return m_durability;
}

int ItemStack::getMaxDurability() const
{
    const ToolDefinition *tool =
        runtimeToolRegistry().find(m_pMaterial->id);
    return tool != nullptr ? tool->maxDurability : 0;
}

bool ItemStack::isDamageable() const
{
    return !isEmpty() && m_pMaterial->isTool &&
           runtimeToolRegistry().find(m_pMaterial->id) != nullptr;
}

const Material &ItemStack::getMaterial() const
{
    return *m_pMaterial;
}

bool ItemStack::isEmpty() const
{
    return m_pMaterial->id == Material::ID::Nothing || m_numInStack <= 0;
}
