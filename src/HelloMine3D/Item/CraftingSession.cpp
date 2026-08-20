#include "CraftingSession.h"

#include <algorithm>
#include <map>

namespace
{
    std::vector<InventorySlotState> requirements(
        const RecipeDefinition &recipe, int crafts = 1)
    {
        std::vector<InventorySlotState> result;
        result.reserve(recipe.ingredients.size());
        for (const RecipeIngredient &ingredient : recipe.ingredients) {
            result.push_back(
                {ingredient.materialId, ingredient.count * crafts});
        }
        return result;
    }
}

CraftingSession::CraftingSession(int gridSize)
    : m_gridSize(gridSize == WorkbenchGridSize ? WorkbenchGridSize
                                               : PlayerGridSize)
    , m_cells(static_cast<std::size_t>(m_gridSize * m_gridSize))
{
}

int CraftingSession::gridSize() const noexcept
{
    return m_gridSize;
}

int CraftingSession::cellCount() const noexcept
{
    return static_cast<int>(m_cells.size());
}

std::uint64_t CraftingSession::version() const noexcept
{
    return m_version;
}

const InventorySlotState &CraftingSession::cell(int index) const noexcept
{
    static const InventorySlotState empty;
    if (index < 0 || index >= cellCount()) {
        return empty;
    }
    return m_cells[static_cast<std::size_t>(index)];
}

bool CraftingSession::setCell(int index, Material::ID materialId, int amount)
{
    if (index < 0 || index >= cellCount() ||
        materialId <= Material::ID::Nothing ||
        materialId >= Material::ID::Count || amount <= 0 ||
        amount > RecipeRegistry::MaxIngredientUnits) {
        return false;
    }
    InventorySlotState &target = m_cells[static_cast<std::size_t>(index)];
    if (target.materialId == materialId && target.amount == amount) {
        return true;
    }
    target = {materialId, amount};
    ++m_version;
    return true;
}

bool CraftingSession::clearCell(int index)
{
    if (index < 0 || index >= cellCount()) {
        return false;
    }
    InventorySlotState &target = m_cells[static_cast<std::size_t>(index)];
    if (target.materialId == Material::ID::Nothing && target.amount == 0) {
        return true;
    }
    target = {};
    ++m_version;
    return true;
}

void CraftingSession::clear()
{
    if (std::all_of(m_cells.begin(), m_cells.end(),
                    [](const InventorySlotState &cell) {
                        return cell.materialId == Material::ID::Nothing &&
                               cell.amount == 0;
                    })) {
        return;
    }
    std::fill(m_cells.begin(), m_cells.end(), InventorySlotState{});
    ++m_version;
}

bool CraftingSession::loadRecipe(const RecipeDefinition &recipe)
{
    std::vector<InventorySlotState> loaded(m_cells.size());
    if (recipe.type == RecipeType::Shaped) {
        if (recipe.width <= 0 || recipe.height <= 0 ||
            recipe.width > m_gridSize || recipe.height > m_gridSize ||
            recipe.shapedCells.size() != static_cast<std::size_t>(
                                             recipe.width * recipe.height)) {
            return false;
        }
        for (int y = 0; y < recipe.height; ++y) {
            for (int x = 0; x < recipe.width; ++x) {
                const Material::ID materialId =
                    recipe.shapedCells[static_cast<std::size_t>(
                        y * recipe.width + x)];
                if (materialId != Material::ID::Nothing) {
                    loaded[static_cast<std::size_t>(y * m_gridSize + x)] =
                        {materialId, 1};
                }
            }
        }
    }
    else {
        int cellIndex = 0;
        for (const RecipeIngredient &ingredient : recipe.ingredients) {
            for (int unit = 0; unit < ingredient.count; ++unit) {
                if (cellIndex >= cellCount()) {
                    return false;
                }
                loaded[static_cast<std::size_t>(cellIndex++)] =
                    {ingredient.materialId, 1};
            }
        }
    }
    if (loaded != m_cells) {
        m_cells = std::move(loaded);
        ++m_version;
    }
    return true;
}

CraftingPreview CraftingSession::preview(
    const RecipeRegistry &recipes, const Inventory &inventory) const
{
    CraftingPreview result;
    result.sessionVersion = m_version;
    result.inventoryVersion = inventory.revision();
    const RecipeDefinition *recipe = matchingRecipe(recipes);
    if (recipe == nullptr) {
        result.message = "No recipe matches the current grid.";
        return result;
    }
    result.recipeId = recipe->id;
    result.outputMaterialId = recipe->outputMaterialId;
    result.outputCount = recipe->outputCount;

    int ingredientLimit = MaxCraftBatch;
    for (const RecipeIngredient &ingredient : recipe->ingredients) {
        ingredientLimit = std::min(
            ingredientLimit,
            inventory.count(ingredient.materialId) / ingredient.count);
    }
    if (ingredientLimit <= 0) {
        result.status = CraftingPreviewStatus::MissingIngredients;
        result.message = "The inventory does not contain every ingredient.";
        return result;
    }

    const Material &output =
        Material::toMaterial(recipe->outputMaterialId);
    for (int crafts = 1; crafts <= ingredientLimit; ++crafts) {
        if (!inventory.canExchange(requirements(*recipe, crafts), output,
                                   recipe->outputCount * crafts)) {
            break;
        }
        result.maxCrafts = crafts;
    }
    if (result.maxCrafts <= 0) {
        result.status = CraftingPreviewStatus::OutputFull;
        result.message = "The crafted output does not fit in the inventory.";
        return result;
    }
    result.status = CraftingPreviewStatus::Ready;
    result.message = "Ready to craft.";
    return result;
}

