#ifndef MOBACTOR_H_INCLUDED
#define MOBACTOR_H_INCLUDED

#include "../Item/Material.h"
#include "LivingActor.h"

class MobActor : public LivingActor {
  public:
    MobActor(ActorId id, std::string type, const glm::vec3 &position);

    void tick(World &world, float dt) override;
    void stepWander(float dt);
    void dropLoot(World &world);

    void setWanderSpeed(float speed);
    void setDrop(Material::ID materialId, int amount);
    Material::ID getDropMaterialId() const;
    int getDropAmount() const;

  private:
    float m_wanderTime = 0.f;
    float m_wanderSpeed = 1.2f;
    Material::ID m_dropMaterialId = Material::ID::Dirt;
    int m_dropAmount = 1;
};

#endif // MOBACTOR_H_INCLUDED
