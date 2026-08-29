#include "../Item/RecipeRegistry.h"
#include "../Item/CraftingSession.h"
#include "../Item/FoodRegistry.h"
#include "../Item/ResourceEconomyVerifier.h"
#include "../Item/SmeltingRegistry.h"
#include "../Item/ToolRegistry.h"
#include "../Actor/EnemyRegistry.h"
#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
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
attack 2
attack_cooldown 12
attack_reach 3
end
tool hellomine:stone_pickaxe
class pickaxe
tier 2
speed 4
durability 32
attack 3
attack_cooldown 11
attack_reach 3
end
tool hellomine:iron_pickaxe
class pickaxe
tier 3
speed 6
durability 64
attack 4
attack_cooldown 10
attack_reach 3
end
tool hellomine:iron_sword
class weapon
tier 3
speed 1
durability 80
attack 7
attack_cooldown 8
attack_reach 3.75
end
tool hellomine:wooden_sword
class weapon
tier 1
speed 1
durability 32
attack 4
attack_cooldown 10
attack_reach 3.25
end
tool hellomine:stone_sword
class weapon
tier 2
speed 1
durability 56
attack 5
attack_cooldown 9
attack_reach 3.5
end
tool hellomine:wooden_axe
class axe
tier 1
speed 3
durability 16
attack 3
attack_cooldown 11
attack_reach 3
end
tool hellomine:wooden_shovel
class shovel
tier 1
speed 4
durability 16
attack 2
attack_cooldown 12
attack_reach 3
end
)";
    }

    std::string validEnemies()
    {
        return R"(# HelloMine3D enemy registry v3
enemy hellomine:natural_mob
health 10
dimensions 0.35 0.9 0.35
wander_speed 1.2
chase_radius 12
chase_speed 2.4
contact_damage 2
combat_mode melee
attack_range 0.75
attack_windup_ticks 8
attack_recover_ticks 8
attack_cooldown_ticks 20
knockback 2.5
natural 0
loot hellomine:dirt 1 1
end
enemy hellomine:stalker
health 8
dimensions 0.30 0.75 0.30
wander_speed 1.6
chase_radius 14
chase_speed 3.2
contact_damage 1
combat_mode melee
attack_range 0.75
attack_windup_ticks 6
attack_recover_ticks 7
attack_cooldown_ticks 16
knockback 2
natural 1
loot hellomine:dirt 1 1
loot hellomine:wheat 1 2
loot hellomine:raw_meat 1 1
end
enemy hellomine:brute
health 16
dimensions 0.45 1.05 0.45
wander_speed 0.8
chase_radius 10
chase_speed 1.6
contact_damage 4
combat_mode melee
attack_range 0.9
attack_windup_ticks 14
attack_recover_ticks 12
attack_cooldown_ticks 28
knockback 4
natural 1
loot hellomine:dirt 1 1
loot hellomine:coal_ore 1 1
loot hellomine:wheat 1 1
loot hellomine:raw_meat 1 2
end
enemy hellomine:spitter
health 7
dimensions 0.32 0.80 0.32
wander_speed 1.1
chase_radius 18
chase_speed 2
contact_damage 0
combat_mode ranged
attack_range 12
attack_windup_ticks 12
attack_recover_ticks 10
attack_cooldown_ticks 32
knockback 1.5
projectile_speed 10
projectile_damage 2
projectile_lifetime_ticks 50
projectile_max_distance 20
projectile_radius 0.15
projectile_world_limit 24
projectile_local_limit 8
projectile_active_radius 32
natural 1
loot hellomine:dirt 1 1
loot hellomine:wheat_seeds 1 2
end
)";
    }

    std::string validFoods()
    {
        return R"(# HelloMine3D food registry v1
food hellomine:bread
restore 6
cooldown_ticks 20
end
food hellomine:cooked_meat
restore 9
cooldown_ticks 28
end
food hellomine:cactus_salad
restore 4
cooldown_ticks 12
end
food hellomine:trail_ration
restore 14
cooldown_ticks 40
end
)";
    }

    std::string validSmelting()
    {
        return R"(# HelloMine3D smelting registry v1
smelt hellomine:iron_ingot
input hellomine:iron_ore
output hellomine:iron_ingot 1
ticks 100
end
smelt hellomine:cooked_meat
input hellomine:raw_meat
output hellomine:cooked_meat 1
ticks 60
end
smelt hellomine:glass
input hellomine:sand
output hellomine:glass 1
ticks 80
end
fuel hellomine:coal_ore
ticks 160
end
fuel hellomine:plant_fiber
ticks 40
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
                  static_cast<int>(Material::ID::Torch) == 34 &&
                  static_cast<int>(Material::ID::OakPlank) == 35 &&
                  static_cast<int>(Material::ID::Cobblestone) == 36 &&
                  static_cast<int>(Material::ID::OakDoor) == 37 &&
                  static_cast<int>(Material::ID::WoodenAxe) == 38 &&
                  static_cast<int>(Material::ID::WoodenShovel) == 39 &&
                  static_cast<int>(Material::ID::AncientCompass) == 40 &&
                  static_cast<int>(Material::ID::RaiderWard) == 41 &&
                  Material::BREAD.isFood && !Material::BREAD.isTool &&
                  Material::COOKED_MEAT.isFood &&
                  Material::CACTUS_SALAD.isFood &&
                  Material::TRAIL_RATION.isFood &&
                  !Material::RAW_MEAT.isFood &&
                  !Material::PLANT_FIBER.isFood &&
                  Material::WOODEN_SWORD.isTool &&
                  Material::STONE_SWORD.isTool &&
                  Material::WAYSTONE_CORE.isBlock &&
                  Material::WAYSTONE_CORE.toBlockID() ==
                      BlockId::WaystoneCore &&
                  Material::toMaterial(BlockId::WaystoneCore).id ==
                      Material::ID::WaystoneCore &&
                  Material::TORCH.isBlock &&
                  Material::TORCH.toBlockID() == BlockId::Torch &&
                  Material::toMaterial(BlockId::Torch).id ==
                      Material::ID::Torch &&
                  Material::WOODEN_AXE.isTool &&
                  Material::WOODEN_SHOVEL.isTool &&
                  Material::OAK_DOOR.toBlockID() ==
                      BlockId::OakDoorClosed &&
                  Material::toMaterial(BlockId::OakDoorOpen).id ==
                      Material::ID::OakDoor &&
                  static_cast<int>(BlockId::Torch) == 21 &&
                  static_cast<int>(BlockId::OakDoorOpen) == 25);
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

        CraftingSession recipeBook(CraftingSession::PlayerGridSize);
        const RecipeDefinition *compact =
            recipes.find("hellomine:compact");
        const std::vector<InventorySlotState> inventoryBeforeLoad =
            inventory.getSaveState();
        check("N6/recipe-book-loads-shaped-pattern-without-inventory-use",
              compact != nullptr && recipeBook.loadRecipe(*compact) &&
                  recipeBook.cell(0).materialId == Material::ID::Stone &&
                  recipeBook.cell(2).materialId == Material::ID::Dirt &&
                  inventory.getSaveState() == inventoryBeforeLoad);
        const RecipeDefinition *shapeless =
            recipes.find("hellomine:glass_upgrade");
        check("N6/recipe-book-loads-shapeless-pattern",
              shapeless != nullptr && recipeBook.loadRecipe(*shapeless) &&
                  recipeBook.cell(0).materialId == Material::ID::Glass &&
                  recipeBook.cell(1).materialId == Material::ID::Nothing);
        const RecipeDefinition *large = recipes.find("hellomine:chest");
        const InventorySlotState beforeRejected = recipeBook.cell(0);
        check("N6/recipe-book-rejects-pattern-larger-than-grid",
              large != nullptr && !recipeBook.loadRecipe(*large) &&
                  recipeBook.cell(0) == beforeRejected);
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
        const ToolDefinition *woodenSword =
            registry.find(Material::ID::WoodenSword);
        const ToolDefinition *stoneSword =
            registry.find(Material::ID::StoneSword);
        const ToolDefinition *woodenAxe =
            registry.find(Material::ID::WoodenAxe);
        const ToolDefinition *woodenShovel =
            registry.find(Material::ID::WoodenShovel);
        check("G3/tool-registry-freezes-complete-base-set",
              registry.isFrozen() && registry.tools().size() == 8 &&
                  wood != nullptr && stone != nullptr && iron != nullptr &&
                  sword != nullptr && woodenSword != nullptr &&
                  stoneSword != nullptr && woodenAxe != nullptr &&
                  woodenShovel != nullptr);
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
                  sword->attackDamage == 7.0f &&
                  sword->attackCooldownTicks == 8 &&
                  sword->attackReach == 3.75f &&
                  woodenSword->attackDamage == 4.f &&
                  woodenSword->attackCooldownTicks == 10 &&
                  woodenSword->attackReach == 3.25f &&
                  stoneSword->attackDamage == 5.f &&
                  stoneSword->attackCooldownTicks == 9 &&
                  stoneSword->attackReach == 3.5f &&
                  woodenAxe->miningClass == MiningClass::Axe &&
                  woodenAxe->speedMultiplier == 3.f &&
                  woodenShovel->miningClass == MiningClass::Shovel &&
                  woodenShovel->speedMultiplier == 4.f);
        check("G3/tool-registry-is-startup-frozen",
              throwsContaining(
                  [&registry]
                  {
                      registry.freeze({{"again.tool", validTools()}});
                  },
                  "already frozen"));

        const std::string duplicate = validTools() +
            "tool hellomine:wooden_pickaxe\nclass pickaxe\n"
            "tier 1\nspeed 2\ndurability 16\nattack 2\n"
            "attack_cooldown 12\nattack_reach 3\nend\n";
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
                          "durability 16\nattack 2\n"
                          "attack_cooldown 12\nattack_reach 3\nend\n"}});
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
        check("N4/invalid-attack-cooldown-is-rejected",
              throwsContaining(
                  []
                  {
                      std::string source = validTools();
                      source.replace(source.find("attack_cooldown 8"), 17,
                                     "attack_cooldown 0");
                      ToolRegistry invalid;
                      invalid.freeze({{"cooldown.tool", source}});
                  },
                  "attack_cooldown must be in"));
        check("N4/invalid-attack-reach-is-rejected",
              throwsContaining(
                  []
                  {
                      std::string source = validTools();
                      source.replace(source.find("attack_reach 3.75"), 17,
                                     "attack_reach 7");
                      ToolRegistry invalid;
                      invalid.freeze({{"reach.tool", source}});
                  },
                  "attack_reach must be in"));

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

    void caseFoodRegistry()
    {
        FoodRegistry registry;
        registry.freeze({{"base.food", validFoods()}});
        const FoodDefinition *bread = registry.find(Material::ID::Bread);
        const FoodDefinition *cooked =
            registry.find(Material::ID::CookedMeat);
        const FoodDefinition *salad =
            registry.find(Material::ID::CactusSalad);
        const FoodDefinition *ration =
            registry.find(Material::ID::TrailRation);
        check("N10/food-registry-freezes-expanded-base-set",
              registry.isFrozen() && registry.foods().size() == 4 &&
                  bread != nullptr && cooked != nullptr &&
                  salad != nullptr && ration != nullptr);
        check("N3/food-stats-are-data-driven",
              bread != nullptr && bread->healthRestored == 6.f &&
                  bread->cooldownTicks == 20);
        check("N10/food-choices-have-distinct-recovery-and-cooldown",
              cooked->healthRestored == 9.f &&
                  cooked->cooldownTicks == 28 &&
                  salad->healthRestored == 4.f &&
                  salad->cooldownTicks == 12 &&
                  ration->healthRestored == 14.f &&
                  ration->cooldownTicks == 40);
        check("N3/food-registry-is-startup-frozen",
              throwsContaining(
                  [&registry]
                  {
                      registry.freeze({{"again.food", validFoods()}});
                  },
                  "already frozen"));

        check("N3/food-header-is-strict",
              throwsContaining(
                  []
                  {
                      FoodRegistry invalid;
                      invalid.freeze({{"bad.food", "food bread\n"}});
                  },
                  "unsupported or missing header"));
        check("N3/non-food-material-is-rejected",
              throwsContaining(
                  []
                  {
                      FoodRegistry invalid;
                      invalid.freeze({{"bad.food",
                          "# HelloMine3D food registry v1\n"
                          "food hellomine:wheat\nrestore 2\n"
                          "cooldown_ticks 10\nend\n"}});
                  },
                  "registered as food"));
        check("N3/incomplete-food-is-rejected",
              throwsContaining(
                  []
                  {
                      FoodRegistry invalid;
                      invalid.freeze({{"bad.food",
                          "# HelloMine3D food registry v1\n"
                          "food hellomine:bread\nrestore 2\nend\n"}});
                  },
                  "missing restore or cooldown_ticks"));
        check("N3/duplicate-food-is-rejected",
              throwsContaining(
                  []
                  {
                      FoodRegistry invalid;
                      invalid.freeze({{"a.food", validFoods()},
                                      {"b.food", validFoods()}});
                  },
                  "Duplicate food material"));
        check("N3/food-bounds-are-strict",
              throwsContaining(
                  []
                  {
                      std::string source = validFoods();
                      source.replace(source.find("restore 6"), 9,
                                     "restore 21");
                      FoodRegistry invalid;
                      invalid.freeze({{"bad.food", source}});
                  },
                  "restore must be in") &&
              throwsContaining(
                  []
                  {
                      std::string source = validFoods();
                      source.replace(source.find("cooldown_ticks 20"), 17,
                                     "cooldown_ticks 0");
                      FoodRegistry invalid;
                      invalid.freeze({{"bad.food", source}});
                  },
                  "cooldown_ticks must be in"));

        FoodRegistry atomic;
        const bool failed = throwsContaining(
            [&atomic]
            {
                atomic.freeze({{"bad.food", "invalid\n"}});
            },
            "unsupported or missing header");
        atomic.freeze({{"valid.food", validFoods()}});
        check("N3/failed-food-freeze-is-atomic",
              failed && atomic.isFrozen() && atomic.foods().size() == 4);

        const fs::path root = fs::current_path() / "bin" /
                              "validation_runs" / "foods";
        std::error_code error;
        fs::remove_all(root, error);
        const std::string logicalPath = "media/foods/Base.food";
        writeFile(root / logicalPath, validFoods());
        ResourcePackResolver resolver;
        resolver.freeze(root.string(), {{"food", logicalPath}}, {});
        FoodRegistry fromView;
        fromView.freezeFromResourceView(resolver);
        check("N3/frozen-food-resource-loads",
              fromView.isFrozen() && fromView.foods().size() == 4 &&
                  resolver.effectiveManifest().find(
                      "food|media/foods/Base.food|base\n") !=
                      std::string::npos);

        std::ifstream baseRecipes(
            ResourcePaths::media("recipes/Base.recipe"),
            std::ios::binary);
        std::ostringstream baseContent;
        baseContent << baseRecipes.rdbuf();
        RecipeRegistry recipes;
        bool recipeLoaded = false;
        try {
            recipes.freeze({{"Base.recipe", baseContent.str()}});
            recipeLoaded = true;
        }
        catch (const std::exception &) {
        }
        const RecipeDefinition *breadRecipe =
            recipeLoaded ? recipes.find("hellomine:bread") : nullptr;
        check("N3/base-bread-recipe-consumes-three-wheat",
              breadRecipe != nullptr &&
                  breadRecipe->outputMaterialId == Material::ID::Bread &&
                  breadRecipe->outputCount == 1 &&
                  breadRecipe->ingredients.size() == 1 &&
                  breadRecipe->ingredients[0].materialId ==
                      Material::ID::Wheat &&
                  breadRecipe->ingredients[0].count == 3);
    }

    void caseEnemyRegistry()
    {
        EnemyRegistry registry;
        registry.freeze({{"Base.enemy", validEnemies()}});
        const EnemyDefinition *stalker = registry.find("hellomine:stalker");
        const EnemyDefinition *brute = registry.find("hellomine:brute");
        const EnemyDefinition *spitter = registry.find("hellomine:spitter");
        const std::vector<const EnemyDefinition *> natural =
            registry.naturalEnemies();
        check("N4/enemy-registry-freezes-complete-base-set",
              registry.isFrozen() && registry.enemies().size() == 4 &&
                  stalker != nullptr && brute != nullptr &&
                  spitter != nullptr && natural.size() == 3);
        check("N4/enemy-archetypes-are-readable-and-data-driven",
              stalker->maxHealth == 8.f && stalker->wanderSpeed == 1.6f &&
                  stalker->chaseSpeed == 3.2f &&
                  stalker->contactDamage == 1.f &&
                  stalker->loot.size() == 3 &&
                  stalker->loot[0].materialId == Material::ID::Dirt &&
                  stalker->loot[1].materialId == Material::ID::Wheat &&
                  stalker->loot[1].minimumAmount == 1 &&
                  stalker->loot[1].maximumAmount == 2 &&
                  stalker->loot[2].materialId == Material::ID::RawMeat &&
                  brute->maxHealth == 16.f && brute->wanderSpeed == 0.8f &&
                  brute->chaseSpeed == 1.6f &&
                  brute->contactDamage == 4.f && brute->loot.size() == 4 &&
                  brute->loot[0].materialId == Material::ID::Dirt &&
                  brute->loot[1].materialId == Material::ID::CoalOre &&
                  brute->loot[2].materialId == Material::ID::Wheat &&
                  brute->loot[3].materialId == Material::ID::RawMeat &&
                  natural[0]->type == "hellomine:brute" &&
                  natural[1]->type == "hellomine:spitter" &&
                  natural[2]->type == "hellomine:stalker");
        check("N8A/melee-combat-profiles-are-versioned-and-distinct",
              stalker != nullptr && brute != nullptr &&
                  stalker->combat.mode == EnemyCombatMode::Melee &&
                  stalker->combat.attackRange == 0.75f &&
                  stalker->combat.windupTicks == 6 &&
                  stalker->combat.recoverTicks == 7 &&
                  stalker->combat.cooldownTicks == 16 &&
                  stalker->combat.knockback == 2.f &&
                  brute->combat.attackRange == 0.9f &&
                  brute->combat.windupTicks == 14 &&
                  brute->combat.recoverTicks == 12 &&
                  brute->combat.cooldownTicks == 28 &&
                  brute->combat.knockback == 4.f);
        check("N8B/ranged-combat-profile-is-complete-and-bounded",
              spitter != nullptr &&
                  spitter->combat.mode == EnemyCombatMode::Ranged &&
                  spitter->combat.attackRange == 12.f &&
                  spitter->combat.windupTicks == 12 &&
                  spitter->combat.recoverTicks == 10 &&
                  spitter->combat.cooldownTicks == 32 &&
                  spitter->combat.knockback == 1.5f &&
                  spitter->combat.projectileSpeed == 10.f &&
                  spitter->combat.projectileDamage == 2.f &&
                  spitter->combat.projectileLifetimeTicks == 50 &&
                  spitter->combat.projectileMaxDistance == 20.f &&
                  spitter->combat.projectileRadius == 0.15f &&
                  spitter->combat.projectileWorldLimit == 24 &&
                  spitter->combat.projectileLocalLimit == 8 &&
                  spitter->combat.projectileActiveRadius == 32.f);
        std::ifstream baseEnemyInput(
            ResourcePaths::media("enemies/Base.enemy"),
            std::ios::binary);
        std::ostringstream baseEnemyContent;
        baseEnemyContent << baseEnemyInput.rdbuf();
        EnemyRegistry baseRegistry;
        bool baseLoaded = false;
        try {
            baseRegistry.freeze(
                {{"Base.enemy", baseEnemyContent.str()}});
            baseLoaded = true;
        }
        catch (const std::exception &) {
        }
        check("N8B/base-enemy-resource-includes-one-ranged-archetype",
              baseLoaded && baseRegistry.enemies().size() == 6 &&
                  baseRegistry.naturalEnemies().size() == 3 &&
                  baseRegistry.find("hellomine:spitter") != nullptr &&
                  baseRegistry.find("hellomine:spitter")
                          ->combat.mode == EnemyCombatMode::Ranged);
        bool dirtFreeIdentityLoot = baseLoaded;
        for (const EnemyDefinition &definition : baseRegistry.enemies()) {
            dirtFreeIdentityLoot = dirtFreeIdentityLoot &&
                !definition.loot.empty() && std::none_of(
                    definition.loot.begin(), definition.loot.end(),
                    [](const EnemyLootDefinition &loot) {
                        return loot.materialId == Material::ID::Dirt;
                    });
        }
        const EnemyDefinition *waystoneStalker =
            baseRegistry.find("hellomine:waystone_stalker");
        const EnemyDefinition *waystoneBrute =
            baseRegistry.find("hellomine:waystone_brute");
        check("P11E/base-enemies-have-distinct-dirt-free-loot",
              dirtFreeIdentityLoot && waystoneStalker != nullptr &&
                  waystoneBrute != nullptr &&
                  waystoneStalker->loot.front().materialId ==
                      Material::ID::IronOre &&
                  waystoneBrute->loot.front().materialId ==
                      Material::ID::IronIngot);
        check("N4/enemy-registry-is-startup-frozen",
              throwsContaining(
                  [&registry]
                  {
                      registry.freeze({{"again.enemy", validEnemies()}});
                  },
                  "already frozen"));
        check("N4/enemy-header-is-strict",
              throwsContaining(
                  []
                  {
                      EnemyRegistry invalid;
                      invalid.freeze({{"bad.enemy", "enemy stalker\n"}});
                  },
                  "unsupported or missing header"));
        check("N8B/legacy-v2-header-cannot-bypass-projectile-profile",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("registry v3"), 11,
                                     "registry v2");
                      EnemyRegistry invalid;
                      invalid.freeze({{"legacy.enemy", source}});
                  },
                  "unsupported or missing header"));
        check("N4/incomplete-enemy-is-rejected",
              throwsContaining(
                  []
                  {
                      EnemyRegistry invalid;
                      invalid.freeze({{"bad.enemy",
                          "# HelloMine3D enemy registry v3\n"
                          "enemy hellomine:stalker\nhealth 8\nend\n"}});
                  },
                  "incomplete or has no loot"));
        check("N4/duplicate-enemy-type-is-rejected",
              throwsContaining(
                  []
                  {
                      EnemyRegistry invalid;
                      invalid.freeze({{"a.enemy", validEnemies()},
                                      {"b.enemy", validEnemies()}});
                  },
                  "Duplicate enemy type"));
        check("N4/enemy-numeric-bounds-are-strict",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("health 8"), 8,
                                     "health 101");
                      EnemyRegistry invalid;
                      invalid.freeze({{"health.enemy", source}});
                  },
                  "health must be a finite number in") &&
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("wheat 1 2"), 9,
                                     "wheat 3 2");
                      EnemyRegistry invalid;
                      invalid.freeze({{"loot.enemy", source}});
                  },
                  "loot maximum must be an integer in"));
        check("N8B/combat-mode-and-timing-bounds-are-strict",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("combat_mode melee"), 17,
                                     "combat_mode magic");
                      EnemyRegistry invalid;
                      invalid.freeze({{"ranged.enemy", source}});
                  },
                  "combat_mode must be melee or ranged") &&
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("attack_windup_ticks 6"),
                                     21, "attack_windup_ticks 0");
                      EnemyRegistry invalid;
                      invalid.freeze({{"windup.enemy", source}});
                  },
                  "attack_windup_ticks must be an integer in"));
        check("N8B/ranged-projectile-fields-are-required-and-strict",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      const std::string field = "projectile_speed 10\n";
                      source.erase(source.find(field), field.size());
                      EnemyRegistry invalid;
                      invalid.freeze({{"missing-projectile.enemy", source}});
                  },
                  "incomplete or has no loot") &&
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("projectile_radius 0.15"),
                                     22, "projectile_radius 0.9");
                      EnemyRegistry invalid;
                      invalid.freeze({{"radius.enemy", source}});
                  },
                  "projectile_radius must be a finite number in"));
        check("N8B/projectile-capacity-and-active-range-are-consistent",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("projectile_local_limit 8"),
                                     24, "projectile_local_limit 16");
                      source.replace(source.find("projectile_world_limit 24"),
                                     25, "projectile_world_limit 8");
                      EnemyRegistry invalid;
                      invalid.freeze({{"capacity.enemy", source}});
                  },
                  "projectile_local_limit must not exceed") &&
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      source.replace(source.find("projectile_active_radius 32"),
                                     27, "projectile_active_radius 8");
                      EnemyRegistry invalid;
                      invalid.freeze({{"active.enemy", source}});
                  },
                  "attack_range must not exceed"));
        check("N8A/combat-cooldown-cannot-be-shorter-than-recovery",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      const std::size_t position =
                          source.find("attack_cooldown_ticks 16");
                      source.replace(position, 24,
                                     "attack_cooldown_ticks 6");
                      EnemyRegistry invalid;
                      invalid.freeze({{"cooldown.enemy", source}});
                  },
                  "must be greater than or equal"));
        check("N8A/combat-profile-fields-cannot-be-duplicated",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      const std::size_t end = source.find(
                          "natural 0\nloot hellomine:dirt");
                      source.insert(end, "attack_range 1\n");
                      EnemyRegistry invalid;
                      invalid.freeze({{"duplicate.enemy", source}});
                  },
                  "attack_range is duplicated"));
        check("N4/duplicate-enemy-loot-is-rejected",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      const std::string marker = "loot hellomine:wheat 1 2\n";
                      source.insert(source.find(marker) + marker.size(),
                                    "loot hellomine:wheat 1 1\n");
                      EnemyRegistry invalid;
                      invalid.freeze({{"loot.enemy", source}});
                  },
                  "loot material is duplicated"));
        check("N4/enemy-loot-entry-limit-is-enforced",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      const std::size_t stalker =
                          source.find("enemy hellomine:stalker");
                      const std::size_t end = source.find("end\n", stalker);
                      source.insert(end,
                                    "loot hellomine:stone 1 1\n"
                                    "loot hellomine:coal_ore 1 1\n"
                                    "loot hellomine:oak_bark 1 1\n");
                      EnemyRegistry invalid;
                      invalid.freeze({{"loot-limit.enemy", source}});
                  },
                  "loot table exceeds its entry limit"));
        check("N4/natural-enemy-is-required",
              throwsContaining(
                  []
                  {
                      std::string source = validEnemies();
                      std::size_t offset = 0;
                      while ((offset = source.find("natural 1", offset)) !=
                             std::string::npos) {
                          source.replace(offset, 9, "natural 0");
                          offset += 9;
                      }
                      EnemyRegistry invalid;
                      invalid.freeze({{"natural.enemy", source}});
                  },
                  "requires at least one natural enemy"));

        EnemyRegistry atomic;
        const bool failed = throwsContaining(
            [&atomic]
            {
                atomic.freeze({{"bad.enemy", "invalid\n"}});
            },
            "unsupported or missing header");
        atomic.freeze({{"valid.enemy", validEnemies()}});
        check("N4/failed-enemy-freeze-is-atomic",
              failed && atomic.isFrozen() && atomic.enemies().size() == 4);

        const fs::path root = fs::current_path() / "bin" /
                              "validation_runs" / "enemies";
        std::error_code error;
        fs::remove_all(root, error);
        const std::string logicalPath = "media/enemies/Base.enemy";
        writeFile(root / logicalPath, validEnemies());
        ResourcePackResolver resolver;
        resolver.freeze(root.string(), {{"enemy", logicalPath}}, {});
        EnemyRegistry fromView;
        fromView.freezeFromResourceView(resolver);
        check("N4/frozen-enemy-resource-loads",
              fromView.isFrozen() && fromView.enemies().size() == 4 &&
                  resolver.effectiveManifest().find(
                      "enemy|media/enemies/Base.enemy|base\n") !=
                      std::string::npos);
    }

    void caseCombatRecipes()
    {
        std::ifstream baseRecipes(
            ResourcePaths::media("recipes/Base.recipe"),
            std::ios::binary);
        std::ostringstream content;
        content << baseRecipes.rdbuf();
        RecipeRegistry recipes;
        recipes.freeze({{"Base.recipe", content.str()}});
        const RecipeDefinition *wood =
            recipes.find("hellomine:wooden_sword");
        const RecipeDefinition *stone =
            recipes.find("hellomine:stone_sword");
        check("N4/wooden-sword-recipe-is-progression-bounded",
              wood != nullptr &&
                  wood->outputMaterialId == Material::ID::WoodenSword &&
                  wood->outputCount == 1 && wood->ingredients.size() == 1 &&
                  wood->ingredients[0].materialId == Material::ID::OakBark &&
                  wood->ingredients[0].count == 3);
        check("N4/stone-sword-recipe-uses-cobblestone-and-handle",
              stone != nullptr &&
                  stone->outputMaterialId == Material::ID::StoneSword &&
                  stone->outputCount == 1 && stone->ingredients.size() == 2 &&
                  std::any_of(
                      stone->ingredients.begin(), stone->ingredients.end(),
                      [](const RecipeIngredient &ingredient) {
                          return ingredient.materialId ==
                                     Material::ID::Cobblestone &&
                                 ingredient.count == 2;
                      }));
    }

    void caseResourceEconomy()
    {
        const auto readResource = [](const std::string &path) {
            std::ifstream input(ResourcePaths::media(path),
                                std::ios::binary);
            std::ostringstream content;
            content << input.rdbuf();
            return content.str();
        };

        RecipeRegistry recipes;
        recipes.freeze({{"Base.recipe",
                         readResource("recipes/Base.recipe")}});
        SmeltingRegistry smelting;
        smelting.freeze({{"Base.smelting",
                          readResource("smelting/Base.smelting")}});
        FoodRegistry foods;
        foods.freeze({{"Base.food", readResource("foods/Base.food")}});

        const SmeltingRecipeDefinition *meat =
            smelting.findRecipe(Material::ID::RawMeat);
        const SmeltingRecipeDefinition *glass =
            smelting.findRecipe(Material::ID::Sand);
        const SmeltingRecipeDefinition *stone =
            smelting.findRecipe(Material::ID::Cobblestone);
        const SmeltingFuelDefinition *fiberFuel =
            smelting.findFuel(Material::ID::PlantFiber);
        check("N10/base-content-counts-are-frozen",
              recipes.recipes().size() == 24 &&
                  smelting.recipes().size() == 4 &&
                  smelting.fuels().size() == 2 &&
                  foods.foods().size() == 4);
        check("N10/new-smelting-paths-reuse-three-slot-contract",
              meat != nullptr &&
                  meat->outputMaterialId == Material::ID::CookedMeat &&
                  meat->durationTicks == 60 && glass != nullptr &&
                  glass->outputMaterialId == Material::ID::Glass &&
                  glass->durationTicks == 80 && fiberFuel != nullptr &&
                  fiberFuel->burnTicks == 40 && stone != nullptr &&
                  stone->outputMaterialId == Material::ID::Stone &&
                  stone->durationTicks == 80);

        const RecipeDefinition *fiber =
            recipes.find("hellomine:plant_fiber_from_tall_grass");
        const RecipeDefinition *salad =
            recipes.find("hellomine:cactus_salad");
        const RecipeDefinition *ration =
            recipes.find("hellomine:trail_ration");
        const RecipeDefinition *fieldChest =
            recipes.find("hellomine:field_chest");
        const RecipeDefinition *fieldWorkbench =
            recipes.find("hellomine:field_workbench");
        const RecipeDefinition *reinforcedFurnace =
            recipes.find("hellomine:reinforced_furnace");
        const RecipeDefinition *torch =
            recipes.find("hellomine:torch");
        const RecipeDefinition *planks =
            recipes.find("hellomine:oak_planks");
        const RecipeDefinition *door =
            recipes.find("hellomine:oak_door");
        const RecipeDefinition *axe =
            recipes.find("hellomine:wooden_axe");
        const RecipeDefinition *shovel =
            recipes.find("hellomine:wooden_shovel");
        check("N10/ecology-crop-and-enemy-chains-have-useful-sinks",
              fiber != nullptr && salad != nullptr && ration != nullptr &&
                  fieldChest != nullptr && fieldWorkbench != nullptr &&
                  reinforcedFurnace != nullptr &&
                  ration->ingredients.size() == 4 &&
                  ration->outputMaterialId == Material::ID::TrailRation);
        check("P11-0/torch-recipe-is-bounded-2x2-progression",
              torch != nullptr && torch->type == RecipeType::Shaped &&
                  torch->width == 1 && torch->height == 2 &&
                  torch->outputMaterialId == Material::ID::Torch &&
                  torch->outputCount == 4 &&
                  torch->ingredients.size() == 2 &&
                  std::any_of(
                      torch->ingredients.begin(), torch->ingredients.end(),
                      [](const RecipeIngredient &ingredient) {
                          return ingredient.materialId ==
                                     Material::ID::CoalOre &&
                                 ingredient.count == 1;
                      }) &&
                  std::any_of(
                      torch->ingredients.begin(), torch->ingredients.end(),
                      [](const RecipeIngredient &ingredient) {
                          return ingredient.materialId ==
                                     Material::ID::OakBark &&
                                 ingredient.count == 1;
                      }));
        check("P11-1/minimum-building-and-tool-recipes-are-bounded",
              planks != nullptr && planks->type == RecipeType::Shapeless &&
                  planks->outputMaterialId == Material::ID::OakPlank &&
                  planks->outputCount == 4 && door != nullptr &&
                  door->width == 2 && door->height == 3 &&
                  door->outputMaterialId == Material::ID::OakDoor &&
                  axe != nullptr && axe->outputMaterialId ==
                      Material::ID::WoodenAxe && shovel != nullptr &&
                  shovel->outputMaterialId == Material::ID::WoodenShovel);

        const RecipeDefinition *stonePickaxe =
            recipes.find("hellomine:stone_pickaxe");
        check("P11-1/cobblestone-loop-feeds-progression-without-cycle",
              stone != nullptr && stonePickaxe != nullptr &&
                  std::any_of(
                      stonePickaxe->ingredients.begin(),
                      stonePickaxe->ingredients.end(),
                      [](const RecipeIngredient &ingredient) {
                          return ingredient.materialId ==
                                     Material::ID::Cobblestone &&
                                 ingredient.count == 3;
                      }));

        Inventory torchInputs;
        torchInputs.addItem(Material::COAL_ORE_BLOCK, 25);
        torchInputs.addItem(Material::OAK_BARK_BLOCK, 25);
        CraftingSession playerGrid(CraftingSession::PlayerGridSize);
        const bool torchLoaded = torch != nullptr &&
            playerGrid.loadRecipe(*torch);
        const CraftingPreview torchPreview =
            playerGrid.preview(recipes, torchInputs);
        const CraftingCommitResult torchCommitted =
            playerGrid.commit(recipes, torchInputs, torchPreview, 25);
        check("P11-0/repeated-torch-craft-is-atomic-and-conservative",
              torchLoaded && torchPreview.ready() &&
                  torchCommitted.succeeded() &&
                  torchInputs.count(Material::ID::CoalOre) == 0 &&
                  torchInputs.count(Material::ID::OakBark) == 0 &&
                  torchInputs.count(Material::ID::Torch) == 100);

        Inventory rationInputs;
        rationInputs.addItem(Material::BREAD, 1);
        rationInputs.addItem(Material::COOKED_MEAT, 1);
        rationInputs.addItem(Material::CACTUS_SALAD, 1);
        rationInputs.addItem(Material::PLANT_FIBER, 1);
        CraftingSession workbench(CraftingSession::WorkbenchGridSize);
        const bool loaded = workbench.loadRecipe(*ration);
        const CraftingPreview preview =
            workbench.preview(recipes, rationInputs);
        const CraftingCommitResult committed =
            workbench.commit(recipes, rationInputs, preview, 1);
        check("N10/trail-ration-craft-is-atomic-and-conservative",
              loaded && preview.ready() && committed.succeeded() &&
                  rationInputs.count(Material::ID::Bread) == 0 &&
                  rationInputs.count(Material::ID::CookedMeat) == 0 &&
                  rationInputs.count(Material::ID::CactusSalad) == 0 &&
                  rationInputs.count(Material::ID::PlantFiber) == 0 &&
                  rationInputs.count(Material::ID::TrailRation) == 1);

        const ResourceEconomyContract contract =
            makeBaseResourceEconomyContract();
        const ResourceEconomyReport report =
            ResourceEconomyVerifier::verify(
                contract, recipes, smelting, foods);
        check("N10/clean-world-mainline-is-reachable",
              report.allRequiredReachable && report.issues.empty());
        check("N10/new-materials-have-source-and-sink",
              report.trackedSourcesAndSinks);
        check("N10/transformation-graph-has-no-cycle-or-duplication-path",
              report.transformationGraphAcyclic && report.passed());

        bool trackedMetricsComplete = true;
        int trackedMetricCount = 0;
        int ironGoalAmount = 0;
        for (const ResourceEconomyMetric &metric : report.metrics) {
            if (std::find(contract.trackedNewMaterials.begin(),
                          contract.trackedNewMaterials.end(),
                          metric.materialId) !=
                contract.trackedNewMaterials.end()) {
                ++trackedMetricCount;
                trackedMetricsComplete = trackedMetricsComplete &&
                    std::isfinite(metric.acquisitionTicksPerUnit) &&
                    metric.acquisitionTicksPerUnit > 0.0 &&
                    metric.maxStackSize == 99;
            }
            if (metric.materialId == Material::ID::IronIngot) {
                ironGoalAmount = metric.goalAmount;
            }
        }
        check("N10/balance-metrics-cover-cost-recovery-capacity-and-goals",
              trackedMetricCount == 5 && trackedMetricsComplete &&
                  ironGoalAmount == 7);

        const std::string csv = report.toCsv();
        const fs::path outputDirectory = fs::current_path() / "bin" /
            "validation_runs" / "resource_economy";
        std::error_code error;
        fs::create_directories(outputDirectory, error);
        const fs::path csvPath = outputDirectory / "resource-economy.csv";
        writeFile(csvPath, csv);
        check("N10/balance-report-is-exportable-and-bounded",
              !error && csv.size() < 32 * 1024 &&
                  csv.find("recovery_per_acquisition_tick") !=
                      std::string::npos &&
                  csv.find("hellomine:trail_ration") !=
                      std::string::npos && fs::file_size(csvPath) ==
                      csv.size());

        ResourceEconomyContract missingSource = contract;
        missingSource.acquisitionSources.erase(
            std::remove_if(
                missingSource.acquisitionSources.begin(),
                missingSource.acquisitionSources.end(),
                [](const ResourceAcquisitionSource &source) {
                    return source.materialId == Material::ID::RawMeat;
                }),
            missingSource.acquisitionSources.end());
        const ResourceEconomyReport unreachable =
            ResourceEconomyVerifier::verify(
                missingSource, recipes, smelting, foods);
        check("N10/missing-source-dead-chain-is-rejected",
              !unreachable.allRequiredReachable &&
                  !unreachable.trackedSourcesAndSinks &&
                  !unreachable.passed());

        ResourceEconomyContract missingSink = contract;
        missingSink.trackedNewMaterials = {Material::ID::OakLeaf};
        const ResourceEconomyReport noSink =
            ResourceEconomyVerifier::verify(
                missingSink, recipes, smelting, foods);
        check("N10/tracked-resource-without-sink-is-rejected",
              !noSink.trackedSourcesAndSinks && !noSink.passed());

        RecipeRegistry cyclicRecipes;
        cyclicRecipes.freeze({{"cycle.recipe", oneRecipe(
            "recipe hellomine:dirt_to_stone shapeless\n"
            "input hellomine:dirt 1\n"
            "output hellomine:stone 2\nend\n"
            "recipe hellomine:stone_to_dirt shapeless\n"
            "input hellomine:stone 1\n"
            "output hellomine:dirt 1\nend\n")}});
        ResourceEconomyContract cycleContract = contract;
        cycleContract.requiredMaterials.clear();
        cycleContract.trackedNewMaterials.clear();
        cycleContract.goalRequirements.clear();
        const ResourceEconomyReport cycle =
            ResourceEconomyVerifier::verify(
                cycleContract, cyclicRecipes, smelting, foods);
        check("N10/net-positive-material-cycle-is-rejected",
              !cycle.transformationGraphAcyclic && !cycle.passed());
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
    caseFoodRegistry();
    caseEnemyRegistry();
    caseCombatRecipes();
    caseResourceEconomy();
    constexpr int ExpectedChecks = 122;
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
