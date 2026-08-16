#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "Material.h"

class ResourcePackResolver;

enum class RecipeType
{
    Shaped,
    Shapeless
};

struct RecipeIngredient
{
    Material::ID materialId = Material::ID::Nothing;
    int count = 0;
};

struct RecipeDefinition
{
    std::string id;
    RecipeType type = RecipeType::Shapeless;
    int width = 0;
    int height = 0;
    std::vector<Material::ID> shapedCells;
    std::vector<RecipeIngredient> ingredients;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int outputCount = 0;
    std::string sourceName;
};

struct RecipeSource
{
    std::string name;
    std::string content;
};

/// Parses and freezes the base recipe view. A successful registry is
/// immutable for the lifetime of the process; resource-pack v1 may not own a
/// recipe source.
class RecipeRegistry
{
  public:
    static constexpr std::size_t MaxSourceCount = 32;
    static constexpr std::size_t MaxSourceBytes = 256 * 1024;
    static constexpr std::size_t MaxRecipes = 256;
    static constexpr int MaxGridDimension = 3;
    static constexpr int MaxIngredientEntries = 9;
    static constexpr int MaxIngredientUnits = 99;
    static constexpr std::size_t MaxRecipeIdLength = 80;

    void freeze(std::vector<RecipeSource> sources);
    void freezeFromResourceView(const ResourcePackResolver &resolver);

    bool isFrozen() const noexcept;
    const std::vector<RecipeDefinition> &recipes() const noexcept;
    const RecipeDefinition *find(const std::string &id) const noexcept;

  private:
    std::vector<RecipeDefinition> m_recipes;
    std::unordered_map<std::string, std::size_t> m_byId;
    bool m_frozen = false;
};

RecipeRegistry &runtimeRecipeRegistry();
