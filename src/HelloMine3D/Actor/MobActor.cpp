#include "MobActor.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
    if (m_chaseTarget != nullptr && stepChase(m_chaseTarget->position, dt)) {
        return;
    }

    stepWander(dt);
}

bool MobActor::stepChase(const glm::vec3 &targetPosition, float dt)
{
    const float offsetX = targetPosition.x - position.x;
    const float offsetZ = targetPosition.z - position.z;
    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    if (distance > m_chaseRadius) {
        return false;
    }
    if (distance <= ChaseStopDistance || distance <= 0.f || dt <= 0.f) {
        return true;
    }

    const float travel =
        std::min(m_chaseSpeed * dt, distance - ChaseStopDistance);
    position.x += offsetX / distance * travel;
    position.z += offsetZ / distance * travel;
    box.update(position);
    return true;
}

ActorSaveState MobActor::getSaveState() const
{
    ActorSaveState state = LivingActor::getSaveState();
    state.kind = ActorSaveKind::Mob;
    state.wanderTime = m_wanderTime;
    state.wanderSpeed = m_wanderSpeed;
    if (!m_loot.empty()) {
        state.dropMaterialId =
            static_cast<int>(m_loot.front().materialId);
        state.dropAmount = m_loot.front().minimumAmount;
    }
    return state;
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
}

void MobActor::stepWander(float dt)
{
    if (!isAlive()) {
        return;
    }

    m_wanderTime += dt;
    position.x += std::cos(m_wanderTime * 0.7f) * m_wanderSpeed * dt;
    position.z += std::sin(m_wanderTime * 0.5f) * m_wanderSpeed * dt;
    box.update(position);
}

void MobActor::setChaseTarget(const Entity *target)
{
    m_chaseTarget = target;
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

const std::vector<EnemyLootDefinition> &MobActor::getLootTable() const
{
    return m_loot;
}
