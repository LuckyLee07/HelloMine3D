#ifndef COMBATTYPES_H_INCLUDED
#define COMBATTYPES_H_INCLUDED

#include "ActorTypes.h"

enum class EnemyCombatMode
{
    Melee = 0
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
    TargetEscaped,
    AttackOccluded,
    TargetRejected,
    RayBudgetExhausted,
    ChaseBudgetExhausted,
    PathBlocked,
    HitInterrupted,
    RecoveryComplete
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
};

inline const char *enemyCombatModeName(EnemyCombatMode mode) noexcept
{
    return mode == EnemyCombatMode::Melee ? "melee" : "unknown";
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
