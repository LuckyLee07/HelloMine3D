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
        "hellomine:raw_meat",
        "hellomine:cooked_meat",
        "hellomine:cactus_salad",
        "hellomine:trail_ration",
        "hellomine:plant_fiber",
        "hellomine:torch",
        "hellomine:oak_planks",
        "hellomine:cobblestone",
        "hellomine:oak_door",
        "hellomine:wooden_axe",
        "hellomine:wooden_shovel",
        "hellomine:ancient_compass",
        "hellomine:raider_ward",
        "hellomine:crusher",
    }};

    constexpr std::array<Material::IconCoordinate, Material::ID::Count>
        MaterialIcons = {{
            {-1, -1}, {0, 0}, {2, 0}, {3, 0}, {4, 0}, {6, 0},
            {7, 0}, {9, 0}, {10, 0}, {11, 0}, {12, 0}, {13, 0},
            {14, 0}, {3, 1}, {4, 1}, {0, 1}, {0, 2}, {1, 2},
            {1, 1}, {2, 2}, {3, 2}, {2, 1}, {4, 2}, {5, 2},
            {6, 2}, {7, 2}, {8, 2}, {9, 2}, {15, 0}, {10, 2},
            {11, 2}, {12, 2}, {13, 2}, {14, 2}, {6, 1}, {5, 1},
            {7, 1}, {8, 1}, {15, 2}, {15, 3}, {9, 1}, {10, 1},
            {11, 1},
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
const Material Material::RAW_MEAT(ID::RawMeat, 99, false, "Raw Meat");
const Material Material::COOKED_MEAT(ID::CookedMeat, 99, false,
                                     "Cooked Meat", false, true);
const Material Material::CACTUS_SALAD(ID::CactusSalad, 99, false,
                                      "Cactus Salad", false, true);
const Material Material::TRAIL_RATION(ID::TrailRation, 99, false,
                                      "Trail Ration", false, true);
const Material Material::PLANT_FIBER(ID::PlantFiber, 99, false,
                                     "Plant Fiber");
const Material Material::TORCH(ID::Torch, 99, true, "Torch");
const Material Material::OAK_PLANK_BLOCK(ID::OakPlank, 99, true,
                                         "Oak Planks");
const Material Material::COBBLESTONE_BLOCK(ID::Cobblestone, 99, true,
                                           "Cobblestone");
const Material Material::OAK_DOOR(ID::OakDoor, 99, true, "Oak Door");
const Material Material::WOODEN_AXE(ID::WoodenAxe, 1, false,
                                    "Wooden Axe", true);
const Material Material::WOODEN_SHOVEL(ID::WoodenShovel, 1, false,
                                       "Wooden Shovel", true);
const Material Material::ANCIENT_COMPASS(ID::AncientCompass, 1, false,
                                         "Ancient Compass");
const Material Material::RAIDER_WARD(ID::RaiderWard, 1, false,
                                     "Raider Ward");
const Material Material::CRUSHER_BLOCK(ID::Crusher, 99, true,
                                       "Hand-Cranked Crusher");

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

        case Torch:
            return BlockId::Torch;

        case OakPlank:
            return BlockId::OakPlank;

        case Cobblestone:
            return BlockId::Cobblestone;

        case OakDoor:
            return BlockId::OakDoorClosed;

        case Crusher:
            return BlockId::Crusher;

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

        case BlockId::Torch:
            return TORCH;

        case BlockId::OakPlank:
            return OAK_PLANK_BLOCK;

        case BlockId::Cobblestone:
            return COBBLESTONE_BLOCK;

        case BlockId::OakDoorClosed:
        case BlockId::OakDoorOpen:
            return OAK_DOOR;

        case BlockId::Crusher:
            return CRUSHER_BLOCK;

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

        case RawMeat:
            return RAW_MEAT;

        case CookedMeat:
            return COOKED_MEAT;

        case CactusSalad:
            return CACTUS_SALAD;

        case TrailRation:
            return TRAIL_RATION;

        case PlantFiber:
            return PLANT_FIBER;

        case Torch:
            return TORCH;

        case OakPlank:
            return OAK_PLANK_BLOCK;

        case Cobblestone:
            return COBBLESTONE_BLOCK;

        case OakDoor:
            return OAK_DOOR;

        case WoodenAxe:
            return WOODEN_AXE;

        case WoodenShovel:
            return WOODEN_SHOVEL;

        case AncientCompass:
            return ANCIENT_COMPASS;

        case RaiderWard:
            return RAIDER_WARD;

        case Crusher:
            return CRUSHER_BLOCK;

        default:
            return NOTHING;
    }
}

Material::IconCoordinate Material::iconCoordinate(Material::ID id) noexcept
{
    const int index = static_cast<int>(id);
    if (index < 0 || index >= static_cast<int>(MaterialIcons.size())) {
        return {};
    }
    return MaterialIcons[static_cast<std::size_t>(index)];
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
