#include "ResourceEconomyVerifier.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>

#include "FoodRegistry.h"
#include "RecipeRegistry.h"
#include "SmeltingRegistry.h"

namespace
{
constexpr double Unreachable = std::numeric_limits<double>::infinity();

bool validMaterial(Material::ID id) noexcept
{
    return id > Material::ID::Nothing && id < Material::ID::Count;
}

std::vector<RecipeIngredient> ingredientsOf(
    const RecipeDefinition &recipe)
{
    if (recipe.type == RecipeType::Shapeless) {
        return recipe.ingredients;
    }
    std::unordered_map<Material::ID, int> counts;
    for (Material::ID materialId : recipe.shapedCells) {
        if (materialId != Material::ID::Nothing) {
            ++counts[materialId];
        }
    }
    std::vector<RecipeIngredient> result;
    result.reserve(counts.size());
    for (const auto &entry : counts) {
        result.push_back({entry.first, entry.second});
    }
    return result;
}

void addEdge(std::vector<std::vector<int>> &graph,
             Material::ID from, Material::ID to)
{
    if (validMaterial(from) && validMaterial(to)) {
        graph[static_cast<std::size_t>(from)].push_back(
            static_cast<int>(to));
    }
}

bool visitCycle(int node, const std::vector<std::vector<int>> &graph,
                std::vector<unsigned char> &state)
{
    if (state[static_cast<std::size_t>(node)] == 1) {
        return true;
    }
    if (state[static_cast<std::size_t>(node)] == 2) {
        return false;
    }
    state[static_cast<std::size_t>(node)] = 1;
    for (int next : graph[static_cast<std::size_t>(node)]) {
        if (visitCycle(next, graph, state)) {
            return true;
        }
    }
    state[static_cast<std::size_t>(node)] = 2;
    return false;
}

std::string materialName(Material::ID id)
{
    return Material::toStringId(id);
}
} // namespace

bool ResourceEconomyReport::passed() const noexcept
{
    return allRequiredReachable && trackedSourcesAndSinks &&
           transformationGraphAcyclic && issues.empty();
}

std::string ResourceEconomyReport::toCsv() const
{
    std::ostringstream output;
    output << "material_id,acquisition_ticks_per_unit,health_restored,"
              "cooldown_ticks,recovery_per_acquisition_tick,max_stack,"
              "goal_amount,goal_inventory_slots\n";
    output << std::fixed << std::setprecision(4);
    for (const ResourceEconomyMetric &metric : metrics) {
        output << Material::toStringId(metric.materialId) << ','
               << metric.acquisitionTicksPerUnit << ','
               << metric.healthRestored << ',' << metric.cooldownTicks
               << ',' << metric.recoveryPerAcquisitionTick << ','
               << metric.maxStackSize << ',' << metric.goalAmount << ','
               << metric.goalInventorySlots << '\n';
    }
    return output.str();
}

