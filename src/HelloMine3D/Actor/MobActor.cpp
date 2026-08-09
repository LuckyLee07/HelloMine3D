#include "MobActor.h"

#include <cmath>
#include <utility>

MobActor::MobActor(ActorId id, std::string type,
                   const glm::vec3 &actorPosition)
    : LivingActor(id, std::move(type), actorPosition,
                  glm::vec3(0.35f, 0.9f, 0.35f), 10.f)
{
}

void MobActor::tick(World &world, float dt)
{
    (void)world;

    if (!isAlive()) {
        return;
    }

    stepWander(dt);
}

ActorSaveState MobActor::getSaveState() const
{
    ActorSaveState state = LivingActor::getSaveState();
    state.kind = ActorSaveKind::Mob;
    state.wanderTime = m_wanderTime;
    state.wanderSpeed = m_wanderSpeed;
    state.dropMaterialId = static_cast<int>(m_dropMaterialId);
    state.dropAmount = m_dropAmount;
    return state;
}

void MobActor::applySaveState(const ActorSaveState &state)
{
    LivingActor::applySaveState(state);
    m_wanderTime = state.wanderTime;
    m_wanderSpeed = state.wanderSpeed;
    m_dropMaterialId = static_cast<Material::ID>(state.dropMaterialId);
    m_dropAmount = state.dropAmount;
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

void MobActor::setWanderSpeed(float speed)
{
    m_wanderSpeed = speed;
}

void MobActor::setDrop(Material::ID materialId, int amount)
{
    m_dropMaterialId = materialId;
    m_dropAmount = amount;
}

Material::ID MobActor::getDropMaterialId() const
{
    return m_dropMaterialId;
}

int MobActor::getDropAmount() const
{
    return m_dropAmount;
}

float MobActor::getWanderTime() const
{
    return m_wanderTime;
}

float MobActor::getWanderSpeed() const
{
    return m_wanderSpeed;
}
