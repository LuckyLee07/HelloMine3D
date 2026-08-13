#include "LivingActor.h"

#include <algorithm>
#include <utility>

#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"

LivingActor::LivingActor(ActorId id, std::string type,
                         const glm::vec3 &actorPosition,
                         const glm::vec3 &boxDimensions, float maxHealth)
    : Actor(id, std::move(type), actorPosition, boxDimensions)
    , m_health(maxHealth)
    , m_maxHealth(maxHealth)
{
}

void LivingActor::tick(World &world, float dt)
{
    Actor::tick(world, dt);
    m_damageInvulnerabilityRemaining =
        std::max(0.f, m_damageInvulnerabilityRemaining - dt);
}

float LivingActor::getHealth() const
{
    return m_health;
}

float LivingActor::getMaxHealth() const
{
    return m_maxHealth;
}

float LivingActor::getDamageInvulnerabilityRemaining() const
{
    return m_damageInvulnerabilityRemaining;
}

ActorSaveState LivingActor::getSaveState() const
{
    ActorSaveState state = Actor::getSaveState();
    state.health = m_health;
    return state;
}

void LivingActor::applySaveState(const ActorSaveState &state)
{
    Actor::applySaveState(state);
    m_health = std::clamp(state.health, 0.f, m_maxHealth);
    m_damageInvulnerabilityRemaining = 0.f;
    if (m_health <= 0.f) {
        setAlive(false);
    }
}

bool LivingActor::damage(SandboxEventBus &eventBus, float amount,
                         ActorId sourceId)
{
    if (!isAlive() || amount <= 0.f ||
        m_damageInvulnerabilityRemaining > 0.f) {
        return false;
    }

    m_health = std::max(0.f, m_health - amount);
    m_damageInvulnerabilityRemaining = DamageInvulnerabilityDuration;
    eventBus.publish(
        EntityDamageEvent(getId(), sourceId, amount, m_health, position));

    if (m_health <= 0.f) {
        die(eventBus, sourceId);
    }

    return true;
}

void LivingActor::heal(float amount)
{
    if (!isAlive() || amount <= 0.f) {
        return;
    }

    m_health = std::min(m_maxHealth, m_health + amount);
}

void LivingActor::revive()
{
    setAlive(true);
    m_health = m_maxHealth;
    m_damageInvulnerabilityRemaining = 0.f;
}

void LivingActor::die(SandboxEventBus &eventBus, ActorId killerId)
{
    if (!isAlive()) {
        return;
    }

    kill();
    m_health = 0.f;
    eventBus.publish(EntityDeathEvent(getId(), killerId, position));
}
