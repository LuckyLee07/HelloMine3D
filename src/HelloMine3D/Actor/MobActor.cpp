#include "MobActor.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "../World/World.h"

MobActor::MobActor(ActorId id, std::string type,
                   const glm::vec3 &actorPosition, float maxHealth,
                   const glm::vec3 &dimensions)
    : LivingActor(id, std::move(type), actorPosition,
                  dimensions, maxHealth)
{
}

void MobActor::tick(World &world, float dt)
{
    if (!isAlive()) {
        return;
    }

    LivingActor::tick(world, dt);
    if (dt > 0.f && m_combatCooldownTicksRemaining > 0) {
        --m_combatCooldownTicksRemaining;
    }

    if (m_chaseTarget == nullptr || m_chaseTargetId == InvalidActorId) {
        transitionTo(MobCombatState::Idle,
                     MobCombatTransitionReason::TargetMissing);
        stepWander(dt);
        return;
    }
    if (!world.isCombatTargetAvailable(m_chaseTargetId)) {
        transitionTo(MobCombatState::Idle,
                     MobCombatTransitionReason::TargetDead);
        return;
    }

    const float horizontalOffset = glm::length(glm::vec2(
        m_chaseTarget->position.x - position.x,
        m_chaseTarget->position.z - position.z));
    if (horizontalOffset > m_chaseRadius &&
        m_combatState != MobCombatState::Windup &&
        m_combatState != MobCombatState::Recover) {
        transitionTo(MobCombatState::Idle,
                     MobCombatTransitionReason::TargetOutOfRange);
        stepWander(dt);
        return;
    }

    if (m_combatState == MobCombatState::Idle) {
        transitionTo(MobCombatState::Chase,
                     MobCombatTransitionReason::TargetAcquired);
    }

    switch (m_combatState) {
        case MobCombatState::Idle:
            return;
        case MobCombatState::Chase:
            faceTarget(m_chaseTarget->position);
            if (targetSeparation(*m_chaseTarget) <=
                    m_combat.attackRange + 0.001f &&
                m_combatCooldownTicksRemaining == 0) {
                transitionTo(MobCombatState::Windup,
                             MobCombatTransitionReason::AttackRangeReached,
                             m_combat.windupTicks);
                world.publishCombatWindup(*this, m_chaseTargetId,
                                          m_combat.windupTicks);
                return;
            }
            stepChase(world, *m_chaseTarget, dt);
            return;
        case MobCombatState::Windup:
            faceTarget(m_chaseTarget->position);
            if (dt <= 0.f) {
                return;
            }
            if (m_combatStateTicksRemaining > 0) {
                --m_combatStateTicksRemaining;
            }
            if (m_combatStateTicksRemaining == 0) {
                m_combatCooldownTicksRemaining = m_combat.cooldownTicks;
                if (m_combat.mode == EnemyCombatMode::Ranged) {
                    const MobRangedAttackResult result =
                        world.launchMobProjectile(*this, m_chaseTargetId);
                    transitionTo(MobCombatState::Recover,
                                 reasonForAttackResult(result),
                                 m_combat.recoverTicks);
                }
                else {
                    const MobMeleeAttackResult result =
                        world.resolveMobMeleeAttack(*this, m_chaseTargetId);
                    transitionTo(MobCombatState::Recover,
                                 reasonForAttackResult(result),
                                 m_combat.recoverTicks);
                }
            }
            return;
        case MobCombatState::Recover:
            if (dt <= 0.f) {
                return;
            }
            if (m_combatStateTicksRemaining > 0) {
                --m_combatStateTicksRemaining;
            }
            if (m_combatStateTicksRemaining == 0) {
                transitionTo(MobCombatState::Chase,
                             MobCombatTransitionReason::RecoveryComplete);
            }
            return;
    }
}

bool MobActor::stepChase(World &world, const Entity &target, float dt)
{
    const float offsetX = target.position.x - position.x;
    const float offsetZ = target.position.z - position.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    if (distance > m_chaseRadius) {
        return false;
    }
    if (distance <= 0.f || dt <= 0.f) {
        return true;
    }
    if (!world.tryConsumeCombatChaseStep()) {
        m_lastCombatTransitionReason =
            MobCombatTransitionReason::ChaseBudgetExhausted;
        return true;
    }

    const float stopDistance = std::max(0.f,
        std::max(box.dimensions.x, box.dimensions.z) +
        std::max(target.box.dimensions.x, target.box.dimensions.z) +
        m_combat.attackRange - 0.01f);
    if (distance <= stopDistance) {
        return true;
    }

    const float travel =
        std::min(m_chaseSpeed * dt, distance - stopDistance);
    glm::vec3 candidate = position;
    candidate.x += offsetX / distance * travel;
    candidate.z += offsetZ / distance * travel;
    if (!world.canOccupyCombatPosition(*this, candidate)) {
        m_lastCombatTransitionReason = MobCombatTransitionReason::PathBlocked;
        return true;
    }
    position = candidate;
    box.update(position);
    return true;
}

