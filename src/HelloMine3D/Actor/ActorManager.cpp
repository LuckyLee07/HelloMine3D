#include "ActorManager.h"

#include <algorithm>

ActorId ActorManager::allocateActorId()
{
    return m_nextActorId++;
}

ActorId ActorManager::addActor(std::unique_ptr<Actor> actor,
                               SandboxEventBus &eventBus)
{
    if (!actor) {
        return InvalidActorId;
    }

    const ActorId id = actor->getId();
    actor->enterWorld(eventBus);
    m_actors.push_back(std::move(actor));
    return id;
}

Actor *ActorManager::findActor(ActorId id)
{
    for (auto &actor : m_actors) {
        if (actor && actor->getId() == id) {
            return actor.get();
        }
    }

    return nullptr;
}

const Actor *ActorManager::findActor(ActorId id) const
{
    for (const auto &actor : m_actors) {
        if (actor && actor->getId() == id) {
            return actor.get();
        }
    }

    return nullptr;
}

void ActorManager::tick(World &world, float dt)
{
    for (auto &actor : m_actors) {
        if (actor && actor->isAlive()) {
            actor->tick(world, dt);
        }
    }

    removeDeadActors();
}

void ActorManager::removeDeadActors()
{
    m_actors.erase(
        std::remove_if(m_actors.begin(), m_actors.end(),
                       [](const std::unique_ptr<Actor> &actor) {
                           return !actor || !actor->isAlive();
                       }),
        m_actors.end());
}

std::size_t ActorManager::getActorCount() const
{
    return m_actors.size();
}

std::vector<ActorSnapshot> ActorManager::collectSnapshots() const
{
    std::vector<ActorSnapshot> snapshots;
    snapshots.reserve(m_actors.size());
    for (const auto &actor : m_actors) {
        if (actor && actor->isAlive()) {
            snapshots.push_back(actor->getSnapshot());
        }
    }
    return snapshots;
}