ResourceEconomyReport ResourceEconomyVerifier::verify(
    const ResourceEconomyContract &contract,
    const RecipeRegistry &recipes,
    const SmeltingRegistry &smelting,
    const FoodRegistry &foods)
{
    ResourceEconomyReport report;
    if (contract.version != 1 || contract.craftingTicks <= 0 ||
        contract.playerInventorySlots <= 0 || !recipes.isFrozen() ||
        !smelting.isFrozen() || !foods.isFrozen()) {
        report.issues.push_back(
            "economy contract and registries must be valid and frozen");
        return report;
    }

    const std::size_t materialCount =
        static_cast<std::size_t>(Material::ID::Count);
    std::vector<double> costs(materialCount, Unreachable);
    std::set<Material::ID> produced;
    std::set<Material::ID> consumed;
    for (const ResourceAcquisitionSource &source :
         contract.acquisitionSources) {
        if (source.id.empty() || !validMaterial(source.materialId) ||
            source.amount <= 0 || source.acquisitionTicks <= 0) {
            report.issues.push_back("invalid acquisition source");
            continue;
        }
        const double unitCost =
            static_cast<double>(source.acquisitionTicks) /
            static_cast<double>(source.amount);
        double &current = costs[static_cast<std::size_t>(source.materialId)];
        current = std::min(current, unitCost);
        produced.insert(source.materialId);
    }

    std::vector<std::vector<int>> graph(materialCount);
    for (const RecipeDefinition &recipe : recipes.recipes()) {
        produced.insert(recipe.outputMaterialId);
        for (const RecipeIngredient &ingredient : ingredientsOf(recipe)) {
            consumed.insert(ingredient.materialId);
            addEdge(graph, ingredient.materialId,
                    recipe.outputMaterialId);
        }
    }
    for (const SmeltingRecipeDefinition &recipe : smelting.recipes()) {
        produced.insert(recipe.outputMaterialId);
        consumed.insert(recipe.inputMaterialId);
        addEdge(graph, recipe.inputMaterialId, recipe.outputMaterialId);
    }
    for (const SmeltingFuelDefinition &fuel : smelting.fuels()) {
        consumed.insert(fuel.materialId);
        for (const SmeltingRecipeDefinition &recipe :
             smelting.recipes()) {
            addEdge(graph, fuel.materialId, recipe.outputMaterialId);
        }
    }
    for (const FoodDefinition &food : foods.foods()) {
        consumed.insert(food.materialId);
    }

    std::vector<unsigned char> cycleState(materialCount, 0);
    bool hasCycle = false;
    for (int material = static_cast<int>(Material::ID::Nothing) + 1;
         material < static_cast<int>(Material::ID::Count); ++material) {
        if (visitCycle(material, graph, cycleState)) {
            hasCycle = true;
            break;
        }
    }
    report.transformationGraphAcyclic = !hasCycle;
    if (hasCycle) {
        report.issues.push_back(
            "transformation graph contains a material cycle");
    }

    const int passLimit = static_cast<int>(Material::ID::Count) * 4;
    for (int pass = 0; pass < passLimit; ++pass) {
        bool changed = false;
        for (const RecipeDefinition &recipe : recipes.recipes()) {
            double candidate = contract.craftingTicks;
            bool reachable = true;
            for (const RecipeIngredient &ingredient : ingredientsOf(recipe)) {
                const double ingredientCost =
                    costs[static_cast<std::size_t>(ingredient.materialId)];
                if (!std::isfinite(ingredientCost)) {
                    reachable = false;
                    break;
                }
                candidate += ingredientCost * ingredient.count;
            }
            if (!reachable) {
                continue;
            }
            candidate /= recipe.outputCount;
            double &outputCost =
                costs[static_cast<std::size_t>(recipe.outputMaterialId)];
            if (candidate + 0.000001 < outputCost) {
                outputCost = candidate;
                changed = true;
            }
        }

        double fuelCostPerTick = Unreachable;
        for (const SmeltingFuelDefinition &fuel : smelting.fuels()) {
            const double fuelCost =
                costs[static_cast<std::size_t>(fuel.materialId)];
            if (std::isfinite(fuelCost)) {
                fuelCostPerTick = std::min(
                    fuelCostPerTick, fuelCost / fuel.burnTicks);
            }
        }
        if (std::isfinite(fuelCostPerTick)) {
            for (const SmeltingRecipeDefinition &recipe :
                 smelting.recipes()) {
                const double inputCost =
                    costs[static_cast<std::size_t>(
                        recipe.inputMaterialId)];
                if (!std::isfinite(inputCost)) {
                    continue;
                }
                const double candidate =
                    (inputCost + fuelCostPerTick * recipe.durationTicks) /
                    recipe.outputAmount;
                double &outputCost =
                    costs[static_cast<std::size_t>(
                        recipe.outputMaterialId)];
                if (candidate + 0.000001 < outputCost) {
                    outputCost = candidate;
                    changed = true;
                }
            }
        }
        if (!changed) {
            break;
        }
    }

    report.allRequiredReachable = true;
    std::set<Material::ID> required;
    for (Material::ID materialId : contract.requiredMaterials) {
        if (!validMaterial(materialId) ||
            !std::isfinite(costs[static_cast<std::size_t>(materialId)])) {
            report.allRequiredReachable = false;
            report.issues.push_back(
                "required material is unreachable: " +
                materialName(materialId));
        }
        else {
            required.insert(materialId);
        }
    }

    report.trackedSourcesAndSinks = true;
    for (Material::ID materialId : contract.trackedNewMaterials) {
        if (!validMaterial(materialId) || produced.count(materialId) == 0) {
            report.trackedSourcesAndSinks = false;
            report.issues.push_back(
                "tracked material has no source: " +
                materialName(materialId));
        }
        if (!validMaterial(materialId) || consumed.count(materialId) == 0) {
            report.trackedSourcesAndSinks = false;
            report.issues.push_back(
                "tracked material has no sink: " +
                materialName(materialId));
        }
    }

    std::unordered_map<Material::ID, int> goalAmounts;
    for (const ResourceGoalRequirement &goal : contract.goalRequirements) {
        if (goal.goalId.empty() || !validMaterial(goal.materialId) ||
            goal.amount <= 0) {
            report.issues.push_back("invalid goal requirement");
            continue;
        }
        goalAmounts[goal.materialId] += goal.amount;
    }

    std::set<Material::ID> metricMaterials = required;
    metricMaterials.insert(contract.trackedNewMaterials.begin(),
                           contract.trackedNewMaterials.end());
    for (const auto &goal : goalAmounts) {
        metricMaterials.insert(goal.first);
    }
    for (Material::ID materialId : metricMaterials) {
        if (!validMaterial(materialId)) {
            continue;
        }
        ResourceEconomyMetric metric;
        metric.materialId = materialId;
        metric.acquisitionTicksPerUnit =
            costs[static_cast<std::size_t>(materialId)];
        const FoodDefinition *food = foods.find(materialId);
        if (food != nullptr) {
            metric.healthRestored = food->healthRestored;
            metric.cooldownTicks = food->cooldownTicks;
            if (std::isfinite(metric.acquisitionTicksPerUnit) &&
                metric.acquisitionTicksPerUnit > 0.0) {
                metric.recoveryPerAcquisitionTick =
                    food->healthRestored /
                    metric.acquisitionTicksPerUnit;
            }
        }
        metric.maxStackSize = Material::toMaterial(materialId).maxStackSize;
        metric.goalAmount = goalAmounts[materialId];
        metric.goalInventorySlots = metric.goalAmount > 0
            ? (metric.goalAmount + metric.maxStackSize - 1) /
                  metric.maxStackSize
            : 0;
        report.metrics.push_back(metric);
    }
    std::sort(report.metrics.begin(), report.metrics.end(),
              [](const ResourceEconomyMetric &left,
                 const ResourceEconomyMetric &right) {
                  return left.materialId < right.materialId;
              });
    return report;
}

