#include "ActorManager.h"

#include <algorithm>
#include <cmath>

#include "EnemyPresentation.h"

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
    if (id == InvalidActorId || findActor(id) != nullptr) {
        return InvalidActorId;
    }
    m_nextActorId = std::max(m_nextActorId, id + 1);
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

std::size_t ActorManager::tick(World &world, float dt)
{
    if (dt > 0.f) {
        for (DeathPresentation &presentation : m_deathPresentations) {
            --presentation.ticksRemaining;
        }
        m_deathPresentations.erase(
            std::remove_if(
                m_deathPresentations.begin(),
                m_deathPresentations.end(),
                [](const DeathPresentation &presentation) {
                    return presentation.ticksRemaining <= 0;
                }),
            m_deathPresentations.end());
    }

    std::size_t processed = 0;
    for (auto &actor : m_actors) {
        if (actor && actor->isAlive()) {
            actor->tick(world, dt);
            ++processed;
        }
    }

    removeDeadActors();
    return processed;
}

void ActorManager::removeDeadActors()
{
    for (auto iterator = m_actors.begin(); iterator != m_actors.end();) {
        if (*iterator && !(*iterator)->isAlive()) {
            ActorSnapshot snapshot = (*iterator)->getSnapshot();
            if (snapshot.combatant) {
                if (m_deathPresentations.size() >=
                    EnemyPresentation::MaximumDeathPoses) {
                    m_deathPresentations.erase(
                        m_deathPresentations.begin());
                }
                snapshot.deathPresentation = true;
                snapshot.deathPresentationTicksRemaining =
                    EnemyPresentation::DeathPoseTicks;
                snapshot.deathPresentationTicksTotal =
                    EnemyPresentation::DeathPoseTicks;
                m_deathPresentations.push_back({
                    std::move(snapshot),
                    EnemyPresentation::DeathPoseTicks});
            }
            iterator = m_actors.erase(iterator);
        }
        else if (!*iterator) {
            iterator = m_actors.erase(iterator);
        }
        else {
            ++iterator;
        }
    }
}

std::size_t ActorManager::getActorCount() const
{
    return m_actors.size();
}

std::vector<ActorSnapshot> ActorManager::collectSnapshots() const
{
    std::vector<ActorSnapshot> snapshots;
    snapshots.reserve(m_actors.size() + m_deathPresentations.size());
    for (const auto &actor : m_actors) {
        if (actor && actor->isAlive()) {
            snapshots.push_back(actor->getSnapshot());
        }
    }
    for (const DeathPresentation &presentation : m_deathPresentations) {
        ActorSnapshot snapshot = presentation.snapshot;
        snapshot.deathPresentationTicksRemaining =
            presentation.ticksRemaining;
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

std::vector<ActorSaveState> ActorManager::collectSaveStates() const
{
    std::vector<ActorSaveState> states;
    states.reserve(m_actors.size());
    for (const auto &actor : m_actors) {
        if (actor && actor->isAlive()) {
            states.push_back(actor->getSaveState());
        }
    }
    return states;
}

std::size_t ActorManager::countActorsByType(const std::string &type) const
{
    return static_cast<std::size_t>(std::count_if(
        m_actors.begin(), m_actors.end(),
        [&type](const std::unique_ptr<Actor> &actor) {
            return actor && actor->isAlive() && actor->getType() == type;
        }));
}

std::size_t ActorManager::countActorsByTypeNear(
    const std::string &type, const glm::vec3 &position, float radius) const
{
    const float radiusSquared = std::max(0.f, radius) *
                                std::max(0.f, radius);
    return static_cast<std::size_t>(std::count_if(
        m_actors.begin(), m_actors.end(),
        [&type, &position, radiusSquared](
            const std::unique_ptr<Actor> &actor) {
            if (!actor || !actor->isAlive() || actor->getType() != type) {
                return false;
            }
            const float dx = actor->position.x - position.x;
            const float dz = actor->position.z - position.z;
            return dx * dx + dz * dz <= radiusSquared;
        }));
}

bool ActorManager::hasActorByTypeNear(const std::string &type,
                                      const glm::vec3 &position,
                                      float radius) const
{
    return countActorsByTypeNear(type, position, radius) > 0;
}

std::size_t ActorManager::removeActorsIf(
    const std::function<bool(const Actor &)> &predicate)
{
    const std::size_t before = m_actors.size();
    m_actors.erase(
        std::remove_if(m_actors.begin(), m_actors.end(),
                       [&predicate](const std::unique_ptr<Actor> &actor) {
                           return !actor || predicate(*actor);
                       }),
        m_actors.end());
    return before - m_actors.size();
}
