#include "Actor.h"

#include <utility>

#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"

Actor::Actor(ActorId id, std::string type, const glm::vec3 &actorPosition,
             const glm::vec3 &boxDimensions)
    : Entity(actorPosition, glm::vec3(0.f), boxDimensions)
    , m_id(id)
    , m_type(std::move(type))
{
}

void Actor::enterWorld(SandboxEventBus &eventBus)
{
    setAlive(true);
    eventBus.publish(EntitySpawnEvent(m_id, m_type, position));
}

void Actor::tick(World &world, float dt)
{
    (void)world;
    (void)dt;
}

ActorSaveState Actor::getSaveState() const
{
    ActorSaveState state;
    state.id = m_id;
    state.type = m_type;
    state.position = position;
    state.rotation = rotation;
    state.velocity = velocity;
    state.alive = m_alive;
    return state;
}

void Actor::applySaveState(const ActorSaveState &state)
{
    m_id = state.id;
    m_type = state.type;
    position = state.position;
    rotation = state.rotation;
    velocity = state.velocity;
    m_alive = state.alive;
    box.update(position);
}

ActorId Actor::getId() const
{
    return m_id;
}

const std::string &Actor::getType() const
{
    return m_type;
}

bool Actor::isAlive() const
{
    return m_alive;
}

void Actor::kill()
{
    m_alive = false;
}

void Actor::setAlive(bool alive)
{
    m_alive = alive;
}
