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
