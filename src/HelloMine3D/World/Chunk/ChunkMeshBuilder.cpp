#include "ChunkMeshBuilder.h"

#include "ChunkMesh.h"
#include "SectionMeshInput.h"

#include "../Block/BlockData.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockTextureCoordinates.h"
#include "../Block/BlockDefinition.h"

#include <algorithm>
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

float combineTerrainLight(float cardinalLight, LightLevel sunlight)
{
    return cardinalLight * lightLevelToBrightness(sunlight);
}

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
    if (!m_pInput->needsMeshBuild()) {
        return;
    }

    buildGreedySolidMesh();

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

        if (block == BlockId::Air) {
            continue;
        }

        const auto &definition = BlockDatabase::get().getDefinition(
            static_cast<BlockId>(block.id));
        if (isGreedySolidBlock(block)) {
            continue;
        }

        setActiveMesh(block);
        const auto &renderInfo = definition.render;

        if (renderInfo.meshType == BlockMeshType::X) {
            addXBlockToMesh(renderInfo.texTopCoord, position);
            continue;
        }

        directions.update(x, y, z);

        // Up/ Down
        if ((m_pInput->getLocation().y != 0) || y != 0)
            tryAddFaceToMesh(bottomFace, renderInfo.texBottomCoord, block,
                             position, directions.down, LIGHT_BOT);
        tryAddFaceToMesh(topFace, renderInfo.texTopCoord, block, position,
                         directions.up, LIGHT_TOP);

        // Left/ Right
        tryAddFaceToMesh(leftFace, renderInfo.texSideCoord, block, position,
                         directions.left, LIGHT_X);
        tryAddFaceToMesh(rightFace, renderInfo.texSideCoord, block, position,
                         directions.right, LIGHT_X);

        // Front/ Back
        tryAddFaceToMesh(frontFace, renderInfo.texSideCoord, block, position,
                         directions.front, LIGHT_Z);
        tryAddFaceToMesh(backFace, renderInfo.texSideCoord, block, position,
                         directions.back, LIGHT_Z);
        }
        }
    }
}

void ChunkMeshBuilder::buildGreedySolidMesh()
{
    buildGreedyFaces(CubeFace::Bottom);
    buildGreedyFaces(CubeFace::Top);
    buildGreedyFaces(CubeFace::Left);
    buildGreedyFaces(CubeFace::Right);
    buildGreedyFaces(CubeFace::Front);
    buildGreedyFaces(CubeFace::Back);
}