CraftingCommitResult CraftingSession::commit(
    const RecipeRegistry &recipes, Inventory &inventory,
    const CraftingPreview &expected, int craftCount)
{
    CraftingCommitResult result;
    result.recipeId = expected.recipeId;
    if (craftCount <= 0 || craftCount > MaxCraftBatch ||
        expected.recipeId.empty()) {
        result.message = "Craft count or expected recipe is invalid.";
        return result;
    }
    if (expected.sessionVersion != m_version) {
        result.status = CraftingCommitStatus::StaleSession;
        result.message = "The crafting grid changed after preview.";
        return result;
    }
    if (expected.inventoryVersion != inventory.revision()) {
        result.status = CraftingCommitStatus::StaleInventory;
        result.message = "The inventory changed after preview.";
        return result;
    }

    const CraftingPreview current = preview(recipes, inventory);
    if (current.recipeId != expected.recipeId) {
        result.status = CraftingCommitStatus::NoMatch;
        result.message = "The recipe no longer matches.";
        return result;
    }
    if (current.status == CraftingPreviewStatus::MissingIngredients) {
        result.status = CraftingCommitStatus::MissingIngredients;
        result.message = current.message;
        return result;
    }
    if (current.status == CraftingPreviewStatus::OutputFull ||
        craftCount > current.maxCrafts) {
        result.status = CraftingCommitStatus::OutputFull;
        result.message = current.message;
        return result;
    }
    if (!current.ready()) {
        result.status = CraftingCommitStatus::NoMatch;
        result.message = current.message;
        return result;
    }

    const RecipeDefinition *recipe = recipes.find(current.recipeId);
    if (recipe == nullptr ||
        !inventory.exchange(
            requirements(*recipe, craftCount),
            Material::toMaterial(recipe->outputMaterialId),
            recipe->outputCount * craftCount,
            expected.inventoryVersion)) {
        result.status = CraftingCommitStatus::StaleInventory;
        result.message = "The inventory exchange could not be committed.";
        return result;
    }
    ++m_version;
    result.status = CraftingCommitStatus::Success;
    result.craftsCompleted = craftCount;
    result.outputAdded = recipe->outputCount * craftCount;
    result.message = "Craft completed.";
    return result;
}

const RecipeDefinition *CraftingSession::matchingRecipe(
    const RecipeRegistry &recipes) const
{
    for (const RecipeDefinition &recipe : recipes.recipes()) {
        if (recipe.type == RecipeType::Shaped
                ? matchesShaped(recipe)
                : matchesShapeless(recipe)) {
            return &recipe;
        }
    }
    return nullptr;
}

bool CraftingSession::matchesShaped(const RecipeDefinition &recipe) const
{
    if (recipe.width <= 0 || recipe.height <= 0 ||
        recipe.width > m_gridSize || recipe.height > m_gridSize) {
        return false;
    }
    int minimumX = m_gridSize;
    int minimumY = m_gridSize;
    int maximumX = -1;
    int maximumY = -1;
    for (int index = 0; index < cellCount(); ++index) {
        const InventorySlotState &value = cell(index);
        if (value.materialId == Material::ID::Nothing || value.amount <= 0) {
            continue;
        }
        if (value.amount != 1) {
            return false;
        }
        const int x = index % m_gridSize;
        const int y = index / m_gridSize;
        minimumX = std::min(minimumX, x);
        minimumY = std::min(minimumY, y);
        maximumX = std::max(maximumX, x);
        maximumY = std::max(maximumY, y);
    }
    if (maximumX < 0 || maximumY < 0 ||
        maximumX - minimumX + 1 != recipe.width ||
        maximumY - minimumY + 1 != recipe.height) {
        return false;
    }
    for (int y = 0; y < recipe.height; ++y) {
        for (int x = 0; x < recipe.width; ++x) {
            const Material::ID actual =
                cell((minimumY + y) * m_gridSize + minimumX + x)
                    .materialId;
            const Material::ID expected =
                recipe.shapedCells[static_cast<std::size_t>(
                    y * recipe.width + x)];
            if (actual != expected) {
                return false;
            }
        }
    }
    return true;
}

bool CraftingSession::matchesShapeless(
    const RecipeDefinition &recipe) const
{
    std::map<Material::ID, int> actual;
    for (const InventorySlotState &value : m_cells) {
        if (value.materialId != Material::ID::Nothing && value.amount > 0) {
            actual[value.materialId] += value.amount;
        }
    }
    if (actual.size() != recipe.ingredients.size()) {
        return false;
    }
    for (const RecipeIngredient &ingredient : recipe.ingredients) {
        const auto found = actual.find(ingredient.materialId);
        if (found == actual.end() || found->second != ingredient.count) {
            return false;
        }
    }
    return true;
}
