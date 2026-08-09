#ifndef ACTORMANAGER_H_INCLUDED
#define ACTORMANAGER_H_INCLUDED

#include <memory>
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

    void tick(World &world, float dt);
    void removeDeadActors();
    std::size_t getActorCount() const;
    std::vector<ActorSnapshot> collectSnapshots() const;
    std::vector<ActorSaveState> collectSaveStates() const;

  private:
    std::vector<std::unique_ptr<Actor>> m_actors;
    ActorId m_nextActorId = DefaultPlayerActorId + 1;
};

#endif // ACTORMANAGER_H_INCLUDED
