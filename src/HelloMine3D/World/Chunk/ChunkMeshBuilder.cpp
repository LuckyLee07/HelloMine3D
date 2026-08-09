#include "ChunkMeshBuilder.h"

#include "ChunkMesh.h"
#include "SectionMeshInput.h"

#include "../Block/BlockData.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockTextureCoordinates.h"
#include "../Block/BlockDefinition.h"

#include <cassert>
#include <vector>

namespace {
const std::array<float, 12> frontFace{
    0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
};

const std::array<float, 12> backFace{
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
};

const std::array<float, 12> leftFace{
    0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0,
};

const std::array<float, 12> rightFace{
    1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1,
};

const std::array<float, 12> topFace{
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0,
};

const std::array<float, 12> bottomFace{0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};

const std::array<float, 12> xFace1{
    0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0,
};

const std::array<float, 12> xFace2{
    0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
};

constexpr float LIGHT_TOP = 1.0f;
constexpr float LIGHT_X = 0.8f;
constexpr float LIGHT_Z = 0.6f;
constexpr float LIGHT_BOT = 0.4f;

} // namespace

ChunkMeshBuilder::ChunkMeshBuilder(const SectionMeshInput &input,
                                   ChunkMeshCollection &mesh)
    : m_pInput(&input)
    , m_pMeshes(&mesh)
{
}

struct AdjacentBlockPositions {
    void update(int x, int y, int z)
    {
        up = {x, y + 1, z};
        down = {x, y - 1, z};
        left = {x - 1, y, z};
        right = {x + 1, y, z};
        front = {x, y, z + 1};
        back = {x, y, z - 1};
    }

    glm::ivec3 up;
    glm::ivec3 down;
    glm::ivec3 left;
    glm::ivec3 right;
    glm::ivec3 front;
    glm::ivec3 back;
};

void ChunkMeshBuilder::buildMesh()
{
    AdjacentBlockPositions directions;

    for (int y = 0; y < CHUNK_SIZE; ++y) {
        // A layer sealed in on every side emits nothing, so skip the whole
        // slice. Iterating per layer also keeps the block lookup keyed on the
        // real coordinates: the previous version walked a running pointer that
        // was not advanced for skipped layers, which silently offset every
        // block read after the first skipped layer.
        if (!m_pInput->shouldMakeLayer(y)) {
            continue;
        }

        for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int x = 0; x < CHUNK_SIZE; ++x) {
        const ChunkBlock block = m_pInput->getBlock(x, y, z);

        glm::ivec3 position(x, y, z);
        setActiveMesh(block);

        if (block == BlockId::Air) {
            continue;
        }

        m_pBlockDefinition =
            &BlockDatabase::get().getDefinition(static_cast<BlockId>(block.id));
        const auto &renderInfo = m_pBlockDefinition->render;

        if (renderInfo.meshType == BlockMeshType::X) {
            addXBlockToMesh(renderInfo.texTopCoord, position);
            continue;
        }

        directions.update(x, y, z);

        // Up/ Down
        if ((m_pInput->getLocation().y != 0) || y != 0)
            tryAddFaceToMesh(bottomFace, renderInfo.texBottomCoord, position,
                             directions.down, LIGHT_BOT);
        tryAddFaceToMesh(topFace, renderInfo.texTopCoord, position,
                         directions.up, LIGHT_TOP);

        // Left/ Right
        tryAddFaceToMesh(leftFace, renderInfo.texSideCoord, position,
                         directions.left, LIGHT_X);
        tryAddFaceToMesh(rightFace, renderInfo.texSideCoord, position,
                         directions.right, LIGHT_X);

        // Front/ Back
        tryAddFaceToMesh(frontFace, renderInfo.texSideCoord, position,
                         directions.front, LIGHT_Z);
        tryAddFaceToMesh(backFace, renderInfo.texSideCoord, position,
                         directions.back, LIGHT_Z);
        }
        }
    }
}

void ChunkMeshBuilder::setActiveMesh(ChunkBlock block)
{
    const auto &definition =
        BlockDatabase::get().getDefinition(static_cast<BlockId>(block.id));

    switch (definition.render.shaderType) {
        case BlockShaderType::Chunk:
            m_pActiveMesh = &m_pMeshes->solidMesh;
            break;

        case BlockShaderType::Liquid:
            m_pActiveMesh = &m_pMeshes->waterMesh;
            break;

        case BlockShaderType::Flora:
            m_pActiveMesh = &m_pMeshes->floraMesh;
            break;
    }
}

void ChunkMeshBuilder::addXBlockToMesh(const glm::ivec2 &textureCoords,
                                       const glm::ivec3 &blockPosition)
{
    const auto texCoords =
        BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);

    m_pActiveMesh->addFace(xFace1, texCoords, m_pInput->getLocation(),
                           blockPosition, LIGHT_X);

    m_pActiveMesh->addFace(xFace2, texCoords, m_pInput->getLocation(),
                           blockPosition, LIGHT_X);
}

void ChunkMeshBuilder::tryAddFaceToMesh(
    const std::array<float, 12> &blockFace, const glm::ivec2 &textureCoords,
    const glm::ivec3 &blockPosition, const glm::ivec3 &blockFacing,
    float cardinalLight)
{
    if (shouldMakeFace(blockFacing)) {
        const auto texCoords =
            BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);

        m_pActiveMesh->addFace(blockFace, texCoords, m_pInput->getLocation(),
                               blockPosition, cardinalLight);
    }
}

bool ChunkMeshBuilder::shouldMakeFace(const glm::ivec3 &adjBlock)
{
    auto block = m_pInput->getBlock(adjBlock.x, adjBlock.y, adjBlock.z);
    const auto &definition =
        BlockDatabase::get().getDefinition(static_cast<BlockId>(block.id));

    if (block == BlockId::Air) {
        return true;
    }
    else if (definition.transparent &&
             definition.id != m_pBlockDefinition->id) {
        return true;
    }
    return false;
}
