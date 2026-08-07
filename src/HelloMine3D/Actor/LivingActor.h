#ifndef LIVINGACTOR_H_INCLUDED
#define LIVINGACTOR_H_INCLUDED

#include "Actor.h"

class SandboxEventBus;

class LivingActor : public Actor {
  public:
    LivingActor(ActorId id, std::string type, const glm::vec3 &position,
                const glm::vec3 &boxDimensions, float maxHealth);

    float getHealth() const;
    float getMaxHealth() const;
    bool damage(World &world, float amount,
                ActorId sourceId = InvalidActorId);
    bool damage(SandboxEventBus &eventBus, float amount,
                ActorId sourceId = InvalidActorId);
    void heal(float amount);
    void die(World &world, ActorId killerId = InvalidActorId);
    virtual void die(SandboxEventBus &eventBus,
                     ActorId killerId = InvalidActorId);

  private:
    float m_health = 20.f;
    float m_maxHealth = 20.f;
};

#endif // LIVINGACTOR_H_INCLUDED
