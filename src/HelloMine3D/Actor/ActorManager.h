#ifndef ACTORMANAGER_H_INCLUDED
#define ACTORMANAGER_H_INCLUDED

#include <memory>
#include <functional>
#include <string>
#include <vector>

#include "Actor.h"

class SandboxEventBus;

class ActorManager {
  public:
    ActorId allocateActorId();
    ActorId addActor(std::unique_ptr<Actor> actor, World &world);
    ActorId addActor(std::unique_ptr<Actor> actor, SandboxEventBus &eventBus);

    Actor *findActor(ActorId id);
    const Actor *findActor(ActorId id) const;

    std::size_t tick(World &world, float dt);
    void removeDeadActors();
    std::size_t getActorCount() const;
    std::vector<ActorSnapshot> collectSnapshots() const;
    std::vector<ActorSaveState> collectSaveStates() const;
    std::size_t countActorsByType(const std::string &type) const;
    std::size_t countActorsByTypeNear(const std::string &type,
                                      const glm::vec3 &position,
                                      float radius) const;
    bool hasActorByTypeNear(const std::string &type,
                            const glm::vec3 &position,
                            float radius) const;
    std::size_t removeActorsIf(
        const std::function<bool(const Actor &)> &predicate);

  private:
    struct DeathPresentation
    {
        ActorSnapshot snapshot;
        int ticksRemaining = 0;
    };

    std::vector<std::unique_ptr<Actor>> m_actors;
    std::vector<DeathPresentation> m_deathPresentations;
    ActorId m_nextActorId = DefaultPlayerActorId + 1;
};

#endif // ACTORMANAGER_H_INCLUDED
