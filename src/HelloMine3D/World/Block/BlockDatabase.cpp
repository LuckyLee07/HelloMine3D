#include "BlockDatabase.h"

#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
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
    definition.light = 0;
    definition.defaultDrop = Material::toMaterial(data.id).id;
    definition.render.texTopCoord = data.texTopCoord;
    definition.render.texSideCoord = data.texSideCoord;
    definition.render.texBottomCoord = data.texBottomCoord;
    definition.render.meshType = data.meshType;
    definition.render.shaderType = data.shaderType;
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
    addBlock(BlockId::TallGrass, "TallGrass");
    addBlock(BlockId::Rose, "Rose");
    addBlock(BlockId::DeadShrub, "DeadShrub");
    addBlock(BlockId::CoalOre, "CoalOre");
    addBlock(BlockId::IronOre, "IronOre");
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

void BlockDatabase::addBlock(BlockId id, const std::string &fileName)
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
    m_definitions[(int)id] = makeDefinition(fileName, *block);
    m_blocks[(int)id] = std::move(block);
}
