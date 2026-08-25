#include "EnemyRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"

namespace
{
    constexpr const char *EnemyHeader =
        "# HelloMine3D enemy registry v3";

    struct PendingEnemy
    {
        EnemyDefinition definition;
        std::set<std::string> fields;
        std::size_t startLine = 0;
    };

    std::string trim(const std::string &value)
    {
        const std::size_t begin = value.find_first_not_of(" \t\r");
        if (begin == std::string::npos) {
            return {};
        }
        const std::size_t end = value.find_last_not_of(" \t\r");
        return value.substr(begin, end - begin + 1);
    }

    std::vector<std::string> tokens(const std::string &line)
    {
        std::istringstream input(line);
        std::vector<std::string> result;
        std::string token;
        while (input >> token) {
            result.push_back(token);
        }
        return result;
    }

    [[noreturn]] void fail(const std::string &source, std::size_t line,
                           const std::string &message)
    {
        throw std::runtime_error(
            "Invalid enemy source '" + source + "' at line " +
            std::to_string(line) + ": " + message);
    }

    bool canonicalType(const std::string &value)
    {
        if (value.empty() || value.size() > EnemyRegistry::MaxTypeLength) {
            return false;
        }
        const std::size_t separator = value.find(':');
        if (separator == std::string::npos || separator == 0 ||
            separator + 1 >= value.size() ||
            value.find(':', separator + 1) != std::string::npos) {
            return false;
        }
        const auto validCharacter = [](unsigned char character) {
            return (character >= 'a' && character <= 'z') ||
                   (character >= '0' && character <= '9') ||
                   character == '.' || character == '_' ||
                   character == '-';
        };
        return std::all_of(value.begin(), value.begin() + separator,
                           validCharacter) &&
               std::all_of(value.begin() + separator + 1, value.end(),
                           validCharacter);
    }

    int integerValue(const std::string &text, int minimum, int maximum,
                     const std::string &source, std::size_t line,
                     const std::string &field)
    {
        try {
            std::size_t consumed = 0;
            const int value = std::stoi(text, &consumed);
            if (consumed == text.size() && value >= minimum &&
                value <= maximum) {
                return value;
            }
        }
        catch (const std::exception &) {
        }
        fail(source, line,
             field + " must be an integer in [" +
                 std::to_string(minimum) + ", " +
                 std::to_string(maximum) + "].");
    }

    float floatValue(const std::string &text, float minimum, float maximum,
                     const std::string &source, std::size_t line,
                     const std::string &field)
    {
        try {
            std::size_t consumed = 0;
            const float value = std::stof(text, &consumed);
            if (consumed == text.size() && std::isfinite(value) &&
                value >= minimum && value <= maximum) {
                return value;
            }
        }
        catch (const std::exception &) {
        }
        fail(source, line,
             field + " must be a finite number in [" +
                 std::to_string(minimum) + ", " +
                 std::to_string(maximum) + "].");
    }

    void claim(PendingEnemy &pending, const std::string &field,
               const std::string &source, std::size_t line)
    {
        if (!pending.fields.insert(field).second) {
            fail(source, line, field + " is duplicated.");
        }
    }