void ChunkMeshBuilder::buildGreedyFaces(CubeFace face)
{
    struct FaceCell {
        bool visible = false;
        ChunkBlock block;
        glm::ivec2 textureCoords{0};
        LightLevel light = MIN_LIGHT_LEVEL;
    };

    const auto matches = [](const FaceCell &left, const FaceCell &right) {
        return left.visible && right.visible && left.block == right.block &&
               left.textureCoords.x == right.textureCoords.x &&
               left.textureCoords.y == right.textureCoords.y &&
               left.light == right.light;
    };
    const auto positionFor = [face](int slice, int u, int v) {
        switch (face) {
            case CubeFace::Bottom:
            case CubeFace::Top:
                return glm::ivec3(u, slice, v);
            case CubeFace::Left:
            case CubeFace::Right:
                return glm::ivec3(slice, v, u);
            case CubeFace::Front:
            case CubeFace::Back:
                return glm::ivec3(u, v, slice);
        }
        return glm::ivec3(0);
    };
    const auto adjacentOffset = [face]() {
        switch (face) {
            case CubeFace::Bottom:
                return glm::ivec3(0, -1, 0);
            case CubeFace::Top:
                return glm::ivec3(0, 1, 0);
            case CubeFace::Left:
                return glm::ivec3(-1, 0, 0);
            case CubeFace::Right:
                return glm::ivec3(1, 0, 0);
            case CubeFace::Front:
                return glm::ivec3(0, 0, 1);
            case CubeFace::Back:
                return glm::ivec3(0, 0, -1);
        }
        return glm::ivec3(0);
    }();

    std::array<FaceCell, CHUNK_AREA> mask;
    for (int slice = 0; slice < CHUNK_SIZE; ++slice) {
        mask.fill(FaceCell{});
        if (face == CubeFace::Bottom && slice == 0 &&
            m_pInput->getLocation().y == 0) {
            continue;
        }

        for (int v = 0; v < CHUNK_SIZE; ++v) {
            for (int u = 0; u < CHUNK_SIZE; ++u) {
                const glm::ivec3 position = positionFor(slice, u, v);
                const ChunkBlock block = m_pInput->getBlock(
                    position.x, position.y, position.z);
                if (!isGreedySolidBlock(block) ||
                    !shouldMakeFace(block, position + adjacentOffset)) {
                    continue;
                }

                const auto &renderInfo =
                    BlockDatabase::get()
                        .getDefinition(static_cast<BlockId>(block.id))
                        .render;
                glm::ivec2 textureCoords = renderInfo.texSideCoord;
                if (face == CubeFace::Top) {
                    textureCoords = renderInfo.texTopCoord;
                }
                else if (face == CubeFace::Bottom) {
                    textureCoords = renderInfo.texBottomCoord;
                }
                const LightLevel light = m_pInput->getCombinedLight(
                    position.x + adjacentOffset.x,
                    position.y + adjacentOffset.y,
                    position.z + adjacentOffset.z);
                mask[v * CHUNK_SIZE + u] =
                    {true, block, textureCoords, light};
            }
        }

        for (int v = 0; v < CHUNK_SIZE; ++v) {
            for (int u = 0; u < CHUNK_SIZE;) {
                FaceCell &cell = mask[v * CHUNK_SIZE + u];
                if (!cell.visible) {
                    ++u;
                    continue;
                }

                int width = 1;
                while (u + width < CHUNK_SIZE &&
                       matches(cell, mask[v * CHUNK_SIZE + u + width])) {
                    ++width;
                }

                int height = 1;
                bool canExtend = true;
                while (v + height < CHUNK_SIZE && canExtend) {
                    for (int offset = 0; offset < width; ++offset) {
                        if (!matches(
                                cell,
                                mask[(v + height) * CHUNK_SIZE + u + offset])) {
                            canExtend = false;
                            break;
                        }
                    }
                    if (canExtend) {
                        ++height;
                    }
                }

                addGreedyFace(face, cell.textureCoords, cell.light, slice,
                              u, v, width, height);
                for (int dv = 0; dv < height; ++dv) {
                    for (int du = 0; du < width; ++du) {
                        mask[(v + dv) * CHUNK_SIZE + u + du].visible = false;
                    }
                }
                u += width;
            }
        }
    }
}

