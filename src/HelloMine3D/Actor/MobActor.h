#ifndef MOBACTOR_H_INCLUDED
#define MOBACTOR_H_INCLUDED

#include "../Item/Material.h"
#include "LivingActor.h"

class MobActor : public LivingActor {
  public:
    MobActor(ActorId id, std::string type, const glm::vec3 &position);

    void tick(World &world, float dt) override;
    ActorSaveState getSaveState() const override;
    void applySaveState(const ActorSaveState &state) override;
    void stepWander(float dt);
    void dropLoot(World &world);

    void setChaseTarget(const Entity *target);
    void setWanderSpeed(float speed);
    void setDrop(Material::ID materialId, int amount);
    Material::ID getDropMaterialId() const;
    int getDropAmount() const;
    float getWanderTime() const;
    float getWanderSpeed() const;

  private:
    bool stepChase(const glm::vec3 &targetPosition, float dt);

    static constexpr float ChaseRadius = 12.f;
    static constexpr float ChaseSpeed = 2.4f;
    static constexpr float ChaseStopDistance = 0.75f;

    const Entity *m_chaseTarget = nullptr;
    float m_wanderTime = 0.f;
    float m_wanderSpeed = 1.2f;
    Material::ID m_dropMaterialId = Material::ID::Dirt;
    int m_dropAmount = 1;
};

#endif // MOBACTOR_H_INCLUDED