    std::vector<EnemyDefinition> parseSource(const EnemySource &source)
    {
        std::istringstream input(source.content);
        std::vector<EnemyDefinition> result;
        PendingEnemy pending;
        bool headerSeen = false;
        bool insideEnemy = false;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
            line = trim(line);
            if (!headerSeen) {
                if (line != EnemyHeader) {
                    fail(source.name, lineNumber,
                         "unsupported or missing header.");
                }
                headerSeen = true;
                continue;
            }
            if (line.empty() || line.front() == '#') {
                continue;
            }
            const std::vector<std::string> parts = tokens(line);
            if (!insideEnemy) {
                if (parts.size() != 2 || parts[0] != "enemy" ||
                    !canonicalType(parts[1])) {
                    fail(source.name, lineNumber,
                         "expected 'enemy <canonical-type>'.");
                }
                pending = {};
                pending.definition.type = parts[1];
                pending.definition.sourceName = source.name;
                pending.startLine = lineNumber;
                insideEnemy = true;
                continue;
            }

            if (parts.size() == 2 && parts[0] == "health") {
                claim(pending, "health", source.name, lineNumber);
                pending.definition.maxHealth = floatValue(
                    parts[1], 1.f, EnemyRegistry::MaxHealth,
                    source.name, lineNumber, "health");
            }
            else if (parts.size() == 4 && parts[0] == "dimensions") {
                claim(pending, "dimensions", source.name, lineNumber);
                pending.definition.dimensions = {
                    floatValue(parts[1], 0.1f, EnemyRegistry::MaxDimension,
                               source.name, lineNumber, "dimensions"),
                    floatValue(parts[2], 0.1f, EnemyRegistry::MaxDimension,
                               source.name, lineNumber, "dimensions"),
                    floatValue(parts[3], 0.1f, EnemyRegistry::MaxDimension,
                               source.name, lineNumber, "dimensions")};
            }
            else if (parts.size() == 2 && parts[0] == "wander_speed") {
                claim(pending, "wander_speed", source.name, lineNumber);
                pending.definition.wanderSpeed = floatValue(
                    parts[1], 0.f, EnemyRegistry::MaxWanderSpeed,
                    source.name, lineNumber, "wander_speed");
            }
            else if (parts.size() == 2 && parts[0] == "chase_radius") {
                claim(pending, "chase_radius", source.name, lineNumber);
                pending.definition.chaseRadius = floatValue(
                    parts[1], 1.f, EnemyRegistry::MaxChaseRadius,
                    source.name, lineNumber, "chase_radius");
            }
            else if (parts.size() == 2 && parts[0] == "chase_speed") {
                claim(pending, "chase_speed", source.name, lineNumber);
                pending.definition.chaseSpeed = floatValue(
                    parts[1], 0.f, EnemyRegistry::MaxChaseSpeed,
                    source.name, lineNumber, "chase_speed");
            }
            else if (parts.size() == 2 && parts[0] == "contact_damage") {
                claim(pending, "contact_damage", source.name, lineNumber);
                pending.definition.contactDamage = floatValue(
                    parts[1], 0.f, EnemyRegistry::MaxContactDamage,
                    source.name, lineNumber, "contact_damage");
            }
            else if (parts.size() == 2 && parts[0] == "combat_mode") {
                claim(pending, "combat_mode", source.name, lineNumber);
                if (parts[1] == "melee") {
                    pending.definition.combat.mode = EnemyCombatMode::Melee;
                }
                else if (parts[1] == "ranged") {
                    pending.definition.combat.mode = EnemyCombatMode::Ranged;
                }
                else {
                    fail(source.name, lineNumber,
                         "combat_mode must be melee or ranged in enemy "
                         "registry v3.");
                }
            }
            else if (parts.size() == 2 && parts[0] == "attack_range") {
                claim(pending, "attack_range", source.name, lineNumber);
                pending.definition.combat.attackRange = floatValue(
                    parts[1], 0.f, EnemyRegistry::MaxAttackRange,
                    source.name, lineNumber, "attack_range");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "attack_windup_ticks") {
                claim(pending, "attack_windup_ticks", source.name,
                      lineNumber);
                pending.definition.combat.windupTicks = integerValue(
                    parts[1], 1, EnemyRegistry::MaxAttackWindupTicks,
                    source.name, lineNumber, "attack_windup_ticks");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "attack_recover_ticks") {
                claim(pending, "attack_recover_ticks", source.name,
                      lineNumber);
                pending.definition.combat.recoverTicks = integerValue(
                    parts[1], 1, EnemyRegistry::MaxAttackRecoverTicks,
                    source.name, lineNumber, "attack_recover_ticks");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "attack_cooldown_ticks") {
                claim(pending, "attack_cooldown_ticks", source.name,
                      lineNumber);
                pending.definition.combat.cooldownTicks = integerValue(
                    parts[1], 1, EnemyRegistry::MaxAttackCooldownTicks,
                    source.name, lineNumber, "attack_cooldown_ticks");
            }
            else if (parts.size() == 2 && parts[0] == "knockback") {
                claim(pending, "knockback", source.name, lineNumber);
                pending.definition.combat.knockback = floatValue(
                    parts[1], 0.f, EnemyRegistry::MaxKnockback,
                    source.name, lineNumber, "knockback");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_speed") {
                claim(pending, "projectile_speed", source.name, lineNumber);
                pending.definition.combat.projectileSpeed = floatValue(
                    parts[1], 1.f, EnemyRegistry::MaxProjectileSpeed,
                    source.name, lineNumber, "projectile_speed");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_damage") {
                claim(pending, "projectile_damage", source.name, lineNumber);
                pending.definition.combat.projectileDamage = floatValue(
                    parts[1], 0.5f, EnemyRegistry::MaxProjectileDamage,
                    source.name, lineNumber, "projectile_damage");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_lifetime_ticks") {
                claim(pending, "projectile_lifetime_ticks", source.name,
                      lineNumber);
                pending.definition.combat.projectileLifetimeTicks =
                    integerValue(
                        parts[1], 1,
                        EnemyRegistry::MaxProjectileLifetimeTicks,
                        source.name, lineNumber,
                        "projectile_lifetime_ticks");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_max_distance") {
                claim(pending, "projectile_max_distance", source.name,
                      lineNumber);
                pending.definition.combat.projectileMaxDistance = floatValue(
                    parts[1], 1.f, EnemyRegistry::MaxProjectileDistance,
                    source.name, lineNumber, "projectile_max_distance");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_radius") {
                claim(pending, "projectile_radius", source.name, lineNumber);
                pending.definition.combat.projectileRadius = floatValue(
                    parts[1], 0.05f, EnemyRegistry::MaxProjectileRadius,
                    source.name, lineNumber, "projectile_radius");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_world_limit") {
                claim(pending, "projectile_world_limit", source.name,
                      lineNumber);
                pending.definition.combat.projectileWorldLimit =
                    integerValue(
                        parts[1], 1, EnemyRegistry::MaxProjectileWorldLimit,
                        source.name, lineNumber, "projectile_world_limit");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_local_limit") {
                claim(pending, "projectile_local_limit", source.name,
                      lineNumber);
                pending.definition.combat.projectileLocalLimit =
                    integerValue(
                        parts[1], 1, EnemyRegistry::MaxProjectileLocalLimit,
                        source.name, lineNumber, "projectile_local_limit");
            }
            else if (parts.size() == 2 &&
                     parts[0] == "projectile_active_radius") {
                claim(pending, "projectile_active_radius", source.name,
                      lineNumber);
                pending.definition.combat.projectileActiveRadius =
                    floatValue(
                        parts[1], 1.f,
                        EnemyRegistry::MaxProjectileActiveRadius,
                        source.name, lineNumber,
                        "projectile_active_radius");
            }
            else if (parts.size() == 2 && parts[0] == "natural") {
                claim(pending, "natural", source.name, lineNumber);
                if (parts[1] != "0" && parts[1] != "1") {
                    fail(source.name, lineNumber,
                         "natural must be 0 or 1.");
                }
                pending.definition.natural = parts[1] == "1";
            }
            else if (parts.size() == 4 && parts[0] == "loot") {
                if (pending.definition.loot.size() >=
                    EnemyRegistry::MaxLootEntries) {
                    fail(source.name, lineNumber,
                         "enemy loot table exceeds its entry limit.");
                }
                Material::ID materialId = Material::ID::Nothing;
                if (!Material::tryParseStringId(parts[1], materialId) ||
                    materialId == Material::ID::Nothing) {
                    fail(source.name, lineNumber,
                         "loot material is not registered.");
                }
                const Material &material = Material::toMaterial(materialId);
                const int minimum = integerValue(
                    parts[2], 1, material.maxStackSize,
                    source.name, lineNumber, "loot minimum");
                const int maximum = integerValue(
                    parts[3], minimum, material.maxStackSize,
                    source.name, lineNumber, "loot maximum");
                const bool duplicate = std::any_of(
                    pending.definition.loot.begin(),
                    pending.definition.loot.end(),
                    [materialId](const EnemyLootDefinition &loot) {
                        return loot.materialId == materialId;
                    });
                if (duplicate) {
                    fail(source.name, lineNumber,
                         "loot material is duplicated.");
                }
                pending.definition.loot.push_back(
                    {materialId, minimum, maximum});
            }
            else if (parts.size() == 1 && parts[0] == "end") {
                std::set<std::string> required = {
                    "health", "dimensions", "wander_speed",
                    "chase_radius", "chase_speed", "contact_damage",
                    "combat_mode", "attack_range",
                    "attack_windup_ticks", "attack_recover_ticks",
                    "attack_cooldown_ticks", "knockback", "natural"};
                if (pending.definition.combat.mode ==
                    EnemyCombatMode::Ranged) {
                    required.insert({
                        "projectile_speed", "projectile_damage",
                        "projectile_lifetime_ticks",
                        "projectile_max_distance", "projectile_radius",
                        "projectile_world_limit", "projectile_local_limit",
                        "projectile_active_radius"});
                }
                if (pending.fields != required ||
                    pending.definition.loot.empty()) {
                    fail(source.name, pending.startLine,
                         "enemy is incomplete or has no loot.");
                }
                if (pending.definition.combat.cooldownTicks <
                    pending.definition.combat.recoverTicks) {
                    fail(source.name, pending.startLine,
                         "attack_cooldown_ticks must be greater than or "
                         "equal to attack_recover_ticks.");
                }
                if (pending.definition.combat.mode ==
                        EnemyCombatMode::Melee &&
                    pending.definition.combat.attackRange >
                        EnemyRegistry::MaxMeleeAttackRange) {
                    fail(source.name, pending.startLine,
                         "melee attack_range must not exceed " +
                         std::to_string(
                             EnemyRegistry::MaxMeleeAttackRange) + ".");
                }
                if (pending.definition.combat.mode ==
                        EnemyCombatMode::Ranged &&
                    pending.definition.combat.projectileLocalLimit >
                        pending.definition.combat.projectileWorldLimit) {
                    fail(source.name, pending.startLine,
                         "projectile_local_limit must not exceed "
                         "projectile_world_limit.");
                }
                if (pending.definition.combat.mode ==
                        EnemyCombatMode::Ranged &&
                    pending.definition.combat.attackRange >
                        pending.definition.combat.projectileActiveRadius) {
                    fail(source.name, pending.startLine,
                         "attack_range must not exceed "
                         "projectile_active_radius.");
                }
                result.push_back(std::move(pending.definition));
                insideEnemy = false;
            }
            else {
                fail(source.name, lineNumber,
                     "unknown, duplicated or misplaced directive.");
            }
        }
        if (!headerSeen) {
            fail(source.name, 1, "empty source.");
        }
        if (insideEnemy) {
            fail(source.name, pending.startLine,
                 "enemy is missing its 'end' directive.");
        }
        return result;
    }
}