float MobActor::targetSeparation(const Entity &target) const
{
    const glm::vec3 separation = glm::max(
        glm::abs(target.position - position) -
            target.box.dimensions - box.dimensions,
        glm::vec3(0.f));
    return glm::length(separation);
}

void MobActor::faceTarget(const glm::vec3 &targetPosition)
{
    const float offsetX = targetPosition.x - position.x;
    const float offsetZ = targetPosition.z - position.z;
    if (std::abs(offsetX) <= 0.000001f &&
        std::abs(offsetZ) <= 0.000001f) {
        return;
    }
    rotation.y = glm::degrees(std::atan2(offsetX, -offsetZ));
}

void MobActor::transitionTo(MobCombatState state,
                            MobCombatTransitionReason reason,
                            int ticks)
{
    m_combatState = state;
    m_lastCombatTransitionReason = reason;
    m_combatStateTicksRemaining = std::max(0, ticks);
    m_combatStateTicksTotal = std::max(0, ticks);
}

MobCombatTransitionReason MobActor::reasonForAttackResult(
    MobMeleeAttackResult result) noexcept
{
    switch (result) {
        case MobMeleeAttackResult::Hit:
            return MobCombatTransitionReason::AttackHit;
        case MobMeleeAttackResult::Guarded:
            return MobCombatTransitionReason::AttackGuarded;
        case MobMeleeAttackResult::TargetMissing:
            return MobCombatTransitionReason::TargetMissing;
        case MobMeleeAttackResult::TargetDead:
            return MobCombatTransitionReason::TargetDead;
        case MobMeleeAttackResult::OutOfRange:
            return MobCombatTransitionReason::TargetEscaped;
        case MobMeleeAttackResult::Occluded:
            return MobCombatTransitionReason::AttackOccluded;
        case MobMeleeAttackResult::TargetRejected:
            return MobCombatTransitionReason::TargetRejected;
        case MobMeleeAttackResult::RayBudgetExhausted:
            return MobCombatTransitionReason::RayBudgetExhausted;
    }
    return MobCombatTransitionReason::TargetRejected;
}

MobCombatTransitionReason MobActor::reasonForAttackResult(
    MobRangedAttackResult result) noexcept
{
    switch (result) {
        case MobRangedAttackResult::Launched:
            return MobCombatTransitionReason::ProjectileLaunched;
        case MobRangedAttackResult::CapacityReached:
            return MobCombatTransitionReason::ProjectileCapacityReached;
        case MobRangedAttackResult::TargetMissing:
            return MobCombatTransitionReason::TargetMissing;
        case MobRangedAttackResult::TargetDead:
            return MobCombatTransitionReason::TargetDead;
        case MobRangedAttackResult::OutOfRange:
            return MobCombatTransitionReason::TargetEscaped;
        case MobRangedAttackResult::Occluded:
            return MobCombatTransitionReason::AttackOccluded;
        case MobRangedAttackResult::TargetRejected:
            return MobCombatTransitionReason::TargetRejected;
        case MobRangedAttackResult::RayBudgetExhausted:
            return MobCombatTransitionReason::RayBudgetExhausted;
    }
    return MobCombatTransitionReason::TargetRejected;
}

ActorSaveState MobActor::getSaveState() const
{
    ActorSaveState state = LivingActor::getSaveState();
    state.kind = ActorSaveKind::Mob;
    state.wanderTime = m_wanderTime;
    state.wanderSpeed = m_wanderSpeed;
    if (!m_loot.empty()) {
        state.dropMaterialId = static_cast<int>(m_loot.front().materialId);
        state.dropAmount = m_loot.front().minimumAmount;
    }
    return state;
}

ActorSnapshot MobActor::getSnapshot() const
{
    ActorSnapshot snapshot = LivingActor::getSnapshot();
    snapshot.combatant = true;
    snapshot.combatMode = m_combat.mode;
    snapshot.combatState = m_combatState;
    snapshot.combatTargetId = m_chaseTargetId;
    snapshot.combatStateTicksRemaining = m_combatStateTicksRemaining;
    snapshot.combatStateTicksTotal = m_combatStateTicksTotal;
    snapshot.combatTransitionReason = m_lastCombatTransitionReason;
    return snapshot;
}

