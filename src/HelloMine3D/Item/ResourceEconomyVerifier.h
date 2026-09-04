#pragma once

#include <string>
#include <vector>

#include "Material.h"
#include "MachineProcessDefinition.h"

class FoodRegistry;
class RecipeRegistry;
class SmeltingRegistry;

struct ResourceAcquisitionSource
{
    std::string id;
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;
    int acquisitionTicks = 0;
};

struct ResourceGoalRequirement
{
    std::string goalId;
    Material::ID materialId = Material::ID::Nothing;
    int amount = 0;
};

/// Versioned, code-owned economy inputs. Recipe, smelting and food definitions
/// remain authoritative for their transformations and recovery values; C2
/// adds only its concrete machine process definitions.
struct ResourceEconomyContract
{
    int version = 2;
    int craftingTicks = 1;
    int playerInventorySlots = 5;
    std::vector<ResourceAcquisitionSource> acquisitionSources;
    std::vector<Material::ID> requiredMaterials;
    std::vector<Material::ID> trackedNewMaterials;
    std::vector<ResourceGoalRequirement> goalRequirements;
    std::vector<MachineProcessDefinition> machineProcesses;
};

struct ResourceEconomyMetric
{
    Material::ID materialId = Material::ID::Nothing;
    double acquisitionTicksPerUnit = 0.0;
    float healthRestored = 0.f;
    int cooldownTicks = 0;
    double recoveryPerAcquisitionTick = 0.0;
    int maxStackSize = 0;
    int goalAmount = 0;
    int goalInventorySlots = 0;
};

struct ResourceEconomyReport
{
    bool allRequiredReachable = false;
    bool trackedSourcesAndSinks = false;
    bool transformationGraphAcyclic = false;
    std::vector<std::string> issues;
    std::vector<ResourceEconomyMetric> metrics;

    bool passed() const noexcept;
    std::string toCsv() const;
};

class ResourceEconomyVerifier
{
  public:
    static ResourceEconomyReport verify(
        const ResourceEconomyContract &contract,
        const RecipeRegistry &recipes,
        const SmeltingRegistry &smelting,
        const FoodRegistry &foods);
};

ResourceEconomyContract makeBaseResourceEconomyContract();