void ChunkMeshBuilder::addGreedyFace(CubeFace face,
                                     const glm::ivec2 &textureCoords,
                                     LightLevel light, int slice, int u,
                                     int v, int width, int height)
{
    std::array<float, 12> vertices{};
    glm::ivec3 blockPosition{0};
    float cardinalLight = LIGHT_TOP;
    switch (face) {
        case CubeFace::Bottom:
            vertices = {0, 0, 0, static_cast<float>(width), 0, 0,
                        static_cast<float>(width), 0,
                        static_cast<float>(height), 0, 0,
                        static_cast<float>(height)};
            blockPosition = {u, slice, v};
            cardinalLight = LIGHT_BOT;
            break;
        case CubeFace::Top:
            vertices = {0, 1, static_cast<float>(height),
                        static_cast<float>(width), 1,
                        static_cast<float>(height),
                        static_cast<float>(width), 1, 0, 0, 1, 0};
            blockPosition = {u, slice, v};
            cardinalLight = LIGHT_TOP;
            break;
        case CubeFace::Left:
            vertices = {0, 0, 0, 0, 0, static_cast<float>(width), 0,
                        static_cast<float>(height),
                        static_cast<float>(width), 0,
                        static_cast<float>(height), 0};
            blockPosition = {slice, v, u};
            cardinalLight = LIGHT_X;
            break;
        case CubeFace::Right:
            vertices = {1, 0, static_cast<float>(width), 1, 0, 0, 1,
                        static_cast<float>(height), 0, 1,
                        static_cast<float>(height),
                        static_cast<float>(width)};
            blockPosition = {slice, v, u};
            cardinalLight = LIGHT_X;
            break;
        case CubeFace::Front:
            vertices = {0, 0, 1, static_cast<float>(width), 0, 1,
                        static_cast<float>(width),
                        static_cast<float>(height), 1, 0,
                        static_cast<float>(height), 1};
            blockPosition = {u, v, slice};
            cardinalLight = LIGHT_Z;
            break;
        case CubeFace::Back:
            vertices = {static_cast<float>(width), 0, 0, 0, 0, 0, 0,
                        static_cast<float>(height), 0,
                        static_cast<float>(width),
                        static_cast<float>(height), 0};
            blockPosition = {u, v, slice};
            cardinalLight = LIGHT_Z;
            break;
    }

    const auto atlasCoords =
        BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);
    m_pMeshes->solidMesh.addFace(
        vertices, atlasCoords, m_pInput->getLocation(), blockPosition,
        combineTerrainLight(cardinalLight, light),
        static_cast<float>(width),
        static_cast<float>(height));
}

bool ChunkMeshBuilder::isGreedySolidBlock(ChunkBlock block) const
{
    if (block == BlockId::Air) {
        return false;
    }
    const auto &definition = BlockDatabase::get().getDefinition(
        static_cast<BlockId>(block.id));
    return !definition.transparent &&
           definition.render.meshType == BlockMeshType::Cube &&
           definition.render.shaderType == BlockShaderType::Chunk;
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
                           blockPosition,
                           combineTerrainLight(
                               LIGHT_X,
                               m_pInput->getCombinedLight(blockPosition.x,
                                                         blockPosition.y,
                                                         blockPosition.z)));

    m_pActiveMesh->addFace(xFace2, texCoords, m_pInput->getLocation(),
                           blockPosition,
                           combineTerrainLight(
                               LIGHT_X,
                               m_pInput->getCombinedLight(blockPosition.x,
                                                         blockPosition.y,
                                                         blockPosition.z)));
}

void ChunkMeshBuilder::tryAddFaceToMesh(
    const std::array<float, 12> &blockFace, const glm::ivec2 &textureCoords,
    ChunkBlock block, const glm::ivec3 &blockPosition,
    const glm::ivec3 &blockFacing, float cardinalLight)
{
    if (shouldMakeFace(block, blockFacing)) {
        const auto texCoords =
            BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);

        m_pActiveMesh->addFace(blockFace, texCoords, m_pInput->getLocation(),
                               blockPosition,
                               combineTerrainLight(
                                   cardinalLight,
                                   m_pInput->getCombinedLight(
                                       blockFacing.x, blockFacing.y,
                                       blockFacing.z)));
    }
}

bool ChunkMeshBuilder::shouldMakeFace(ChunkBlock block,
                                      const glm::ivec3 &adjBlock) const
{
    const ChunkBlock adjacent =
        m_pInput->getBlock(adjBlock.x, adjBlock.y, adjBlock.z);
    const auto &currentDefinition = BlockDatabase::get().getDefinition(
        static_cast<BlockId>(block.id));
    const auto &adjacentDefinition = BlockDatabase::get().getDefinition(
        static_cast<BlockId>(adjacent.id));

    if (adjacent == BlockId::Air) {
        return true;
    }
    else if (adjacentDefinition.transparent &&
             adjacentDefinition.id != currentDefinition.id) {
        return true;
    }
    return false;
}
