#include "MobActor.h"

#include "../World/World.h"

void MobActor::dropLoot(World &world)
{
    if (m_dropMaterialId == Material::ID::Nothing || m_dropAmount <= 0) {
        return;
    }

    world.spawnItemEntity(m_dropMaterialId, m_dropAmount, position);
}
