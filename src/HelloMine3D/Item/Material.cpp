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

Material::Material(Material::ID id, int maxStack, bool isBlock,
                   std::string &&name)
    : id(id)
    , maxStackSize(maxStack)
    , isBlock(isBlock)
    , name(std::move(name))
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