void EnemyRegistry::freeze(std::vector<EnemySource> sources)
{
    if (m_frozen) {
        throw std::runtime_error(
            "The enemy registry is already frozen for this process.");
    }
    if (sources.empty() || sources.size() > MaxSourceCount) {
        throw std::runtime_error("Enemy source count is out of bounds.");
    }
    std::sort(sources.begin(), sources.end(),
              [](const EnemySource &left, const EnemySource &right) {
                  return left.name < right.name;
              });

    std::vector<EnemyDefinition> definitions;
    std::set<std::string> sourceNames;
    std::set<std::string> types;
    for (const EnemySource &source : sources) {
        if (source.name.empty() || !sourceNames.insert(source.name).second ||
            source.content.empty() || source.content.size() > MaxSourceBytes) {
            throw std::runtime_error(
                "Enemy sources must be non-empty, bounded and unique.");
        }
        for (EnemyDefinition &definition : parseSource(source)) {
            if (definitions.size() >= MaxEnemies) {
                throw std::runtime_error("Enemy registry exceeds its limit.");
            }
            if (!types.insert(definition.type).second) {
                throw std::runtime_error(
                    "Duplicate enemy type '" + definition.type + "'.");
            }
            definitions.push_back(std::move(definition));
        }
    }
    if (definitions.empty() ||
        std::none_of(definitions.begin(), definitions.end(),
                     [](const EnemyDefinition &definition) {
                         return definition.natural;
                     })) {
        throw std::runtime_error(
            "Enemy registry requires at least one natural enemy.");
    }
    std::sort(definitions.begin(), definitions.end(),
              [](const EnemyDefinition &left,
                 const EnemyDefinition &right) {
                  return left.type < right.type;
              });
    std::unordered_map<std::string, std::size_t> byType;
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        byType.emplace(definitions[index].type, index);
    }
    m_enemies = std::move(definitions);
    m_byType = std::move(byType);
    m_frozen = true;
}

