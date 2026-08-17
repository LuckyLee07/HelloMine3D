#include "../Item/RecipeRegistry.h"
#include "../Item/CraftingSession.h"
#include "../Item/ToolRegistry.h"
#include "../Util/ResourcePackResolver.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    int checks = 0;
    int failures = 0;

    void check(const std::string &id, bool passed,
               const std::string &detail = "")
    {
        ++checks;
        if (!passed) {
            ++failures;
        }
        std::cout << "[RECIPE_TEST] " << (passed ? "PASS " : "FAIL ")
                  << id;
        if (!detail.empty()) {
            std::cout << " :: " << detail;
        }
        std::cout << '\n';
    }

    bool throwsContaining(const std::function<void()> &action,
                          const std::string &expected)
    {
        try {
            action();
        }
        catch (const std::exception &error) {
            return std::string(error.what()).find(expected) !=
                   std::string::npos;
        }
        return false;
    }

    std::string validRecipes()
    {
        return R"(# HelloMine3D recipe registry v1
recipe hellomine:z_shaped shaped
row _ hellomine:stone _
row _ hellomine:dirt _
output hellomine:chest 1
end
recipe hellomine:a_shapeless shapeless
input hellomine:wheat 2
input hellomine:wheat_seeds 1
output hellomine:grass 1
end
)";
    }

    std::string validTools()
    {
        return R"(# HelloMine3D tool registry v1
tool hellomine:wooden_pickaxe
class pickaxe
tier 1
speed 2
durability 16
end
tool hellomine:stone_pickaxe
class pickaxe
tier 2
speed 4
durability 32
end
tool hellomine:iron_pickaxe
class pickaxe
tier 3
speed 6
durability 64
attack 4
end
tool hellomine:iron_sword
class weapon
tier 3
speed 1
durability 80
attack 7
end
)";
    }

    std::string oneRecipe(const std::string &body)
    {
        return "# HelloMine3D recipe registry v1\n" + body;
    }

    std::string simpleRecipe(const std::string &id)
    {
        return oneRecipe("recipe " + id + " shapeless\n"
                         "input hellomine:stone 1\n"
                         "output hellomine:dirt 1\nend\n");
    }

    void writeFile(const fs::path &path, const std::string &content)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << content;
    }

    void caseMaterialIds()
    {
        bool roundTrip = true;
        for (int value = 0; value < static_cast<int>(Material::ID::Count);
             ++value) {
            const Material::ID expected = static_cast<Material::ID>(value);
            Material::ID actual = Material::ID::Count;
            roundTrip = roundTrip && Material::tryParseStringId(
                                         Material::toStringId(expected), actual) &&
                        actual == expected;
        }
        check("G1/material-ids-roundtrip",
              roundTrip &&
                  static_cast<int>(Material::ID::IronSword) ==
                      static_cast<int>(Material::ID::Count) - 1 &&
                  static_cast<int>(BlockId::Furnace) ==
                      static_cast<int>(BlockId::NUM_TYPES) - 1);
        Material::ID unchanged = Material::ID::Stone;
        check("G1/unknown-material-id-rejected",
              !Material::tryParseStringId("hellomine:not_registered",
                                          unchanged) &&
                  unchanged == Material::ID::Stone);
    }

    void caseValidAndFrozen()
    {
        RecipeRegistry registry;
        registry.freeze({{"valid.recipe", validRecipes()}});
        const RecipeDefinition *shaped = registry.find("hellomine:z_shaped");
        const RecipeDefinition *shapeless =
            registry.find("hellomine:a_shapeless");
        check("G1/valid-shaped-and-shapeless",
              registry.isFrozen() && registry.recipes().size() == 2 &&
                  shaped != nullptr && shapeless != nullptr &&
                  shaped->type == RecipeType::Shaped && shaped->width == 1 &&
                  shaped->height == 2 && shaped->shapedCells.size() == 2 &&
                  shaped->ingredients.size() == 2 &&
                  shapeless->type == RecipeType::Shapeless &&
                  shapeless->ingredients.size() == 2);
        check("G1/registry-order-is-deterministic",
              registry.recipes().front().id == "hellomine:a_shapeless" &&
                  registry.recipes().back().id == "hellomine:z_shaped");
        check("G1/registered-output-is-bounded",
              shaped->outputMaterialId == Material::ID::Chest &&
                  shaped->outputCount == 1 &&
                  shapeless->outputMaterialId == Material::ID::Grass);
        check("G1/runtime-reload-rejected",
              throwsContaining(
                  [&]
                  {
                      registry.freeze({{"again.recipe", validRecipes()}});
                  },
                  "already frozen"));
    }

    void caseStrictParsing()
    {
        check("G1/unknown-input-material",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"unknown.recipe", oneRecipe(
                          "recipe hellomine:bad shapeless\n"
                          "input hellomine:missing 1\n"
                          "output hellomine:stone 1\nend\n")}});
                  },
                  "unknown or empty material id"));

        const std::vector<std::pair<std::string, std::string>> badCounts = {
            {"zero", "input hellomine:stone 0\n"},
            {"large", "input hellomine:stone 100\n"},
        };
        for (const auto &fixture : badCounts) {
            check("G1/invalid-count-" + fixture.first,
                  throwsContaining(
                      [&]
                      {
                          RecipeRegistry registry;
                          registry.freeze({{fixture.first + ".recipe",
                              oneRecipe(
                                  "recipe hellomine:bad shapeless\n" +
                                  fixture.second +
                                  "output hellomine:dirt 1\nend\n")}});
                      },
                      "count must be"));
        }

        check("G1/shapeless-total-is-bounded",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"total.recipe", oneRecipe(
                          "recipe hellomine:bad shapeless\n"
                          "input hellomine:stone 50\n"
                          "input hellomine:dirt 50\n"
                          "output hellomine:grass 1\nend\n")}});
                  },
                  "input total exceeds"));
        check("G1/shaped-grid-is-bounded",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"grid.recipe", oneRecipe(
                          "recipe hellomine:bad shaped\n"
                          "row hellomine:stone hellomine:stone "
                          "hellomine:stone hellomine:stone\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "out-of-bounds shaped row"));
        check("G1/missing-end-is-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"unfinished.recipe", oneRecipe(
                          "recipe hellomine:bad shapeless\n"
                          "input hellomine:stone 1\n"
                          "output hellomine:dirt 1\n")}});
                  },
                  "missing its 'end'"));
    }

    void caseIdsAndSourceBounds()
    {
        RecipeRegistry validId;
        validId.freeze({{"nested.recipe",
                         simpleRecipe("hellomine:building/chest")}});
        check("G1/ascii-nested-recipe-id",
              validId.find("hellomine:building/chest") != nullptr);

        const std::vector<std::string> invalidIds = {
            "HelloMine:upper",
            "hellomine:Upper",
            "hellomine:path..escape",
            "hellomine:/leading",
            "hellomine:trailing/",
            "hellomine:double//slash",
            "hellomine:extra:colon",
            "hellomine:\xC3\xA9",
            "hellomine:" + std::string(72, 'a'),
        };
        bool invalidIdsRejected = true;
        for (const std::string &id : invalidIds) {
            invalidIdsRejected =
                invalidIdsRejected && throwsContaining(
                    [&]
                    {
                        RecipeRegistry registry;
                        registry.freeze({{"invalid-id.recipe",
                                          simpleRecipe(id)}});
                    },
                    "expected 'recipe namespace:id");
        }
        check("G1/non-ascii-or-malformed-recipe-id-rejected",
              invalidIdsRejected);

        check("G1/empty-source-set-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({});
                  },
                  "source count must be"));

        check("G1/source-count-is-bounded",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      std::vector<RecipeSource> sources(
                          RecipeRegistry::MaxSourceCount + 1);
                      registry.freeze(std::move(sources));
                  },
                  "source count must be"));

        check("G1/duplicate-source-name-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"same.recipe",
                                        simpleRecipe("hellomine:first")},
                                       {"same.recipe",
                                        simpleRecipe("hellomine:second")}});
                  },
                  "source names must be non-empty and unique"));

        check("G1/empty-source-name-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze(
                          {{"", simpleRecipe("hellomine:empty_name")}});
                  },
                  "source names must be non-empty and unique"));

        check("G1/empty-source-content-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"empty.recipe", ""}});
                  },
                  "is empty or exceeds"));

        check("G1/header-only-registry-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"none.recipe",
                                        "# HelloMine3D recipe registry v1\n"}});
                  },
                  "contains no recipes"));
    }

    void caseDirectiveCompleteness()
    {
        check("G1/empty-shaped-pattern-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"empty-shaped.recipe", oneRecipe(
                          "recipe hellomine:empty shaped\n"
                          "row _ _\nrow _ _\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "only empty cells"));

        check("G1/uneven-shaped-rows-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"uneven.recipe", oneRecipe(
                          "recipe hellomine:uneven shaped\n"
                          "row hellomine:stone\n"
                          "row hellomine:stone hellomine:dirt\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "equal widths"));

        check("G1/shaped-height-is-bounded",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"height.recipe", oneRecipe(
                          "recipe hellomine:tall shaped\n"
                          "row hellomine:stone\nrow hellomine:stone\n"
                          "row hellomine:stone\nrow hellomine:stone\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "out-of-bounds shaped row"));

        std::string manyInputs =
            "recipe hellomine:many shapeless\n";
        for (int value = 1; value <= 10; ++value) {
            manyInputs += "input ";
            manyInputs += Material::toStringId(
                static_cast<Material::ID>(value));
            manyInputs += " 1\n";
        }
        manyInputs += "output hellomine:dirt 1\nend\n";
        check("G1/shapeless-entry-count-is-bounded",
              throwsContaining(
                  [&]
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"many.recipe",
                                        oneRecipe(manyInputs)}});
                  },
                  "out-of-bounds shapeless input"));

        check("G1/duplicate-shapeless-input-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"duplicate-input.recipe", oneRecipe(
                          "recipe hellomine:duplicate shapeless\n"
                          "input hellomine:stone 1\n"
                          "input hellomine:stone 2\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "duplicate shapeless material"));

        check("G1/mixed-recipe-directives-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"mixed.recipe", oneRecipe(
                          "recipe hellomine:mixed shaped\n"
                          "input hellomine:stone 1\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "out-of-bounds shapeless input"));

        check("G1/missing-output-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"missing-output.recipe", oneRecipe(
                          "recipe hellomine:missing_output shapeless\n"
                          "input hellomine:stone 1\nend\n")}});
                  },
                  "missing its output"));

        check("G1/duplicate-output-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"duplicate-output.recipe", oneRecipe(
                          "recipe hellomine:duplicate_output shapeless\n"
                          "input hellomine:stone 1\n"
                          "output hellomine:dirt 1\n"
                          "output hellomine:grass 1\nend\n")}});
                  },
                  "exactly one output"));

        check("G1/unknown-directive-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"directive.recipe", oneRecipe(
                          "recipe hellomine:directive shapeless\n"
                          "ingredient hellomine:stone 1\n"
                          "output hellomine:dirt 1\nend\n")}});
                  },
                  "unknown or misplaced recipe directive"));

        check("G1/invalid-output-count-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"output-count.recipe", oneRecipe(
                          "recipe hellomine:output_count shapeless\n"
                          "input hellomine:stone 1\n"
                          "output hellomine:dirt 0\nend\n")}});
                  },
                  "output count must be"));
    }

    void caseRegistryRecipeLimit()
    {
        std::string source = "# HelloMine3D recipe registry v1\n";
        constexpr int materialCount =
            static_cast<int>(Material::ID::Count) - 1;
        for (std::size_t index = 0;
             index <= RecipeRegistry::MaxRecipes; ++index) {
            const int first =
                1 + static_cast<int>(index % materialCount);
            const int second =
                1 + static_cast<int>((index / materialCount) %
                                     materialCount);
            source += "recipe hellomine:limit_" +
                      std::to_string(index) + " shaped\nrow ";
            source += Material::toStringId(
                static_cast<Material::ID>(first));
            source += " ";
            source += Material::toStringId(
                static_cast<Material::ID>(second));
            source += "\noutput hellomine:dirt 1\nend\n";
        }
        check("G1/registry-recipe-count-is-bounded",
              throwsContaining(
                  [&]
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"limit.recipe", source}});
                  },
                  "exceeds 256 entries"));
    }

    void caseUtf8BomAndCrLf()
    {
        const std::string source =
            "\xEF\xBB\xBF# HelloMine3D recipe registry v1\r\n"
            "# comment\r\n"
            "recipe hellomine:bom shapeless\r\n"
            "input hellomine:stone 1\r\n"
            "output hellomine:dirt 1\r\n"
            "end\r\n";
        RecipeRegistry registry;
        registry.freeze({{"bom.recipe", source}});
        check("G1/utf8-bom-and-crlf-supported",
              registry.find("hellomine:bom") != nullptr);
    }

    void caseDuplicates()
    {
        const std::string first = oneRecipe(
            "recipe hellomine:same shapeless\n"
            "input hellomine:stone 1\n"
            "output hellomine:dirt 1\nend\n");
        const std::string second = oneRecipe(
            "recipe hellomine:same shaped\n"
            "row hellomine:dirt\n"
            "output hellomine:stone 1\nend\n");
        check("G1/duplicate-id-is-deterministic",
              throwsContaining(
                  [&]
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"z.recipe", second},
                                       {"a.recipe", first}});
                  },
                  "first declared in 'a.recipe'"));

        check("G1/equivalent-shaped-pattern-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"shaped.recipe", oneRecipe(
                          "recipe hellomine:first shaped\n"
                          "row _ hellomine:stone _\n"
                          "output hellomine:dirt 1\nend\n"
                          "recipe hellomine:second shaped\n"
                          "row hellomine:stone\n"
                          "output hellomine:grass 1\nend\n")}});
                  },
                  "Duplicate recipe pattern"));
        check("G1/equivalent-shapeless-pattern-rejected",
              throwsContaining(
                  []
                  {
                      RecipeRegistry registry;
                      registry.freeze({{"shapeless.recipe", oneRecipe(
                          "recipe hellomine:first shapeless\n"
                          "input hellomine:stone 1\n"
                          "input hellomine:dirt 2\n"
                          "output hellomine:grass 1\nend\n"
                          "recipe hellomine:second shapeless\n"
                          "input hellomine:dirt 2\n"
                          "input hellomine:stone 1\n"
                          "output hellomine:sand 1\nend\n")}});
                  },
                  "Duplicate recipe pattern"));
    }

    void caseAtomicFailureAndLimits()
    {
        RecipeRegistry registry;
        const bool failed = throwsContaining(
            [&]
            {
                registry.freeze({{"bad.recipe", "not a recipe\n"}});
            },
            "unsupported or missing header");
        registry.freeze({{"valid.recipe", validRecipes()}});
        check("G1/failed-freeze-is-atomic",
              failed && registry.isFrozen() && registry.recipes().size() == 2);

        check("G1/source-size-is-bounded",
              throwsContaining(
                  []
                  {
                      RecipeRegistry oversized;
                      oversized.freeze({{"huge.recipe",
                          std::string(RecipeRegistry::MaxSourceBytes + 1,
                                      'x')}});
                  },
                  "exceeds"));
    }

    void caseFrozenBaseResourceView()
    {
        const fs::path root = fs::current_path() / "bin" /
                              "validation_runs" / "recipes";
        std::error_code error;
        fs::remove_all(root, error);
        const std::string logicalPath = "media/recipes/Base.recipe";
        writeFile(root / logicalPath, validRecipes());
        const std::vector<ResourcePackRequirement> requirements = {
            {"recipe", logicalPath}};

        ResourcePackResolver resolver;
        resolver.freeze(root.string(), requirements, {});
        RecipeRegistry registry;
        registry.freezeFromResourceView(resolver);
        check("G1/frozen-base-resource-loads",
              registry.isFrozen() && registry.recipes().size() == 2 &&
                  resolver.effectiveManifest().find(
                      "recipe|media/recipes/Base.recipe|base\n") !=
                      std::string::npos);

        ResourcePackResolver unfrozen;
        RecipeRegistry blocked;
        check("G1/unfrozen-resource-view-rejected",
              throwsContaining(
                  [&]
                  {
                      blocked.freezeFromResourceView(unfrozen);
                  },
                  "requires a frozen"));

        fs::remove(root / logicalPath, error);
        check("G1/missing-base-resource-rejected",
              throwsContaining(
                  [&]
                  {
                      ResourcePackResolver missing;
                      missing.freeze(root.string(), requirements, {});
                  },
                  "Missing or empty effective recipe resource"));
    }

    RecipeRegistry craftingRecipes()
    {
        RecipeRegistry registry;
        registry.freeze({{"crafting.recipe", oneRecipe(
            "recipe hellomine:glass_upgrade shapeless\n"
            "input hellomine:glass 1\n"
            "output hellomine:glass_borderless 1\nend\n"
            "recipe hellomine:compact shaped\n"
            "row hellomine:stone\nrow hellomine:dirt\n"
            "output hellomine:grass 1\nend\n"
            "recipe hellomine:chest shaped\n"
            "row hellomine:oak_bark hellomine:oak_bark hellomine:oak_bark\n"
            "row hellomine:oak_bark _ hellomine:oak_bark\n"
            "row hellomine:oak_bark hellomine:oak_bark hellomine:oak_bark\n"
            "output hellomine:chest 1\nend\n")}});
        return registry;
    }

    void caseCraftingSession()
    {
        const RecipeRegistry recipes = craftingRecipes();
        Inventory inventory;
        inventory.addItem(Material::GLASS_BLOCK, 3);
        CraftingSession session(CraftingSession::PlayerGridSize);
        session.setCell(0, Material::ID::Glass);
        const std::vector<InventorySlotState> beforePreview =
            inventory.getSaveState();
        const std::uint64_t sessionBefore = session.version();
        const CraftingPreview preview = session.preview(recipes, inventory);
        check("G2/preview-is-pure-and-counts-maximum",
              preview.ready() && preview.maxCrafts == 3 &&
                  preview.outputMaterialId ==
                      Material::ID::GlassBorderless &&
                  inventory.getSaveState() == beforePreview &&
                  session.version() == sessionBefore);

        const CraftingCommitResult crafted =
            session.commit(recipes, inventory, preview, 1);
        check("G2/single-craft-consumes-and-produces-atomically",
              crafted.succeeded() && crafted.craftsCompleted == 1 &&
                  inventory.count(Material::ID::Glass) == 2 &&
                  inventory.count(Material::ID::GlassBorderless) == 1);
        check("G2/stale-session-preview-is-rejected",
              session.commit(recipes, inventory, preview, 1).status ==
                  CraftingCommitStatus::StaleSession);

        const CraftingPreview beforeInventoryChange =
            session.preview(recipes, inventory);
        inventory.addItem(Material::DIRT_BLOCK, 1);
        const std::vector<InventorySlotState> afterInventoryChange =
            inventory.getSaveState();
        check("G2/stale-inventory-preview-is-rejected-without-mutation",
              session.commit(recipes, inventory, beforeInventoryChange, 1)
                      .status == CraftingCommitStatus::StaleInventory &&
                  inventory.getSaveState() == afterInventoryChange);

        const CraftingPreview batch = session.preview(recipes, inventory);
        const CraftingCommitResult batchResult =
            session.commit(recipes, inventory, batch, batch.maxCrafts);
        check("G2/batch-craft-reuses-bounded-atomic-rule",
              batch.maxCrafts == 2 && batchResult.succeeded() &&
                  inventory.count(Material::ID::Glass) == 0 &&
                  inventory.count(Material::ID::GlassBorderless) == 3);

        Inventory missing;
        const CraftingPreview missingPreview =
            session.preview(recipes, missing);
        check("G2/insufficient-ingredients-are-reported",
              missingPreview.status ==
                      CraftingPreviewStatus::MissingIngredients &&
                  missingPreview.maxCrafts == 0);

        Inventory full;
        full.applySaveState(
            {{Material::ID::Glass, 2},
             {Material::ID::Dirt, 99},
             {Material::ID::Stone, 99},
             {Material::ID::Sand, 99},
             {Material::ID::Wheat, 99}},
            0);
        const std::vector<InventorySlotState> fullBefore =
            full.getSaveState();
        const CraftingPreview fullPreview = session.preview(recipes, full);
        check("G2/full-output-inventory-fails-without-consuming-input",
              fullPreview.status == CraftingPreviewStatus::OutputFull &&
                  !session.commit(recipes, full, fullPreview, 1)
                       .succeeded() &&
                  full.getSaveState() == fullBefore);

        Inventory freedSlot;
        freedSlot.applySaveState(
            {{Material::ID::Glass, 1},
             {Material::ID::Dirt, 99},
             {Material::ID::Stone, 99},
             {Material::ID::Sand, 99},
             {Material::ID::Wheat, 99}},
            0);
        const CraftingPreview freedPreview =
            session.preview(recipes, freedSlot);
        check("G2/consumed-stack-can-free-output-capacity",
              freedPreview.ready() && freedPreview.maxCrafts == 1 &&
                  session.commit(recipes, freedSlot, freedPreview, 1)
                      .succeeded() &&
                  freedSlot.count(Material::ID::GlassBorderless) == 1);

        CraftingSession playerGrid(CraftingSession::PlayerGridSize);
        for (int index = 0; index < playerGrid.cellCount(); ++index) {
            playerGrid.setCell(index, Material::ID::OakBark);
        }
        Inventory bark;
        bark.addItem(Material::OAK_BARK_BLOCK, 8);
        check("G2/player-grid-rejects-three-by-three-recipe",
              playerGrid.preview(recipes, bark).status ==
                  CraftingPreviewStatus::NoMatch);

        CraftingSession workbench(CraftingSession::WorkbenchGridSize);
        for (int index = 0; index < workbench.cellCount(); ++index) {
            if (index != 4) {
                workbench.setCell(index, Material::ID::OakBark);
            }
        }
        const CraftingPreview chest = workbench.preview(recipes, bark);
        check("G2/workbench-matches-three-by-three-recipe",
              chest.ready() && chest.recipeId == "hellomine:chest" &&
                  chest.maxCrafts == 1);

        CraftingSession offset(CraftingSession::WorkbenchGridSize);
        offset.setCell(5, Material::ID::Stone);
        offset.setCell(8, Material::ID::Dirt);
        Inventory compactInputs;
        compactInputs.addItem(Material::STONE_BLOCK, 1);
        compactInputs.addItem(Material::DIRT_BLOCK, 1);
        check("G2/shaped-recipe-normalizes-grid-offset",
              offset.preview(recipes, compactInputs).recipeId ==
                  "hellomine:compact");

        const std::vector<InventorySlotState> beforeClear =
            compactInputs.getSaveState();
        offset.clear();
        check("G2/closing-virtual-grid-never-holds-player-items",
              compactInputs.getSaveState() == beforeClear &&
                  offset.preview(recipes, compactInputs).status ==
                      CraftingPreviewStatus::NoMatch);

        Inventory reloaded;
        reloaded.applySaveState(inventory.getSaveState(), 0);
        check("G2/crafted-output-survives-inventory-reload",
              reloaded.count(Material::ID::GlassBorderless) == 3 &&
                  reloaded.count(Material::ID::Glass) == 0);
        check("G2/invalid-grid-and-batch-values-are-rejected",
              !session.setCell(-1, Material::ID::Stone) &&
                  !session.setCell(0, Material::ID::Nothing) &&
                  !session.commit(recipes, inventory,
                                  session.preview(recipes, inventory), 0)
                       .succeeded());
    }

    void caseToolProgression()
    {
        ToolRegistry registry;
        registry.freeze({{"base.tool", validTools()}});
        const ToolDefinition *wood =
            registry.find(Material::ID::WoodenPickaxe);
        const ToolDefinition *stone =
            registry.find(Material::ID::StonePickaxe);
        const ToolDefinition *iron =
            registry.find(Material::ID::IronPickaxe);
        const ToolDefinition *sword =
            registry.find(Material::ID::IronSword);
        check("G3/tool-registry-freezes-complete-base-set",
              registry.isFrozen() && registry.tools().size() == 4 &&
                  wood != nullptr && stone != nullptr && iron != nullptr &&
                  sword != nullptr);
        check("G3/tool-stats-are-data-driven",
              wood != nullptr && wood->miningClass == MiningClass::Pickaxe &&
                  wood->tier == 1 && wood->speedMultiplier == 2.0f &&
                  wood->maxDurability == 16 && stone != nullptr &&
                  stone->tier == 2 && stone->speedMultiplier == 4.0f &&
                  stone->maxDurability == 32 && iron != nullptr &&
                  iron->tier == 3 && iron->speedMultiplier == 6.0f &&
                  iron->maxDurability == 64 && iron->attackDamage == 4.0f &&
                  sword != nullptr &&
                  sword->miningClass == MiningClass::Weapon &&
                  sword->tier == 3 && sword->maxDurability == 80 &&
                  sword->attackDamage == 7.0f);
        check("G3/tool-registry-is-startup-frozen",
              throwsContaining(
                  [&registry]
                  {
                      registry.freeze({{"again.tool", validTools()}});
                  },
                  "already frozen"));

        const std::string duplicate = validTools() +
            "tool hellomine:wooden_pickaxe\nclass pickaxe\n"
            "tier 1\nspeed 2\ndurability 16\nend\n";
        check("G3/duplicate-tool-is-rejected",
              throwsContaining(
                  [&duplicate]
                  {
                      ToolRegistry invalid;
                      invalid.freeze({{"duplicate.tool", duplicate}});
                  },
                  "Duplicate tool material"));
        check("G3/missing-tool-definition-is-rejected",
              throwsContaining(
                  []
                  {
                      ToolRegistry invalid;
                      invalid.freeze({{"missing.tool",
                          "# HelloMine3D tool registry v1\n"
                          "tool hellomine:wooden_pickaxe\n"
                          "class pickaxe\ntier 1\nspeed 2\n"
                          "durability 16\nend\n"}});
                  },
                  "Missing tool definition"));
        check("G3/invalid-tool-speed-is-rejected",
              throwsContaining(
                  []
                  {
                      std::string source = validTools();
                      source.replace(source.find("speed 2"), 7,
                                     "speed 99");
                      ToolRegistry invalid;
                      invalid.freeze({{"speed.tool", source}});
                  },
                  "speed must be in"));
        check("N2/invalid-tool-attack-is-rejected",
              throwsContaining(
                  []
                  {
                      std::string source = validTools();
                      source.replace(source.find("attack 7"), 8,
                                     "attack 65");
                      ToolRegistry invalid;
                      invalid.freeze({{"attack.tool", source}});
                  },
                  "attack must be in"));

        RecipeRegistry recipes;
        recipes.freeze({{"tools.recipe", oneRecipe(
            "recipe hellomine:wooden_pickaxe shaped\n"
            "row hellomine:oak_bark hellomine:oak_bark hellomine:oak_bark\n"
            "row _ hellomine:oak_bark _\n"
            "row _ hellomine:oak_bark _\n"
            "output hellomine:wooden_pickaxe 1\nend\n"
            "recipe hellomine:stone_pickaxe shaped\n"
            "row hellomine:stone hellomine:stone hellomine:stone\n"
            "row _ hellomine:oak_bark _\n"
            "row _ hellomine:oak_bark _\n"
            "output hellomine:stone_pickaxe 1\nend\n")}});
        Inventory inventory;
        inventory.addItem(Material::OAK_BARK_BLOCK, 7);
        inventory.addItem(Material::STONE_BLOCK, 3);
        CraftingSession session(CraftingSession::WorkbenchGridSize);
        session.setCell(0, Material::ID::OakBark);
        session.setCell(1, Material::ID::OakBark);
        session.setCell(2, Material::ID::OakBark);
        session.setCell(4, Material::ID::OakBark);
        session.setCell(7, Material::ID::OakBark);
        const CraftingPreview woodPreview = session.preview(recipes, inventory);
        const CraftingCommitResult woodResult =
            session.commit(recipes, inventory, woodPreview, 1);
        int woodenDurability = 0;
        for (const InventorySlotState &slot : inventory.getSaveState()) {
            if (slot.materialId == Material::ID::WoodenPickaxe) {
                woodenDurability = slot.durability;
            }
        }
        check("G3/workbench-crafts-full-durability-wooden-pickaxe",
              woodPreview.recipeId == "hellomine:wooden_pickaxe" &&
                  woodResult.succeeded() && woodenDurability == 16);

        session.clear();
        session.setCell(0, Material::ID::Stone);
        session.setCell(1, Material::ID::Stone);
        session.setCell(2, Material::ID::Stone);
        session.setCell(4, Material::ID::OakBark);
        session.setCell(7, Material::ID::OakBark);
        const CraftingPreview stonePreview = session.preview(recipes, inventory);
        const CraftingCommitResult stoneResult =
            session.commit(recipes, inventory, stonePreview, 1);
        int stoneDurability = 0;
        for (const InventorySlotState &slot : inventory.getSaveState()) {
            if (slot.materialId == Material::ID::StonePickaxe) {
                stoneDurability = slot.durability;
            }
        }
        check("G3/crafting-chain-produces-stone-pickaxe",
              stonePreview.recipeId == "hellomine:stone_pickaxe" &&
                  stoneResult.succeeded() && stoneDurability == 32 &&
                  inventory.count(Material::ID::Stone) == 0);

        Inventory unstackable(2);
        check("G3/tools-are-unstackable-per-slot",
              unstackable.addItem(Material::WOODEN_PICKAXE, 2) == 2 &&
                  unstackable.getSlot(0).getNumInStack() == 1 &&
                  unstackable.getSlot(1).getNumInStack() == 1);
        Inventory damaged(1);
        damaged.addItem(Material::WOODEN_PICKAXE, 1, 2);
        const auto firstDamage = damaged.damageSelectedTool();
        const auto secondDamage = damaged.damageSelectedTool();
        check("G3/durability-decrements-and-breaks-at-zero",
              firstDamage == Inventory::ToolDamageResult::Damaged &&
                  secondDamage == Inventory::ToolDamageResult::Broken &&
                  damaged.getSelectedStack().isEmpty());
    }
}

int main()
{
    runtimeToolRegistry().freeze({{"runtime.tool", validTools()}});
    caseMaterialIds();
    caseValidAndFrozen();
    caseStrictParsing();
    caseIdsAndSourceBounds();
    caseDirectiveCompleteness();
    caseRegistryRecipeLimit();
    caseUtf8BomAndCrLf();
    caseDuplicates();
    caseAtomicFailureAndLimits();
    caseFrozenBaseResourceView();
    caseCraftingSession();
    caseToolProgression();
    constexpr int ExpectedChecks = 65;
    if (checks != ExpectedChecks) {
        ++failures;
        std::cout << "[RECIPE_TEST] FAIL G1/expected-check-count"
                  << " :: expected=" << ExpectedChecks
                  << " actual=" << checks << '\n';
    }
    std::cout << "[RECIPE_TEST] checks=" << checks
              << " failures=" << failures << '\n';
    std::cout << "[RECIPE_TEST] status="
              << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
