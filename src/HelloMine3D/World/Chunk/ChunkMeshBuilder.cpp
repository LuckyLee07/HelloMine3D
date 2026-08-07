#include "ChunkMeshBuilder.h"

#include "ChunkMesh.h"
#include "ChunkSection.h"

#include "../Block/BlockData.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockDefinition.h"

#include <SFML/System/Clock.hpp>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
const std::array<GLfloat, 12> frontFace{
    0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
};

const std::array<GLfloat, 12> backFace{
    1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
};

const std::array<GLfloat, 12> leftFace{
    0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0,
};

const std::array<GLfloat, 12> rightFace{
    1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1,
};

const std::array<GLfloat, 12> topFace{
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0,
};

const std::array<GLfloat, 12> bottomFace{0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};

const std::array<GLfloat, 12> xFace1{
    0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 0,
};

const std::array<GLfloat, 12> xFace2{
    0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
};

constexpr GLfloat LIGHT_TOP = 1.0f;
constexpr GLfloat LIGHT_X = 0.8f;
constexpr GLfloat LIGHT_Z = 0.6f;
constexpr GLfloat LIGHT_BOT = 0.4f;

} // namespace

ChunkMeshBuilder::ChunkMeshBuilder(ChunkSection &chunk,
                                   ChunkMeshCollection &mesh)
    : m_pChunk(&chunk)
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

    sf::Vector3i up;
    sf::Vector3i down;
    sf::Vector3i left;
    sf::Vector3i right;
    sf::Vector3i front;
    sf::Vector3i back;
};

int faces;
void ChunkMeshBuilder::buildMesh()
{
    AdjacentBlockPositions directions;
    m_pBlockPtr = m_pChunk->begin();
    faces = 0;
    sf::Clock timer;
    for (int16_t i = 0; i < CHUNK_VOLUME; i++) {
        const int x = i % CHUNK_SIZE;
        const int y = i / (CHUNK_SIZE * CHUNK_SIZE);
        const int z = (i / CHUNK_SIZE) % CHUNK_SIZE;

        if (!shouldMakeLayer(y)) {
            continue;
        }

        ChunkBlock block = *m_pBlockPtr;
        m_pBlockPtr++;

        sf::Vector3i position(x, y, z);
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
        if ((m_pChunk->getLocation().y != 0) || y != 0)
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

void ChunkMeshBuilder::addXBlockToMesh(const sf::Vector2i &textureCoords,
                                       const sf::Vector3i &blockPosition)
{
    faces++;
    auto texCoords =
        BlockDatabase::get().textureAtlas.getTexture(textureCoords);

    m_pActiveMesh->addFace(xFace1, texCoords, m_pChunk->getLocation(),
                           blockPosition, LIGHT_X);

    m_pActiveMesh->addFace(xFace2, texCoords, m_pChunk->getLocation(),
                           blockPosition, LIGHT_X);
}

void ChunkMeshBuilder::tryAddFaceToMesh(
    const std::array<GLfloat, 12> &blockFace, const sf::Vector2i &textureCoords,
    const sf::Vector3i &blockPosition, const sf::Vector3i &blockFacing,
    GLfloat cardinalLight)
{
    if (shouldMakeFace(blockFacing)) {
        faces++;
        auto texCoords =
            BlockDatabase::get().textureAtlas.getTexture(textureCoords);

        m_pActiveMesh->addFace(blockFace, texCoords, m_pChunk->getLocation(),
                               blockPosition, cardinalLight);
    }
}

bool ChunkMeshBuilder::shouldMakeFace(const sf::Vector3i &adjBlock)
{
    auto block = m_pChunk->getBlock(adjBlock.x, adjBlock.y, adjBlock.z);
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

bool ChunkMeshBuilder::shouldMakeLayer(int y)
{
    auto adjIsSolid = [&](int dx, int dz) {
        const ChunkSection &sect = m_pChunk->getAdjacent(dx, dz);
        return sect.getLayer(y).isAllSolid();
    };

    return (!m_pChunk->getLayer(y).isAllSolid()) ||
           (!m_pChunk->getLayer(y + 1).isAllSolid()) ||
           (!m_pChunk->getLayer(y - 1).isAllSolid()) ||

           (!adjIsSolid(1, 0)) || (!adjIsSolid(0, 1)) || (!adjIsSolid(-1, 0)) ||
           (!adjIsSolid(0, -1));
}
