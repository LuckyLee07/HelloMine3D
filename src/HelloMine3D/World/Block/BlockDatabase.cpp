#include "BlockDatabase.h"
#include "ChestContainer.h"
#include "ChunkBlock.h"
#include "../../Player/Player.h"
#include "../../Sandbox/Events/PlayerEvents.h"
#include "../World.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void giveBehaviorDrop(World &world, Player &player,
                      const glm::ivec3 &position,
                      Material::ID materialId, const char *reason)
{
    if (player.addItem(Material::toMaterial(materialId))) {
        world.getEventBus().publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId, materialId, 1, reason));
        return;
    }

    world.spawnItemEntity(
        materialId, 1,
        glm::vec3(position) + glm::vec3(0.5f),
        glm::vec3(0.f, 2.5f, 0.f));
}

class NoDropBlockBehavior final : public BlockBehavior {
  public:
    Material::ID getDrop(const BlockDefinition &,
                         const ChunkBlock &) const override
    {
        return Material::ID::Nothing;
    }
};

class TallGrassBlockBehavior final : public BlockBehavior {
  public:
    explicit TallGrassBlockBehavior(BlockMetadata_t matureStage)
        : m_matureStage(matureStage)
    {
    }

    Material::ID getDrop(const BlockDefinition &definition,
                         const ChunkBlock &block) const override
    {
        return block.metadata >= m_matureStage
                   ? BlockBehavior::getDrop(definition, block)
                   : Material::ID::Nothing;
    }

    bool receivesRandomTicks(const BlockDefinition &,
                             const ChunkBlock &block) const noexcept override
    {
        return block.metadata < m_matureStage;
    }

    void onRandomTick(World &world, const glm::ivec3 &position,
                      const ChunkBlock &block) const override
    {
        if (block.metadata < m_matureStage) {
            world.setBlock(position.x, position.y, position.z,
                           ChunkBlock(block.id, m_matureStage));
        }
    }

    void onBroken(World &world, Player &player,
                  const glm::ivec3 &position,
                  const ChunkBlock &block) const override
    {
        if (block.metadata >= m_matureStage) {
            giveBehaviorDrop(world, player, position,
                             Material::ID::WheatSeeds,
                             "tall_grass_seed");
        }
    }

  private:
    BlockMetadata_t m_matureStage;
};

class WheatCropBlockBehavior final : public BlockBehavior {
  public:
    explicit WheatCropBlockBehavior(BlockMetadata_t matureStage)
        : m_matureStage(matureStage)
    {
    }

    Material::ID getDrop(const BlockDefinition &,
                         const ChunkBlock &block) const override
    {
        return block.metadata >= m_matureStage
                   ? Material::ID::Wheat
                   : Material::ID::WheatSeeds;
    }

    bool receivesRandomTicks(const BlockDefinition &,
                             const ChunkBlock &block) const noexcept override
    {
        return block.metadata < m_matureStage;
    }

    void onRandomTick(World &world, const glm::ivec3 &position,
                      const ChunkBlock &block) const override
    {
        if (block.metadata >= m_matureStage) {
            return;
        }
        world.setBlock(position.x, position.y, position.z,
                       ChunkBlock(block.id, static_cast<BlockMetadata_t>(
                                                block.metadata + 1)));
    }

    bool canPlace(World &world, const Player &,
                  const glm::ivec3 &position,
                  const ChunkBlock &previousBlock) const override
    {
        if (previousBlock != BlockId::Air) {
            return false;
        }
        const auto support = static_cast<BlockId>(
            world.getBlock(position.x, position.y - 1, position.z).id);
        return support == BlockId::Grass || support == BlockId::Dirt;
    }

    float verticalRenderScale(
        const BlockDefinition &,
        const ChunkBlock &block) const noexcept override
    {
        const float stage = static_cast<float>(
            std::min(block.metadata, m_matureStage) + 1);
        return stage / static_cast<float>(m_matureStage + 1);
    }

    void onBroken(World &world, Player &player,
                  const glm::ivec3 &position,
                  const ChunkBlock &block) const override
    {
        if (block.metadata >= m_matureStage) {
            giveBehaviorDrop(world, player, position,
                             Material::ID::WheatSeeds,
                             "crop_harvest_seed");
        }
    }

  private:
    BlockMetadata_t m_matureStage;
};

class ChestBlockBehavior final : public BlockBehavior {
  public:
    void onPlaced(World &world, Player &,
                  const glm::ivec3 &position, const ChunkBlock &,
                  const ChunkBlock &) const override
    {
        ChestContainer::initialize(world, position);
    }

    void onBroken(World &world, Player &player,
                  const glm::ivec3 &position,
                  const ChunkBlock &) const override
    {
        ChestContainer::spillContents(world, position);
        if (player.getOpenContainer() &&
            player.getOpenContainer()->x == position.x &&
            player.getOpenContainer()->y == position.y &&
            player.getOpenContainer()->z == position.z) {
            ChestContainer::close(player);
        }
    }

    void onUse(World &world, Player &player,
               const glm::ivec3 &position,
               const ChunkBlock &) const override
    {
        ChestContainer::open(world, player, position);
    }
};

class WorkbenchBlockBehavior final : public BlockBehavior {
  public:
    void onBroken(World &, Player &player,
                  const glm::ivec3 &position,
                  const ChunkBlock &) const override
    {
        if (player.getOpenWorkbench() &&
            player.getOpenWorkbench()->x == position.x &&
            player.getOpenWorkbench()->y == position.y &&
            player.getOpenWorkbench()->z == position.z) {
            player.closeCrafting();
        }
    }

