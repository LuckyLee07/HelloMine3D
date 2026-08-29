#ifndef MATERIAL_H_INCLUDED
#define MATERIAL_H_INCLUDED

#include "../World/Block/BlockId.h"
#include <functional>
#include <string>
#include <type_traits>

#include "../Util/NonCopyable.h"

/// @brief Determines case-by-case properties and behaviors of known block types.
struct Material : public NonCopyable {
    struct IconCoordinate {
        int x = -1;
        int y = -1;

        bool available() const noexcept
        {
            return x >= 0 && x < 16 && y >= 0 && y < 16;
        }
    };

    enum ID {
        Nothing,
        Grass,
        Dirt,
        Stone,
        OakBark,
        OakLeaf,
        Sand,
        Cactus,
        Rose,
        TallGrass,
        DeadShrub,
        CoalOre,
        IronOre,
        Glass,
        GlassBorderless,
        Chest,
        WheatSeeds,
        Wheat,
        Workbench,
        WoodenPickaxe,
        StonePickaxe,
        Furnace,
        IronIngot,
        IronPickaxe,
        IronSword,
        Bread,
        WoodenSword,
        StoneSword,
        WaystoneCore,
        RawMeat,
        CookedMeat,
        CactusSalad,
        TrailRation,
        PlantFiber,
        Torch,
        Count
    };

    const static Material NOTHING, GRASS_BLOCK, DIRT_BLOCK, STONE_BLOCK,
        OAK_BARK_BLOCK, OAK_LEAF_BLOCK, SAND_BLOCK, CACTUS_BLOCK, ROSE,
        TALL_GRASS, DEAD_SHRUB, COAL_ORE_BLOCK, IRON_ORE_BLOCK, GLASS_BLOCK,
        BORDERLESS_GLASS_BLOCK, CHEST_BLOCK, WHEAT_SEEDS, WHEAT,
        WORKBENCH_BLOCK, WOODEN_PICKAXE, STONE_PICKAXE, FURNACE_BLOCK,
        IRON_INGOT, IRON_PICKAXE, IRON_SWORD, BREAD, WOODEN_SWORD,
        STONE_SWORD, WAYSTONE_CORE, RAW_MEAT, COOKED_MEAT,
        CACTUS_SALAD, TRAIL_RATION, PLANT_FIBER, TORCH;

    Material(Material::ID id, int maxStack, bool isBlock, std::string &&name,
             bool isTool = false, bool isFood = false);

    BlockId toBlockID() const;

    static const Material &toMaterial(BlockId id);
    static const Material &toMaterial(Material::ID id);
    static IconCoordinate iconCoordinate(Material::ID id) noexcept;
    static const char *toStringId(Material::ID id) noexcept;
    static bool tryParseStringId(const std::string &value,
                                 Material::ID &id) noexcept;

    const Material::ID id;
    const int maxStackSize;
    const bool isBlock;
    const std::string name;
    const bool isTool;
    const bool isFood;
};

namespace std {
template <> struct hash<Material::ID> {
    size_t operator()(const Material::ID &id) const
    {
        using Underlying = std::underlying_type_t<Material::ID>;
        return std::hash<Underlying>{}(static_cast<Underlying>(id));
    }
};
} // namespace std

#endif // MATERIAL_H_INCLUDED
