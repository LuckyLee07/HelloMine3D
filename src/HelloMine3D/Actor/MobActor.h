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
    ActorSnapshot getSnapshot() const override;
    void applySaveState(const ActorSaveState &state) override;
    void stepWander(float dt);
    void dropLoot(World &world);
    void applyDefinition(const EnemyDefinition &definition);

    void setChaseTarget(const Entity *target,
                        ActorId targetId = DefaultPlayerActorId);
    void setWanderSpeed(float speed);
    void setDrop(Material::ID materialId, int amount);
    Material::ID getDropMaterialId() const;
    int getDropAmount() const;
    float getWanderTime() const;
    float getWanderSpeed() const;
    float getChaseRadius() const;
    float getChaseSpeed() const;
    float getContactDamage() const;
    const EnemyCombatProfile &getCombatProfile() const noexcept;
    MobCombatState getCombatState() const noexcept;
    ActorId getCombatTargetId() const noexcept;
    int getCombatStateTicksRemaining() const noexcept;
    int getCombatCooldownTicksRemaining() const noexcept;
    MobCombatTransitionReason getLastCombatTransitionReason() const noexcept;
    void interruptByPlayerHit(World &world,
                              const glm::vec3 &sourcePosition,
                              float knockbackDistance,
                              int recoverTicks);
    const std::vector<EnemyLootDefinition> &getLootTable() const;

  private:
    bool stepChase(World &world, const Entity &target, float dt);
    float targetSeparation(const Entity &target) const;
    void faceTarget(const glm::vec3 &targetPosition);
    void transitionTo(MobCombatState state,
                      MobCombatTransitionReason reason,
                      int ticks = 0);
    static MobCombatTransitionReason reasonForAttackResult(
        MobMeleeAttackResult result) noexcept;
    static MobCombatTransitionReason reasonForAttackResult(
        MobRangedAttackResult result) noexcept;

    const Entity *m_chaseTarget = nullptr;
    ActorId m_chaseTargetId = InvalidActorId;
    float m_wanderTime = 0.f;
    float m_wanderSpeed = 1.2f;
    float m_chaseRadius = 12.f;
    float m_chaseSpeed = 2.4f;
    float m_contactDamage = 2.f;
    EnemyCombatProfile m_combat;
    MobCombatState m_combatState = MobCombatState::Idle;
    MobCombatTransitionReason m_lastCombatTransitionReason =
        MobCombatTransitionReason::Spawned;
    int m_combatStateTicksRemaining = 0;
    int m_combatStateTicksTotal = 0;
    int m_combatCooldownTicksRemaining = 0;
    std::vector<EnemyLootDefinition> m_loot{{Material::ID::Dirt, 1, 1}};
    bool m_definitionBacked = false;
    bool m_lootDropped = false;
};

#endif // MOBACTOR_H_INCLUDED
