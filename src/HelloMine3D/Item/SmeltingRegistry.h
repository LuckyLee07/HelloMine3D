#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Material.h"

class ResourcePackResolver;

struct SmeltingRecipeDefinition
{
    std::string id;
    Material::ID inputMaterialId = Material::ID::Nothing;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int outputAmount = 0;
    int durationTicks = 0;
    std::string sourceName;
};

struct SmeltingFuelDefinition
{
    Material::ID materialId = Material::ID::Nothing;
    int burnTicks = 0;
    std::string sourceName;
};

struct SmeltingSource
{
    std::string name;
    std::string content;
};

/// Strictly parses and freezes base-owned recipes and fuels.
class SmeltingRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 8;
    static constexpr std::size_t MaxSourceBytes = 64 * 1024;
    static constexpr std::size_t MaxRecipes = 64;
    static constexpr std::size_t MaxFuels = 16;
    static constexpr int MaxTicks = 20 * 60 * 60;

    void freeze(std::vector<SmeltingSource> sources);
    void freezeFromResourceView(const ResourcePackResolver &resolver);

    bool isFrozen() const noexcept;
    const std::vector<SmeltingRecipeDefinition> &recipes() const noexcept;
    const std::vector<SmeltingFuelDefinition> &fuels() const noexcept;
    const SmeltingRecipeDefinition *findRecipe(
        Material::ID inputMaterialId) const noexcept;
    const SmeltingFuelDefinition *findFuel(
        Material::ID materialId) const noexcept;

  private:
    std::vector<SmeltingRecipeDefinition> m_recipes;
    std::vector<SmeltingFuelDefinition> m_fuels;
    std::unordered_map<Material::ID, std::size_t> m_recipeByInput;
    std::unordered_map<Material::ID, std::size_t> m_fuelByMaterial;
    bool m_frozen = false;
};

SmeltingRegistry &runtimeSmeltingRegistry();
