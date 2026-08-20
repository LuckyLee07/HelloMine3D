#ifndef CRAFTINGSESSION_H_INCLUDED
#define CRAFTINGSESSION_H_INCLUDED

#include "Inventory.h"
#include "RecipeRegistry.h"

#include <cstdint>
#include <string>
#include <vector>

enum class CraftingPreviewStatus {
    NoMatch,
    MissingIngredients,
    OutputFull,
    Ready
};

struct CraftingPreview {
    CraftingPreviewStatus status = CraftingPreviewStatus::NoMatch;
    std::string recipeId;
    Material::ID outputMaterialId = Material::ID::Nothing;
    int outputCount = 0;
    int maxCrafts = 0;
    std::uint64_t sessionVersion = 0;
    std::uint64_t inventoryVersion = 0;
    std::string message;

    bool ready() const noexcept
    {
        return status == CraftingPreviewStatus::Ready && maxCrafts > 0;
    }
};

enum class CraftingCommitStatus {
    Success,
    InvalidRequest,
    StaleSession,
    StaleInventory,
    NoMatch,
    MissingIngredients,
    OutputFull
};

struct CraftingCommitResult {
    CraftingCommitStatus status = CraftingCommitStatus::InvalidRequest;
    std::string recipeId;
    int craftsCompleted = 0;
    int outputAdded = 0;
    std::string message;

    bool succeeded() const noexcept
    {
        return status == CraftingCommitStatus::Success;
    }
};

class CraftingSession {
  public:
    static constexpr int PlayerGridSize = 2;
    static constexpr int WorkbenchGridSize = 3;
    static constexpr int MaxCraftBatch = 99;

    explicit CraftingSession(int gridSize);

    int gridSize() const noexcept;
    int cellCount() const noexcept;
    std::uint64_t version() const noexcept;
    const InventorySlotState &cell(int index) const noexcept;

    bool setCell(int index, Material::ID materialId, int amount = 1);
    bool clearCell(int index);
    void clear();
    bool loadRecipe(const RecipeDefinition &recipe);

    CraftingPreview preview(const RecipeRegistry &recipes,
                            const Inventory &inventory) const;
    CraftingCommitResult commit(const RecipeRegistry &recipes,
                                Inventory &inventory,
                                const CraftingPreview &expected,
                                int craftCount);

  private:
    const RecipeDefinition *matchingRecipe(
        const RecipeRegistry &recipes) const;
    bool matchesShaped(const RecipeDefinition &recipe) const;
    bool matchesShapeless(const RecipeDefinition &recipe) const;

    int m_gridSize = PlayerGridSize;
    std::vector<InventorySlotState> m_cells;
    std::uint64_t m_version = 1;
};

#endif // CRAFTINGSESSION_H_INCLUDED
