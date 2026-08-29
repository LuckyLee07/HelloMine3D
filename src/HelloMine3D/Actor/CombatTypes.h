#ifndef COMBATTYPES_H_INCLUDED
#define COMBATTYPES_H_INCLUDED

#include <cstdint>

#include "ActorTypes.h"
#include "../Maths/glm.h"

using CombatProjectileId = std::uint64_t;
constexpr CombatProjectileId InvalidCombatProjectileId = 0;

enum class EnemyCombatMode
{
    Melee = 0,
    Ranged
};

enum class MobCombatState
{
    Idle = 0,
    Chase,
    Windup,
    Recover
};

enum class MobCombatTransitionReason
{
    Spawned = 0,
    TargetAcquired,
    TargetMissing,
    TargetDead,
    TargetOutOfRange,
    AttackRangeReached,
    AttackHit,
    AttackGuarded,
    ProjectileLaunched,
    ProjectileCapacityReached,
    TargetEscaped,
    AttackOccluded,
    TargetRejected,
    RayBudgetExhausted,
    ChaseBudgetExhausted,
    PathBlocked,
    HitInterrupted,
    RecoveryComplete,
    ResonanceInterrupted
};

enum class MobMeleeAttackResult
{
    Hit = 0,
    Guarded,
    TargetMissing,
    TargetDead,
    OutOfRange,
    Occluded,
    TargetRejected,
    RayBudgetExhausted
};

enum class MobRangedAttackResult
{
    Launched = 0,
    CapacityReached,
    TargetMissing,
    TargetDead,
    OutOfRange,
    Occluded,
    TargetRejected,
    RayBudgetExhausted
};

enum class CombatProjectileRemovalReason
{
    None = 0,
    HitPlayer,
    Guarded,
    Blocked,
    LifetimeExpired,
    MaximumDistance,
    OutsideActiveArea,
    OwnerMissing,
    ChunkUnloaded,
    PlayerUnavailable
};

enum class PlayerCombatFeedbackKind
{
    None = 0,
    Damage,
    Guard
};

enum class CombatDirection
{
    None = 0,
    Front,
    Right,
    Back,
    Left
};

struct EnemyCombatProfile
{
    EnemyCombatMode mode = EnemyCombatMode::Melee;
    float attackRange = 0.75f;
    int windupTicks = 6;
    int recoverTicks = 7;
    int cooldownTicks = 16;
    float knockback = 2.f;
    float projectileSpeed = 0.f;
    float projectileDamage = 0.f;
    int projectileLifetimeTicks = 0;
    float projectileMaxDistance = 0.f;
    float projectileRadius = 0.f;
    int projectileWorldLimit = 0;
    int projectileLocalLimit = 0;
    float projectileActiveRadius = 0.f;
};

struct CombatProjectileSnapshot
{
    CombatProjectileId id = InvalidCombatProjectileId;
    ActorId ownerId = InvalidActorId;
    glm::vec3 position{0.f};
    glm::vec3 velocity{0.f};
    float radius = 0.f;
    int ticksRemaining = 0;
    float distanceTravelled = 0.f;
    float maximumDistance = 0.f;
};

inline const char *enemyCombatModeName(EnemyCombatMode mode) noexcept
{
    switch (mode) {
        case EnemyCombatMode::Melee: return "melee";
        case EnemyCombatMode::Ranged: return "ranged";
    }
    return "unknown";
}

inline const char *mobCombatStateName(MobCombatState state) noexcept
{
    switch (state) {
        case MobCombatState::Idle: return "Idle";
        case MobCombatState::Chase: return "Chase";
        case MobCombatState::Windup: return "Windup";
        case MobCombatState::Recover: return "Recover";
    }
    return "Unknown";
}

inline const char *mobCombatTransitionReasonName(
    MobCombatTransitionReason reason) noexcept
{
    switch (reason) {
        case MobCombatTransitionReason::Spawned: return "spawned";
        case MobCombatTransitionReason::TargetAcquired:
            return "target_acquired";
        case MobCombatTransitionReason::TargetMissing: return "target_missing";
        case MobCombatTransitionReason::TargetDead: return "target_dead";
        case MobCombatTransitionReason::TargetOutOfRange:
            return "target_out_of_range";
        case MobCombatTransitionReason::AttackRangeReached:
            return "attack_range_reached";
        case MobCombatTransitionReason::AttackHit: return "attack_hit";
        case MobCombatTransitionReason::AttackGuarded: return "attack_guarded";
        case MobCombatTransitionReason::ProjectileLaunched:
            return "projectile_launched";
        case MobCombatTransitionReason::ProjectileCapacityReached:
            return "projectile_capacity_reached";
        case MobCombatTransitionReason::TargetEscaped: return "target_escaped";
        case MobCombatTransitionReason::AttackOccluded:
            return "attack_occluded";
        case MobCombatTransitionReason::TargetRejected:
            return "target_rejected";
        case MobCombatTransitionReason::RayBudgetExhausted:
            return "ray_budget_exhausted";
        case MobCombatTransitionReason::ChaseBudgetExhausted:
            return "chase_budget_exhausted";
        case MobCombatTransitionReason::PathBlocked: return "path_blocked";
        case MobCombatTransitionReason::HitInterrupted:
            return "hit_interrupted";
        case MobCombatTransitionReason::RecoveryComplete:
            return "recovery_complete";
        case MobCombatTransitionReason::ResonanceInterrupted:
            return "resonance_interrupted";
    }
    return "unknown";
}

inline const char *combatProjectileRemovalReasonName(
    CombatProjectileRemovalReason reason) noexcept
{
    switch (reason) {
        case CombatProjectileRemovalReason::None: return "none";
        case CombatProjectileRemovalReason::HitPlayer: return "hit_player";
        case CombatProjectileRemovalReason::Guarded: return "guarded";
        case CombatProjectileRemovalReason::Blocked: return "blocked";
        case CombatProjectileRemovalReason::LifetimeExpired:
            return "lifetime_expired";
        case CombatProjectileRemovalReason::MaximumDistance:
            return "maximum_distance";
        case CombatProjectileRemovalReason::OutsideActiveArea:
            return "outside_active_area";
        case CombatProjectileRemovalReason::OwnerMissing:
            return "owner_missing";
        case CombatProjectileRemovalReason::ChunkUnloaded:
            return "chunk_unloaded";
        case CombatProjectileRemovalReason::PlayerUnavailable:
            return "player_unavailable";
    }
    return "unknown";
}

inline const char *combatDirectionName(CombatDirection direction) noexcept
{
    switch (direction) {
        case CombatDirection::None: return "None";
        case CombatDirection::Front: return "Front";
        case CombatDirection::Right: return "Right";
        case CombatDirection::Back: return "Back";
        case CombatDirection::Left: return "Left";
    }
    return "Unknown";
}

#endif // COMBATTYPES_H_INCLUDED
