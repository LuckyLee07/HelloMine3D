#include "Material.h"

#include <array>

namespace
{
    constexpr std::array<const char *, Material::ID::Count> MaterialIds = {{
        "hellomine:nothing",
        "hellomine:grass",
        "hellomine:dirt",
        "hellomine:stone",
        "hellomine:oak_bark",
        "hellomine:oak_leaf",
        "hellomine:sand",
        "hellomine:cactus",
        "hellomine:rose",
        "hellomine:tall_grass",
        "hellomine:dead_shrub",
        "hellomine:coal_ore",
        "hellomine:iron_ore",
        "hellomine:glass",
        "hellomine:glass_borderless",
        "hellomine:chest",
        "hellomine:wheat_seeds",
        "hellomine:wheat",
        "hellomine:workbench",
        "hellomine:wooden_pickaxe",
        "hellomine:stone_pickaxe",
        "hellomine:furnace",
        "hellomine:iron_ingot",
        "hellomine:iron_pickaxe",
        "hellomine:iron_sword",
        "hellomine:bread",
        "hellomine:wooden_sword",
        "hellomine:stone_sword",
        "hellomine:waystone_core",
    }};
}

const Material Material::NOTHING(ID::Nothing, 0, false, "None");
const Material Material::GRASS_BLOCK(ID::Grass, 99, true, "Grass Block");
const Material Material::DIRT_BLOCK(ID::Dirt, 99, true, "Dirt Block");
const Material Material::STONE_BLOCK(ID::Stone, 99, true, "Stone Block");
const Material Material::OAK_BARK_BLOCK(ID::OakBark, 99, true,
                                        "Oak Bark Block");
const Material Material::OAK_LEAF_BLOCK(ID::OakLeaf, 99, true,
                                        "Oak Leaf Block");
const Material Material::SAND_BLOCK(ID::Sand, 99, true, "Sand Block");
const Material Material::CACTUS_BLOCK(ID::Cactus, 99, true, "Cactus Block");

const Material Material::ROSE(ID::Rose, 99, true, "Rose");
const Material Material::TALL_GRASS(ID::TallGrass, 99, true, "Tall Grass");
const Material Material::DEAD_SHRUB(ID::DeadShrub, 99, true, "Dead Shrub");
const Material Material::COAL_ORE_BLOCK(ID::CoalOre, 99, true,
                                        "Coal Ore Block");
const Material Material::IRON_ORE_BLOCK(ID::IronOre, 99, true,
                                        "Iron Ore Block");
const Material Material::GLASS_BLOCK(ID::Glass, 99, true, "Glass Block");
const Material Material::BORDERLESS_GLASS_BLOCK(
    ID::GlassBorderless, 99, true, "Borderless Glass Block");
const Material Material::CHEST_BLOCK(ID::Chest, 99, true, "Chest");
const Material Material::WHEAT_SEEDS(ID::WheatSeeds, 99, true,
                                     "Wheat Seeds");
const Material Material::WHEAT(ID::Wheat, 99, false, "Wheat");
const Material Material::WORKBENCH_BLOCK(ID::Workbench, 99, true,
                                         "Workbench");
const Material Material::WOODEN_PICKAXE(ID::WoodenPickaxe, 1, false,
                                        "Wooden Pickaxe", true);
const Material Material::STONE_PICKAXE(ID::StonePickaxe, 1, false,
                                       "Stone Pickaxe", true);
const Material Material::FURNACE_BLOCK(ID::Furnace, 99, true, "Furnace");
const Material Material::IRON_INGOT(ID::IronIngot, 99, false, "Iron Ingot");
const Material Material::IRON_PICKAXE(ID::IronPickaxe, 1, false,
                                      "Iron Pickaxe", true);
const Material Material::IRON_SWORD(ID::IronSword, 1, false,
                                    "Iron Sword", true);
const Material Material::BREAD(ID::Bread, 99, false, "Bread", false, true);
const Material Material::WOODEN_SWORD(ID::WoodenSword, 1, false,
                                      "Wooden Sword", true);
const Material Material::STONE_SWORD(ID::StoneSword, 1, false,
                                     "Stone Sword", true);
const Material Material::WAYSTONE_CORE(ID::WaystoneCore, 16, true,
                                       "Waystone Core");

Material::Material(Material::ID id, int maxStack, bool isBlock,
                   std::string &&name, bool isTool, bool isFood)
    : id(id)
    , maxStackSize(maxStack)
    , isBlock(isBlock)
    , name(std::move(name))
    , isTool(isTool)
    , isFood(isFood)
{
}

