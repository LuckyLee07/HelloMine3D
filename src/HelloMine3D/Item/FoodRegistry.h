#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Material.h"

class ResourcePackResolver;

struct FoodDefinition
{
    Material::ID materialId = Material::ID::Nothing;
    float healthRestored = 0.f;
    int cooldownTicks = 0;
    std::string sourceName;
};

struct FoodSource
{
    std::string name;
    std::string content;
};

/// Parses and freezes base-owned food definitions during startup.
class FoodRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 8;
    static constexpr std::size_t MaxSourceBytes = 64 * 1024;
    static constexpr std::size_t MaxFoods = 32;
    static constexpr float MaxHealthRestored = 20.f;
    static constexpr int MaxCooldownTicks = 1200;

    void freeze(std::vector<FoodSource> sources);
    void freezeFromResourceView(const ResourcePackResolver &resolver);

    bool isFrozen() const noexcept;
    const std::vector<FoodDefinition> &foods() const noexcept;
    const FoodDefinition *find(Material::ID materialId) const noexcept;

  private:
    std::vector<FoodDefinition> m_foods;
    std::unordered_map<Material::ID, std::size_t> m_byMaterial;
    bool m_frozen = false;
};

FoodRegistry &runtimeFoodRegistry();