void MobActor::applySaveState(const ActorSaveState &state)
{
    LivingActor::applySaveState(state);
    m_wanderTime = state.wanderTime;
    m_wanderSpeed = state.wanderSpeed;
    if (!m_definitionBacked) {
        setDrop(static_cast<Material::ID>(state.dropMaterialId),
                state.dropAmount);
    }
    m_lootDropped = false;
    m_combatState = MobCombatState::Idle;
    m_lastCombatTransitionReason = MobCombatTransitionReason::Spawned;
    m_combatStateTicksRemaining = 0;
    m_combatStateTicksTotal = 0;
    m_combatCooldownTicksRemaining = 0;
}

void MobActor::stepWander(float dt)
{
    if (!isAlive() || dt <= 0.f) {
        return;
    }

    m_wanderTime += dt;
    position.x += std::cos(m_wanderTime * 0.7f) * m_wanderSpeed * dt;
    position.z += std::sin(m_wanderTime * 0.5f) * m_wanderSpeed * dt;
    box.update(position);
}

void MobActor::setChaseTarget(const Entity *target, ActorId targetId)
{
    m_chaseTarget = target;
    m_chaseTargetId = target != nullptr ? targetId : InvalidActorId;
    if (target == nullptr) {
        transitionTo(MobCombatState::Idle,
                     MobCombatTransitionReason::TargetMissing);
    }
}

void MobActor::setWanderSpeed(float speed)
{
    m_wanderSpeed = speed;
}

void MobActor::setDrop(Material::ID materialId, int amount)
{
    m_loot.clear();
    if (materialId != Material::ID::Nothing && amount > 0) {
        m_loot.push_back({materialId, amount, amount});
    }
    m_definitionBacked = false;
    m_lootDropped = false;
}

Material::ID MobActor::getDropMaterialId() const
{
    return m_loot.empty() ? Material::ID::Nothing
                          : m_loot.front().materialId;
}

int MobActor::getDropAmount() const
{
    return m_loot.empty() ? 0 : m_loot.front().minimumAmount;
}

float MobActor::getWanderTime() const
{
    return m_wanderTime;
}

float MobActor::getWanderSpeed() const
{
    return m_wanderSpeed;
}

void MobActor::applyDefinition(const EnemyDefinition &definition)
{
    m_wanderSpeed = definition.wanderSpeed;
    m_chaseRadius = definition.chaseRadius;
    m_chaseSpeed = definition.chaseSpeed;
    m_contactDamage = definition.contactDamage;
    m_combat = definition.combat;
    m_loot = definition.loot;
    m_definitionBacked = true;
    m_lootDropped = false;
}

float MobActor::getChaseRadius() const
{
    return m_chaseRadius;
}

float MobActor::getChaseSpeed() const
{
    return m_chaseSpeed;
}

float MobActor::getContactDamage() const
{
    return m_contactDamage;
}

const EnemyCombatProfile &MobActor::getCombatProfile() const noexcept
{
    return m_combat;
}

MobCombatState MobActor::getCombatState() const noexcept
{
    return m_combatState;
}

ActorId MobActor::getCombatTargetId() const noexcept
{
    return m_chaseTargetId;
}

int MobActor::getCombatStateTicksRemaining() const noexcept
{
    return m_combatStateTicksRemaining;
}

int MobActor::getCombatCooldownTicksRemaining() const noexcept
{
    return m_combatCooldownTicksRemaining;
}

MobCombatTransitionReason MobActor::getLastCombatTransitionReason() const noexcept
{
    return m_lastCombatTransitionReason;
}

void MobActor::interruptByPlayerHit(World &world,
                                    const glm::vec3 &sourcePosition,
                                    float knockbackDistance,
                                    int recoverTicks)
{
    if (!isAlive()) {
        return;
    }

    glm::vec2 direction(position.x - sourcePosition.x,
                        position.z - sourcePosition.z);
    const float length = glm::length(direction);
    if (length > 0.000001f && knockbackDistance > 0.f) {
        direction /= length;
        glm::vec3 candidate = position;
        candidate.x += direction.x * knockbackDistance;
        candidate.z += direction.y * knockbackDistance;
        if (world.canOccupyCombatPosition(*this, candidate)) {
            position = candidate;
            box.update(position);
        }
    }
    const int boundedRecover = std::max(1, recoverTicks);
    m_combatCooldownTicksRemaining = std::max(
        m_combatCooldownTicksRemaining, boundedRecover);
    transitionTo(MobCombatState::Recover,
                 MobCombatTransitionReason::HitInterrupted,
                 boundedRecover);
}

const std::vector<EnemyLootDefinition> &MobActor::getLootTable() const
{
    return m_loot;
}