BlockId Material::toBlockID() const
{
    switch (id) {
        case Nothing:
            return BlockId::Air;

        case Grass:
            return BlockId::Grass;

        case Dirt:
            return BlockId::Dirt;

        case Stone:
            return BlockId::Stone;

        case OakBark:
            return BlockId::OakBark;

        case OakLeaf:
            return BlockId::OakLeaf;

        case Sand:
            return BlockId::Sand;

        case Cactus:
            return BlockId::Cactus;

        case TallGrass:
            return BlockId::TallGrass;

        case Rose:
            return BlockId::Rose;

        case DeadShrub:
            return BlockId::DeadShrub;

        case CoalOre:
            return BlockId::CoalOre;

        case IronOre:
            return BlockId::IronOre;

        case Glass:
            return BlockId::Glass;

        case GlassBorderless:
            return BlockId::GlassBorderless;

        case Chest:
            return BlockId::Chest;

        case WheatSeeds:
            return BlockId::WheatCrop;

        case Workbench:
            return BlockId::Workbench;

        case Furnace:
            return BlockId::Furnace;

        case WaystoneCore:
            return BlockId::WaystoneCore;

        default:
            return BlockId::NUM_TYPES;
    }
}

const Material &Material::toMaterial(BlockId id)
{
    switch (id) {
        case BlockId::Grass:
            return GRASS_BLOCK;

        case BlockId::Dirt:
            return DIRT_BLOCK;

        case BlockId::Stone:
            return STONE_BLOCK;

        case BlockId::OakBark:
            return OAK_BARK_BLOCK;

        case BlockId::OakLeaf:
            return OAK_LEAF_BLOCK;

        case BlockId::Sand:
            return SAND_BLOCK;

        case BlockId::Cactus:
            return CACTUS_BLOCK;

        case BlockId::Rose:
            return ROSE;

        case BlockId::TallGrass:
            return TALL_GRASS;

        case BlockId::DeadShrub:
            return DEAD_SHRUB;

        case BlockId::CoalOre:
            return COAL_ORE_BLOCK;

        case BlockId::IronOre:
            return IRON_ORE_BLOCK;

        case BlockId::Glass:
            return GLASS_BLOCK;

        case BlockId::GlassBorderless:
            return BORDERLESS_GLASS_BLOCK;

        case BlockId::Chest:
            return CHEST_BLOCK;

        case BlockId::WheatCrop:
            return WHEAT_SEEDS;

        case BlockId::Workbench:
            return WORKBENCH_BLOCK;

        case BlockId::Furnace:
            return FURNACE_BLOCK;

        case BlockId::WaystoneCore:
            return WAYSTONE_CORE;

        default:
            return NOTHING;
    }
}

const Material &Material::toMaterial(Material::ID id)
{
    switch (id) {
        case Grass:
            return GRASS_BLOCK;

        case Dirt:
            return DIRT_BLOCK;

        case Stone:
            return STONE_BLOCK;

        case OakBark:
            return OAK_BARK_BLOCK;

        case OakLeaf:
            return OAK_LEAF_BLOCK;

        case Sand:
            return SAND_BLOCK;

        case Cactus:
            return CACTUS_BLOCK;

        case Rose:
            return ROSE;

        case TallGrass:
            return TALL_GRASS;

        case DeadShrub:
            return DEAD_SHRUB;

        case CoalOre:
            return COAL_ORE_BLOCK;

        case IronOre:
            return IRON_ORE_BLOCK;

        case Glass:
            return GLASS_BLOCK;

        case GlassBorderless:
            return BORDERLESS_GLASS_BLOCK;

        case Chest:
            return CHEST_BLOCK;

        case WheatSeeds:
            return WHEAT_SEEDS;

        case Wheat:
            return WHEAT;

        case Workbench:
            return WORKBENCH_BLOCK;

        case WoodenPickaxe:
            return WOODEN_PICKAXE;

        case StonePickaxe:
            return STONE_PICKAXE;

        case Furnace:
            return FURNACE_BLOCK;

        case IronIngot:
            return IRON_INGOT;

        case IronPickaxe:
            return IRON_PICKAXE;

        case IronSword:
            return IRON_SWORD;

        case Bread:
            return BREAD;

        case WoodenSword:
            return WOODEN_SWORD;

        case StoneSword:
            return STONE_SWORD;

        case WaystoneCore:
            return WAYSTONE_CORE;

        default:
            return NOTHING;
    }
}

const char *Material::toStringId(Material::ID id) noexcept
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= static_cast<int>(MaterialIds.size())) {
        return MaterialIds[Material::ID::Nothing];
    }
    return MaterialIds[static_cast<std::size_t>(index)];
}

bool Material::tryParseStringId(const std::string &value,
                                Material::ID &id) noexcept
{
    for (std::size_t index = 0; index < MaterialIds.size(); ++index) {
        if (value == MaterialIds[index]) {
            id = static_cast<Material::ID>(index);
            return true;
        }
    }
    return false;
}
