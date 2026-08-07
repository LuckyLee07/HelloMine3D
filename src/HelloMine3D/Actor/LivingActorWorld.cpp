#include "LivingActor.h"

#include <algorithm>

#include "MobActor.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../World/World.h"

bool LivingActor::damage(World &world, float amount, ActorId sourceId)
{
    if (!isAlive() || amount <= 0.f) {
        return false;
    }

    const bool wasAlive = isAlive();
    m_health = std::max(0.f, m_health - amount);
    world.getEventBus().publish(
        EntityDamageEvent(getId(), sourceId, amount, m_health, position));

    if (m_health <= 0.f) {
        die(world, sourceId);
        if (wasAlive && !isAlive()) {
            if (auto *mob = dynamic_cast<MobActor *>(this)) {
                mob->dropLoot(world);
            }
        }
    }

    return true;
}

void LivingActor::die(World &world, ActorId killerId)
{
    die(world.getEventBus(), killerId);
}