ResourceEconomyContract makeBaseResourceEconomyContract()
{
    ResourceEconomyContract contract;
    contract.acquisitionSources = {
        {"terrain.grass", Material::ID::Grass, 1, 30},
        {"terrain.dirt", Material::ID::Dirt, 1, 20},
        {"terrain.stone", Material::ID::Stone, 1, 40},
        {"forest.oak_bark", Material::ID::OakBark, 1, 30},
        {"forest.oak_leaf", Material::ID::OakLeaf, 1, 10},
        {"terrain.sand", Material::ID::Sand, 1, 20},
        {"desert.cactus", Material::ID::Cactus, 1, 20},
        {"grassland.rose", Material::ID::Rose, 1, 10},
        {"grassland.tall_grass", Material::ID::TallGrass, 1, 10},
        {"desert.dead_shrub", Material::ID::DeadShrub, 1, 10},
        {"terrain.coal_ore", Material::ID::CoalOre, 1, 80},
        {"terrain.iron_ore", Material::ID::IronOre, 1, 120},
        {"forage.wheat_seeds", Material::ID::WheatSeeds, 1, 20},
        {"crop.wheat", Material::ID::Wheat, 1, 160},
        {"structure.waystone_core", Material::ID::WaystoneCore, 1, 400},
        {"enemy.raw_meat", Material::ID::RawMeat, 1, 200},
    };
    contract.requiredMaterials = {
        Material::ID::Workbench, Material::ID::Chest,
        Material::ID::WoodenPickaxe, Material::ID::StonePickaxe,
        Material::ID::Furnace, Material::ID::IronIngot,
        Material::ID::IronPickaxe, Material::ID::WoodenSword,
        Material::ID::StoneSword, Material::ID::IronSword,
        Material::ID::Bread, Material::ID::Glass,
        Material::ID::GlassBorderless, Material::ID::WaystoneCore,
        Material::ID::RawMeat, Material::ID::CookedMeat,
        Material::ID::CactusSalad, Material::ID::TrailRation,
        Material::ID::PlantFiber, Material::ID::Torch,
    };
    contract.trackedNewMaterials = {
        Material::ID::RawMeat, Material::ID::CookedMeat,
        Material::ID::CactusSalad, Material::ID::TrailRation,
        Material::ID::PlantFiber,
    };
    contract.goalRequirements = {
        {"progression.iron_pickaxe", Material::ID::IronIngot, 3},
        {"progression.iron_sword", Material::ID::IronIngot, 2},
        {"victory.waystone_activation", Material::ID::IronIngot, 2},
        {"recovery.field_supply", Material::ID::Bread, 2},
        {"recovery.field_supply", Material::ID::CookedMeat, 2},
        {"recovery.expedition", Material::ID::TrailRation, 1},
    };
    return contract;
}
