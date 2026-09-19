#pragma once

#include <algorithm>
#include <cmath>
#include <string>

#include "Actor.h"
#include "EnemyRegistry.h"

namespace EnemyPresentation
{
    // Diagnostic snapshots only. The caller requires capture/performance mode;
    // these never enter the World, combat simulation, collision or save state.
    inline ActorSnapshot gallerySnapshot(const EnemyDefinition& definition,
        const std::string& mode, float seconds)
    {
        ActorSnapshot sample;
        sample.type = definition.type;
        sample.dimensions = definition.dimensions;
        sample.combatant = true;
        sample.combatMode = definition.combat.mode;
        const int windup = std::max(1, definition.combat.windupTicks);
        const int recovery = std::max(1, definition.combat.recoverTicks);
        if (mode == "cycle") {
            // Mirror the world's 20 Hz combat clock, with a one-second rest
            // either side so each species' actual attack timing stays visible.
            const int period = 40 + windup + recovery;
            const double elapsed = std::isfinite(seconds) && seconds > 0.f
                ? static_cast<double>(seconds) : 0.0;
            const int tick = static_cast<int>(std::fmod(
                std::floor(elapsed * 20.0 + 0.00001), period));
            if (tick >= 20 && tick < 20 + windup) {
                sample.combatState = MobCombatState::Windup;
                sample.combatStateTicksTotal = windup;
                sample.combatStateTicksRemaining = 20 + windup - tick;
            }
            else if (tick >= 20 + windup && tick < 20 + windup + recovery) {
                sample.combatState = MobCombatState::Recover;
                sample.combatStateTicksTotal = recovery;
                sample.combatStateTicksRemaining = 20 + windup + recovery - tick;
            }
        }
        else if (mode == "windup" || mode == "recover") {
            sample.combatState = mode == "windup"
                ? MobCombatState::Windup : MobCombatState::Recover;
            sample.combatStateTicksTotal = mode == "windup" ? windup : recovery;
            sample.combatStateTicksRemaining = std::max(1,
                static_cast<int>(std::ceil(sample.combatStateTicksTotal * .15f)));
        }
        else if (mode == "walk") sample.combatState = MobCombatState::Chase;
        return sample;
    }
}