void EnemyRegistry::freezeFromResourceView(
    const ResourcePackResolver &resolver)
{
    if (!resolver.isFrozen()) {
        throw std::runtime_error(
            "Enemy loading requires a frozen effective resource view.");
    }
    std::vector<EnemySource> sources;
    for (const EffectiveResource &resource : resolver.effectiveResources()) {
        if (resource.category != "enemy") {
            continue;
        }
        if (!resource.packName.empty()) {
            throw std::runtime_error(
                "Resource-pack v1 cannot own enemy resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes) {
            throw std::runtime_error(
                "Missing, empty or oversized base enemy resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size)) {
            throw std::runtime_error(
                "Unable to read base enemy resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources));
}

bool EnemyRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const std::vector<EnemyDefinition> &EnemyRegistry::enemies() const noexcept
{
    return m_enemies;
}

const EnemyDefinition *EnemyRegistry::find(
    const std::string &type) const noexcept
{
    const auto found = m_byType.find(type);
    return found == m_byType.end() ? nullptr : &m_enemies[found->second];
}

std::vector<const EnemyDefinition *> EnemyRegistry::naturalEnemies() const
{
    std::vector<const EnemyDefinition *> result;
    for (const EnemyDefinition &definition : m_enemies) {
        if (definition.natural) {
            result.push_back(&definition);
        }
    }
    return result;
}

EnemyRegistry &runtimeEnemyRegistry()
{
    static EnemyRegistry registry;
    return registry;
}

void ensureRuntimeEnemyRegistry()
{
    EnemyRegistry &registry = runtimeEnemyRegistry();
    if (registry.isFrozen()) {
        return;
    }
    const std::string path = ResourcePaths::media("enemies/Base.enemy");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() <= 0 ||
        static_cast<std::size_t>(input.tellg()) >
            EnemyRegistry::MaxSourceBytes) {
        throw std::runtime_error(
            "Missing, empty or oversized base enemy resource '" + path +
            "'.");
    }
    const std::streamsize size = input.tellg();
    input.seekg(0, std::ios::beg);
    std::string content(static_cast<std::size_t>(size), '\0');
    if (!input.read(content.data(), size)) {
        throw std::runtime_error(
            "Unable to read base enemy resource '" + path + "'.");
    }
    registry.freeze({{"media/enemies/Base.enemy", std::move(content)}});
}
