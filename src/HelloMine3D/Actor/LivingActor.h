#ifndef LIVINGACTOR_H_INCLUDED
#define LIVINGACTOR_H_INCLUDED

#include "Actor.h"

class SandboxEventBus;

class LivingActor : public Actor {
  public:
    static constexpr float DamageInvulnerabilityDuration = 0.5f;

    LivingActor(ActorId id, std::string type, const glm::vec3 &position,
                const glm::vec3 &boxDimensions, float maxHealth);

    void tick(World &world, float dt) override;
    float getHealth() const;
    float getMaxHealth() const;
    float getDamageInvulnerabilityRemaining() const;
    int getHitFeedbackTicksRemaining() const noexcept;
    void setDamageInvulnerabilityRemaining(float seconds);
    ActorSaveState getSaveState() const override;
    ActorSnapshot getSnapshot() const override;
    void applySaveState(const ActorSaveState &state) override;
    bool damage(World &world, float amount,
                ActorId sourceId = InvalidActorId);
    bool damage(SandboxEventBus &eventBus, float amount,
                ActorId sourceId = InvalidActorId);
    float heal(float amount);
    void revive();
    void die(World &world, ActorId killerId = InvalidActorId);
    virtual void die(SandboxEventBus &eventBus,
                     ActorId killerId = InvalidActorId);

  private:
    float m_health = 20.f;
    float m_maxHealth = 20.f;
    float m_damageInvulnerabilityRemaining = 0.f;
    int m_hitFeedbackTicksRemaining = 0;
};

#endif // LIVINGACTOR_H_INCLUDED