    void onUse(World &, Player &player,
               const glm::ivec3 &position,
               const ChunkBlock &) const override
    {
        player.openCrafting(3, position);
    }
};

std::string makeStringId(const std::string &fileName)
{
    std::string id = "hellomine:";
    for (char value : fileName) {
        id += static_cast<char>(
            std::tolower(static_cast<unsigned char>(value)));
    }
    return id;
}

BlockDefinition makeDefinition(const std::string &fileName,
                               const BlockType &blockType)
{
    const auto &data = blockType.getData().getBlockData();

    BlockDefinition definition;
    definition.id = data.id;
    definition.stringId = makeStringId(fileName);
    definition.name = data.name.empty() ? fileName : data.name;
    definition.hardness = data.id == BlockId::Air ? 0.f : 1.f;
    definition.collidable = data.isCollidable;
    definition.transparent = !data.isOpaque;
    definition.liquid = data.id == BlockId::Water ||
                        data.shaderType == BlockShaderType::Liquid;
    definition.solid = data.isOpaque && data.isCollidable;
    definition.light = data.light;
    definition.defaultDrop = Material::toMaterial(data.id).id;
    definition.render.texTopCoord = data.texTopCoord;
    definition.render.texSideCoord = data.texSideCoord;
    definition.render.texBottomCoord = data.texBottomCoord;
    definition.render.meshType = data.meshType;
    definition.render.shaderType = data.shaderType;
    definition.render.shape = data.shape;
    return definition;
}
} // namespace

void BlockIdUniquenessValidator::add(BlockId id,
                                     const std::string &sourcePath)
{
    const auto index = static_cast<unsigned>(id);
    if (index >= m_sources.size()) {
        throw std::runtime_error("Invalid block file '" + sourcePath +
                                 "': key 'Id' is outside the registered "
                                 "block id range.");
    }

    if (!m_sources[index].empty()) {
        throw std::runtime_error(
            "Invalid block file '" + sourcePath +
            "': key 'Id' duplicates value " + std::to_string(index) +
            " first declared in '" + m_sources[index] + "'.");
    }
    m_sources[index] = sourcePath;
}

// Block Database initializes to first pack, not the second.
BlockDatabase::BlockDatabase()
{
    addBlock(BlockId::Air, "Air");
    addBlock(BlockId::Grass, "Grass");
    addBlock(BlockId::Dirt, "Dirt");
    addBlock(BlockId::Stone, "Stone");
    addBlock(BlockId::OakBark, "OakBark");
    addBlock(BlockId::OakLeaf, "OakLeaf");
    addBlock(BlockId::Sand, "Sand");
    addBlock(BlockId::Water, "Water");
    addBlock(BlockId::Cactus, "Cactus");
    addBlock(BlockId::TallGrass, "TallGrass",
             std::make_unique<TallGrassBlockBehavior>(
                 BlockMetadata::TallGrass::Mature));
    addBlock(BlockId::Rose, "Rose");
    addBlock(BlockId::DeadShrub, "DeadShrub");
    addBlock(BlockId::CoalOre, "CoalOre");
    addBlock(BlockId::IronOre, "IronOre");
    addBlock(BlockId::Glass, "Glass",
             std::make_unique<NoDropBlockBehavior>());
    addBlock(BlockId::GlassBorderless, "GlassBorderless",
             std::make_unique<NoDropBlockBehavior>());
    addBlock(BlockId::Chest, "Chest",
             std::make_unique<ChestBlockBehavior>());
    addBlock(BlockId::WheatCrop, "WheatCrop",
             std::make_unique<WheatCropBlockBehavior>(
                 BlockMetadata::WheatCrop::Mature));
    addBlock(BlockId::Workbench, "Workbench",
             std::make_unique<WorkbenchBlockBehavior>());
}

BlockDatabase &BlockDatabase::get()
{
    static BlockDatabase d;
    return d;
}

const BlockType &BlockDatabase::getBlock(BlockId id) const
{
    return *m_blocks[(int)id];
}

const BlockData &BlockDatabase::getData(BlockId id) const
{
    return m_blocks[(int)id]->getData();
}

const BlockDefinition &BlockDatabase::getDefinition(BlockId id) const
{
    return m_definitions[(int)id];
}

void BlockDatabase::addBlock(BlockId id, const std::string &fileName,
                             std::unique_ptr<BlockBehavior> behavior)
{
    auto block = std::make_unique<DefaultBlock>(fileName);
    const auto &data = block->getData().getBlockData();
    const std::string &sourcePath = block->getData().getSourcePath();
    m_idValidator.add(data.id, sourcePath);
    if (data.id != id) {
        throw std::runtime_error(
            "Invalid block file '" + sourcePath +
            "': key 'Id' declares " +
            std::to_string(static_cast<unsigned>(data.id)) +
            " but the registry expects " +
            std::to_string(static_cast<unsigned>(id)) + ".");
    }
    if (!behavior) {
        behavior = std::make_unique<BlockBehavior>();
    }
    BlockDefinition definition = makeDefinition(fileName, *block);
    definition.behavior = behavior.get();
    m_definitions[(int)id] = std::move(definition);
    m_behaviors[(int)id] = std::move(behavior);
    m_blocks[(int)id] = std::move(block);
}
