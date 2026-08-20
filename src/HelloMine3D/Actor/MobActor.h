#ifndef MOBACTOR_H_INCLUDED
#define MOBACTOR_H_INCLUDED

#include "EnemyRegistry.h"
#include "../Item/Material.h"
#include "LivingActor.h"

class MobActor : public LivingActor {
  public:
    MobActor(ActorId id, std::string type, const glm::vec3 &position,
             float maxHealth = 10.f,
             const glm::vec3 &dimensions = {0.35f, 0.9f, 0.35f});

    void tick(World &world, float dt) override;
    ActorSaveState getSaveState() const override;
    void applySaveState(const ActorSaveState &state) override;
    void stepWander(float dt);
    void dropLoot(World &world);
    void applyDefinition(const EnemyDefinition &definition);

    void setChaseTarget(const Entity *target);
    void setWanderSpeed(float speed);
    void setDrop(Material::ID materialId, int amount);
    Material::ID getDropMaterialId() const;
    int getDropAmount() const;
    float getWanderTime() const;
    float getWanderSpeed() const;
    float getChaseRadius() const;
    float getChaseSpeed() const;
    float getContactDamage() const;
    const std::vector<EnemyLootDefinition> &getLootTable() const;

  private:
    bool stepChase(const glm::vec3 &targetPosition, float dt);

    static constexpr float ChaseStopDistance = 0.55f;

    const Entity *m_chaseTarget = nullptr;
    float m_wanderTime = 0.f;
    float m_wanderSpeed = 1.2f;
    float m_chaseRadius = 12.f;
    float m_chaseSpeed = 2.4f;
    float m_contactDamage = 2.f;
    std::vector<EnemyLootDefinition> m_loot{{Material::ID::Dirt, 1, 1}};
    bool m_definitionBacked = false;
    bool m_lootDropped = false;
};

#endif // MOBACTOR_H_INCLUDED
