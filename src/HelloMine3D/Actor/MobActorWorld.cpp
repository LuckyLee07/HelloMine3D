#include "MobActor.h"

#include <cstdint>

#include "../World/World.h"

void MobActor::dropLoot(World &world)
{
    if (m_lootDropped) {
        return;
    }
    m_lootDropped = true;
    for (std::size_t index = 0; index < m_loot.size(); ++index) {
        const EnemyLootDefinition &loot = m_loot[index];
        if (loot.materialId == Material::ID::Nothing ||
            loot.minimumAmount <= 0 ||
            loot.maximumAmount < loot.minimumAmount) {
            continue;
        }
        const int range = loot.maximumAmount - loot.minimumAmount + 1;
        const std::uint32_t selection =
            static_cast<std::uint32_t>(getId()) * 2654435761u +
            static_cast<std::uint32_t>(index) * 2246822519u;
        const int amount = loot.minimumAmount +
            static_cast<int>(selection % static_cast<std::uint32_t>(range));
        world.spawnItemEntity(loot.materialId, amount, position);
    }
}
