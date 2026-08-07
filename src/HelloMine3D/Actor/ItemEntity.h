#ifndef ITEMENTITY_H_INCLUDED
#define ITEMENTITY_H_INCLUDED

#include "../Item/Material.h"
#include "Actor.h"

class ItemEntity : public Actor {
  public:
    ItemEntity(ActorId id, Material::ID materialId, int amount,
               const glm::vec3 &position);

    void tick(World &world, float dt) override;

    Material::ID getMaterialId() const;
    int getAmount() const;
    void setPickupDelay(float seconds);

  private:
    void updatePhysics(World &world, float dt);
    void tryPickup(World &world);

    Material::ID m_materialId = Material::ID::Nothing;
    int m_amount = 0;
    float m_pickupDelay = 0.5f;
    float m_pickupRadius = 1.2f;
};

#endif // ITEMENTITY_H_INCLUDED
