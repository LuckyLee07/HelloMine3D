#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Item/Material.h"
#include "../Maths/glm.h"

class ResourcePackResolver;

struct EnemyLootDefinition
{
    Material::ID materialId = Material::ID::Nothing;
    int minimumAmount = 0;
    int maximumAmount = 0;
};

struct EnemyDefinition
{
    std::string type;
    float maxHealth = 10.f;
    glm::vec3 dimensions{0.35f, 0.9f, 0.35f};
    float wanderSpeed = 1.2f;
    float chaseRadius = 12.f;
    float chaseSpeed = 2.4f;
    float contactDamage = 2.f;
    bool natural = false;
    std::vector<EnemyLootDefinition> loot;
    std::string sourceName;
};

struct EnemySource
{
    std::string name;
    std::string content;
};

/// Parses and freezes base-owned enemy archetypes and bounded loot tables.
class EnemyRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 8;
    static constexpr std::size_t MaxSourceBytes = 128 * 1024;
    static constexpr std::size_t MaxEnemies = 16;
    static constexpr std::size_t MaxLootEntries = 4;
    static constexpr std::size_t MaxTypeLength = 80;
    static constexpr float MaxHealth = 100.f;
    static constexpr float MaxDimension = 2.f;
    static constexpr float MaxWanderSpeed = 8.f;
    static constexpr float MaxChaseRadius = 64.f;
    static constexpr float MaxChaseSpeed = 12.f;
    static constexpr float MaxContactDamage = 20.f;

    void freeze(std::vector<EnemySource> sources);
    void freezeFromResourceView(const ResourcePackResolver &resolver);

    bool isFrozen() const noexcept;
    const std::vector<EnemyDefinition> &enemies() const noexcept;
    const EnemyDefinition *find(const std::string &type) const noexcept;
    std::vector<const EnemyDefinition *> naturalEnemies() const;

  private:
    std::vector<EnemyDefinition> m_enemies;
    std::unordered_map<std::string, std::size_t> m_byType;
    bool m_frozen = false;
};

EnemyRegistry &runtimeEnemyRegistry();
void ensureRuntimeEnemyRegistry();
