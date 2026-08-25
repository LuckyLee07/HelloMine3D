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
        m_damageInvulnerabilityRemaining <= dt
            ? 0.f
            : m_damageInvulnerabilityRemaining - dt;
    if (dt > 0.f && m_hitFeedbackTicksRemaining > 0) {
        --m_hitFeedbackTicksRemaining;
    }
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

int LivingActor::getHitFeedbackTicksRemaining() const noexcept
{
    return m_hitFeedbackTicksRemaining;
}

void LivingActor::setDamageInvulnerabilityRemaining(float seconds)
{
    m_damageInvulnerabilityRemaining = std::max(0.f, seconds);
}

ActorSaveState LivingActor::getSaveState() const
{
    ActorSaveState state = Actor::getSaveState();
    state.health = m_health;
    return state;
}

ActorSnapshot LivingActor::getSnapshot() const
{
    ActorSnapshot snapshot = Actor::getSnapshot();
    snapshot.hitFeedback = std::clamp(
        static_cast<float>(m_hitFeedbackTicksRemaining) / 4.f, 0.f, 1.f);
    return snapshot;
}

void LivingActor::applySaveState(const ActorSaveState &state)
{
    Actor::applySaveState(state);
    m_health = std::clamp(state.health, 0.f, m_maxHealth);
    m_damageInvulnerabilityRemaining = 0.f;
    m_hitFeedbackTicksRemaining = 0;
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
    m_hitFeedbackTicksRemaining = 4;
    eventBus.publish(
        EntityDamageEvent(getId(), sourceId, amount, m_health, position));

    if (m_health <= 0.f) {
        die(eventBus, sourceId);
    }

    return true;
}

float LivingActor::heal(float amount)
{
    if (!isAlive() || amount <= 0.f) {
        return 0.f;
    }

    const float previousHealth = m_health;
    m_health = std::min(m_maxHealth, m_health + amount);
    return m_health - previousHealth;
}

void LivingActor::revive()
{
    setAlive(true);
    m_health = m_maxHealth;
    m_damageInvulnerabilityRemaining = 0.f;
    m_hitFeedbackTicksRemaining = 0;
}

void LivingActor::die(SandboxEventBus &eventBus, ActorId killerId)
{
    if (!isAlive()) {
        return;
    }

    kill();
    m_health = 0.f;
    eventBus.publish(EntityDeathEvent(getId(), killerId, position,
                                      getType()));
}
