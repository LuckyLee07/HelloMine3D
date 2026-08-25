// Minimal World boundary used by the actor-only lifecycle executable.
// The full behavior is covered by WorldRuntimeSmoke; this target intentionally
// links no chunk, player, save or gameplay runtime implementation.

#include "../Actor/MobActor.h"
#include "../World/World.h"

bool World::isCombatTargetAvailable(ActorId) const noexcept
{
    return false;
}

bool World::tryConsumeCombatChaseStep() noexcept
{
    return true;
}

bool World::canOccupyCombatPosition(const MobActor &,
                                    const glm::vec3 &)
{
    return true;
}

void World::publishCombatWindup(const MobActor &, ActorId, int)
{
}

MobMeleeAttackResult World::resolveMobMeleeAttack(const MobActor &,
                                                  ActorId)
{
    return MobMeleeAttackResult::TargetMissing;
}

MobRangedAttackResult World::launchMobProjectile(const MobActor &,
                                                 ActorId)
{
    return MobRangedAttackResult::TargetMissing;
}
