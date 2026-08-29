#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Material.h"

class ResourcePackResolver;

enum class MiningClass
{
    None,
    Pickaxe,
    Axe,
    Shovel,
    Weapon
};

struct ToolDefinition
{
    Material::ID materialId = Material::ID::Nothing;
    MiningClass miningClass = MiningClass::None;
    int tier = 0;
    float speedMultiplier = 1.0f;
    int maxDurability = 0;
    float attackDamage = 2.0f;
    int attackCooldownTicks = 10;
    float attackReach = 3.0f;
    std::string sourceName;
};

struct ToolSource
{
    std::string name;
    std::string content;
};

/// Parses and freezes base-owned tool definitions during startup.
class ToolRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 8;
    static constexpr std::size_t MaxSourceBytes = 64 * 1024;
    static constexpr std::size_t MaxTools = 32;
    static constexpr int MaxTier = 10;
    static constexpr int MaxDurability = 100000;
    static constexpr float MaxSpeedMultiplier = 16.0f;
    static constexpr float MaxAttackDamage = 64.0f;
    static constexpr int MaxAttackCooldownTicks = 60;
    static constexpr float MaxAttackReach = 6.0f;

    void freeze(std::vector<ToolSource> sources);
    void freezeFromResourceView(const ResourcePackResolver &resolver);

    bool isFrozen() const noexcept;
    const std::vector<ToolDefinition> &tools() const noexcept;
    const ToolDefinition *find(Material::ID materialId) const noexcept;

    static const char *miningClassName(MiningClass value) noexcept;
    static bool tryParseMiningClass(const std::string &text,
                                    MiningClass &value) noexcept;

  private:
    std::vector<ToolDefinition> m_tools;
    std::unordered_map<Material::ID, std::size_t> m_byMaterial;
    bool m_frozen = false;
};

ToolRegistry &runtimeToolRegistry();
