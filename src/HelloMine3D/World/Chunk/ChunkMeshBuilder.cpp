#include "ChunkMeshBuilder.h"

#include "ChunkMesh.h"
#include "SectionMeshInput.h"

#include "../Block/BlockData.h"
#include "../Block/BlockBehavior.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockTextureCoordinates.h"
#include "../Block/BlockDefinition.h"
#include "../Block/TerrainAppearance.h"
#include "../Block/WetlandGrassGeometry.h"
#include "../../Diagnostics/RuntimeProfiler.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

constexpr float LIGHT_TOP = 1.0f;
constexpr float LIGHT_X = 0.8f;
constexpr float LIGHT_Z = 0.6f;
constexpr float LIGHT_BOT = 0.4f;

float combineTerrainLight(float cardinalLight, LightLevel sunlight)
{
    return std::clamp(cardinalLight * lightLevelToBrightness(sunlight),
                      0.f, 1.f);
}

bool isTrueEnvironmentValue(const char *value)
{
    return value != nullptr &&
           (std::strcmp(value, "1") == 0 ||
            std::strcmp(value, "true") == 0 ||
            std::strcmp(value, "TRUE") == 0 ||
            std::strcmp(value, "on") == 0 ||
            std::strcmp(value, "ON") == 0 ||
            std::strcmp(value, "yes") == 0 ||
            std::strcmp(value, "YES") == 0);
}

bool ambientOcclusionEnabledFromEnvironment()
{
    static const bool enabled = !isTrueEnvironmentValue(
        std::getenv("HELLOMINE3D_DISABLE_VERTEX_AO"));
    return enabled;
}

} // namespace

ChunkMeshBuilder::ChunkMeshBuilder(const SectionMeshInput &input,
                                   ChunkMeshCollection &mesh)
    : ChunkMeshBuilder(input, mesh,
                       ambientOcclusionEnabledFromEnvironment())
{
}

ChunkMeshBuilder::ChunkMeshBuilder(const SectionMeshInput &input,
                                   ChunkMeshCollection &mesh,
                                   bool ambientOcclusionEnabled)
    : m_pInput(&input)
    , m_pMeshes(&mesh)
    , m_ambientOcclusionEnabled(ambientOcclusionEnabled)
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
    HELLOMINE3D_PROFILE_SCOPE("ChunkMeshBuilder::buildMesh");
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

        if (renderInfo.meshType == BlockMeshType::Resource) {
            addResourceShapeToMesh(renderInfo.shape,
                                   renderInfo.texTopCoord, block, position,
                                   definition.behavior->verticalRenderScale(
                                       definition, block));
            continue;
        }

        directions.update(x, y, z);

        // Up/ Down
        if ((m_pInput->getLocation().y != 0) || y != 0)
            tryAddFaceToMesh(bottomFace, renderInfo.texBottomCoord, block,
                             position, directions.down, CubeFace::Bottom);
        tryAddFaceToMesh(topFace, renderInfo.texTopCoord, block, position,
                         directions.up, CubeFace::Top);

        // Left/ Right
        tryAddFaceToMesh(leftFace, renderInfo.texSideCoord, block, position,
                         directions.left, CubeFace::Left);
        tryAddFaceToMesh(rightFace, renderInfo.texSideCoord, block, position,
                         directions.right, CubeFace::Right);

        // Front/ Back
        tryAddFaceToMesh(frontFace, renderInfo.texSideCoord, block, position,
                         directions.front, CubeFace::Front);
        tryAddFaceToMesh(backFace, renderInfo.texSideCoord, block, position,
                         directions.back, CubeFace::Back);
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
    m_pMeshes->solidMesh.beginSharedFaces();

    struct FaceCell {
        bool visible = false;
        ChunkBlock block;
        glm::ivec2 textureCoords{0};
        std::uint16_t appearanceKey = 0;
        VertexLightingQuad lighting;
        std::array<glm::vec2, 4> colour{};
    };

    const auto sameMaterial = [](const FaceCell &left,
                                 const FaceCell &right) {
        return left.visible && right.visible && left.block == right.block &&
               left.textureCoords.x == right.textureCoords.x &&
               left.textureCoords.y == right.textureCoords.y &&
               left.appearanceKey == right.appearanceKey;
    };
    const auto sameCorner = [](const VertexLightCorner &left,
                               const VertexLightCorner &right) {
        return left.smoothLight == right.smoothLight &&
               left.finalLight == right.finalLight &&
               left.ambientOcclusion == right.ambientOcclusion;
    };
    const auto isConstantLighting = [&sameCorner](const FaceCell &cell) {
        return std::all_of(
            cell.lighting.corners.begin() + 1,
            cell.lighting.corners.end(),
            [&](const VertexLightCorner &corner) {
                return sameCorner(cell.lighting.corners[0], corner);
            }) && std::all_of(cell.colour.begin() + 1, cell.colour.end(),
                [&](const glm::vec2 &colour) { return colour == cell.colour[0]; });
    };
    const auto hasSameConstantLighting =
        [&sameCorner, &isConstantLighting](const FaceCell &reference,
                                           const FaceCell &candidate) {
            return isConstantLighting(candidate) &&
                   sameCorner(reference.lighting.corners[0],
                              candidate.lighting.corners[0]) &&
                   reference.colour[0] == candidate.colour[0];
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
    const auto rectangleLighting = [&mask](int startU, int startV,
                                           int width, int height) {
        VertexLightingQuad result;
        result.corners[0] =
            mask[startV * CHUNK_SIZE + startU].lighting.corners[0];
        result.corners[1] =
            mask[startV * CHUNK_SIZE + startU + width - 1]
                .lighting.corners[1];
        result.corners[2] =
            mask[(startV + height - 1) * CHUNK_SIZE + startU + width - 1]
                .lighting.corners[2];
        result.corners[3] =
            mask[(startV + height - 1) * CHUNK_SIZE + startU]
                .lighting.corners[3];
        result.flipDiagonal =
            VertexLighting::shouldFlipDiagonal(result.corners);
        return result;
    };
    const auto reconstructsLighting =
        [&mask, &rectangleLighting](int startU, int startV, int width,
                                    int height) {
            const VertexLightingQuad rectangle =
                rectangleLighting(startU, startV, width, height);
            std::array<float, 4> outerFinal{};
            std::array<float, 4> outerSmooth{};
            std::array<float, 4> outerAo{};
            const std::array<glm::vec2, 4> outerColour = {
                mask[startV * CHUNK_SIZE + startU].colour[0],
                mask[startV * CHUNK_SIZE + startU + width - 1].colour[1],
                mask[(startV + height - 1) * CHUNK_SIZE + startU + width - 1].colour[2],
                mask[(startV + height - 1) * CHUNK_SIZE + startU].colour[3]};
            std::array<float, 4> outerWarmth{}, outerForest{};
            for (std::size_t corner = 0; corner < 4; ++corner) {
                outerFinal[corner] = rectangle.corners[corner].finalLight;
                outerSmooth[corner] = rectangle.corners[corner].smoothLight;
                outerAo[corner] = static_cast<float>(
                    rectangle.corners[corner].ambientOcclusion);
                outerWarmth[corner] = outerColour[corner].x;
                outerForest[corner] = outerColour[corner].y;
            }

            constexpr int cornerU[4] = {0, 1, 1, 0};
            constexpr int cornerV[4] = {0, 0, 1, 1};
            constexpr float epsilon = 0.00001f;
            for (int dv = 0; dv < height; ++dv) {
                for (int du = 0; du < width; ++du) {
                    const FaceCell &cell =
                        mask[(startV + dv) * CHUNK_SIZE + startU + du];
                    for (std::size_t corner = 0; corner < 4; ++corner) {
                        const float x =
                            static_cast<float>(du + cornerU[corner]) /
                            static_cast<float>(width);
                        const float y =
                            static_cast<float>(dv + cornerV[corner]) /
                            static_cast<float>(height);
                        const VertexLightCorner &actual =
                            cell.lighting.corners[corner];
                        const float expectedFinal =
                            VertexLighting::interpolateQuad(
                                outerFinal, rectangle.flipDiagonal, x, y);
                        const float expectedSmooth =
                            VertexLighting::interpolateQuad(
                                outerSmooth, rectangle.flipDiagonal, x, y);
                        const float expectedAo =
                            VertexLighting::interpolateQuad(
                                outerAo, rectangle.flipDiagonal, x, y);
                        const glm::vec2 expectedColour(
                            VertexLighting::interpolateQuad(outerWarmth, rectangle.flipDiagonal, x, y),
                            VertexLighting::interpolateQuad(outerForest, rectangle.flipDiagonal, x, y));
                        if (std::abs(actual.finalLight - expectedFinal) >
                                epsilon ||
                            std::abs(actual.smoothLight - expectedSmooth) >
                                epsilon ||
                            std::abs(static_cast<float>(
                                         actual.ambientOcclusion) -
                                     expectedAo) > epsilon ||
                            std::abs(cell.colour[corner].x - expectedColour.x) > epsilon ||
                            std::abs(cell.colour[corner].y - expectedColour.y) > epsilon) {
                            return false;
                        }
                    }
                }
            }
            return true;
        };

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
                const TerrainTileSelection appearance =
                    selectTerrainTile(block, face, textureCoords,
                                      position);
                FaceCell &cell = mask[v * CHUNK_SIZE + u];
                cell.visible = true;
                cell.block = block;
                cell.textureCoords = appearance.coordinates;
                cell.appearanceKey = appearance.mergeKey;
                if (TerrainEcologyColour::plantTile(appearance.coordinates.x,
                                                    appearance.coordinates.y)) {
                    constexpr int cu[4] = {0, 1, 1, 0};
                    constexpr int cv[4] = {0, 0, 1, 1};
                    const auto positiveFace = glm::max(adjacentOffset, glm::ivec3(0));
                    for (int corner = 0; corner < 4; ++corner) {
                        const auto p = positionFor(slice, u + cu[corner], v + cv[corner]) + positiveFace;
                        cell.colour[corner] = m_pInput->getEcologyColour(p.x, p.z);
                    }
                }
                cell.lighting = calculateVertexLighting(face, position);
                if (m_pInput->containsWater()) {
                    applyShoreTint(face, position, cell.lighting);
                }
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
                bool constantRectangle = isConstantLighting(cell);
                while (u + width < CHUNK_SIZE &&
                       sameMaterial(
                           cell, mask[v * CHUNK_SIZE + u + width])) {
                    const bool nextConstant =
                        constantRectangle && hasSameConstantLighting(
                            cell, mask[v * CHUNK_SIZE + u + width]);
                    if (!nextConstant &&
                        !reconstructsLighting(u, v, width + 1, 1)) {
                        break;
                    }
                    ++width;
                    constantRectangle = nextConstant;
                }

                int height = 1;
                bool canExtend = true;
                while (v + height < CHUNK_SIZE && canExtend) {
                    bool constantRow = constantRectangle;
                    for (int offset = 0; offset < width; ++offset) {
                        const FaceCell &candidate =
                            mask[(v + height) * CHUNK_SIZE + u + offset];
                        if (!sameMaterial(cell, candidate)) {
                            canExtend = false;
                            break;
                        }
                        constantRow =
                            constantRow &&
                            hasSameConstantLighting(cell, candidate);
                    }
                    if (canExtend) {
                        canExtend =
                            constantRow || reconstructsLighting(
                                               u, v, width, height + 1);
                        if (canExtend) {
                            ++height;
                            constantRectangle = constantRow;
                        }
                    }
                }

                addGreedyFace(face, cell.textureCoords,
                              rectangleLighting(u, v, width, height), slice,
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
                                     const VertexLightingQuad &lighting,
                                     int slice, int u, int v, int width,
                                     int height)
{
    std::array<float, 12> vertices{};
    glm::ivec3 blockPosition{0};
    switch (face) {
        case CubeFace::Bottom:
            vertices = {0, 0, 0, static_cast<float>(width), 0, 0,
                        static_cast<float>(width), 0,
                        static_cast<float>(height), 0, 0,
                        static_cast<float>(height)};
            blockPosition = {u, slice, v};
            break;
        case CubeFace::Top:
            vertices = {0, 1, static_cast<float>(height),
                        static_cast<float>(width), 1,
                        static_cast<float>(height),
                        static_cast<float>(width), 1, 0, 0, 1, 0};
            blockPosition = {u, slice, v};
            break;
        case CubeFace::Left:
            vertices = {0, 0, 0, 0, 0, static_cast<float>(width), 0,
                        static_cast<float>(height),
                        static_cast<float>(width), 0,
                        static_cast<float>(height), 0};
            blockPosition = {slice, v, u};
            break;
        case CubeFace::Right:
            vertices = {1, 0, static_cast<float>(width), 1, 0, 0, 1,
                        static_cast<float>(height), 0, 1,
                        static_cast<float>(height),
                        static_cast<float>(width)};
            blockPosition = {slice, v, u};
            break;
        case CubeFace::Front:
            vertices = {0, 0, 1, static_cast<float>(width), 0, 1,
                        static_cast<float>(width),
                        static_cast<float>(height), 1, 0,
                        static_cast<float>(height), 1};
            blockPosition = {u, v, slice};
            break;
        case CubeFace::Back:
            vertices = {static_cast<float>(width), 0, 0, 0, 0, 0, 0,
                        static_cast<float>(height), 0,
                        static_cast<float>(width),
                        static_cast<float>(height), 0};
            blockPosition = {u, v, slice};
            break;
    }

    auto atlasCoords =
        BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);
    // The terrain shader only uses uv0 to select a tile. A single canonical
    // coordinate therefore preserves the sampled tile while allowing
    // coplanar split faces to share a vertex.
    for (std::size_t index = 2; index < atlasCoords.size(); index += 2) {
        atlasCoords[index] = atlasCoords[0];
        atlasCoords[index + 1] = atlasCoords[1];
    }

    const float startU = static_cast<float>(u);
    const float endU = static_cast<float>(u + width);
    const float startV = static_cast<float>(v);
    const float endV = static_cast<float>(v + height);
    const float edge = static_cast<float>(CHUNK_SIZE);
    std::array<float, 8> repeatCoords{};
    switch (face) {
        case CubeFace::Top:
            repeatCoords = {edge - startU, endV, edge - endU, endV,
                            edge - endU, startV, edge - startU, startV};
            break;
        case CubeFace::Right:
        case CubeFace::Back:
            repeatCoords = {endU, edge - startV, startU, edge - startV,
                            startU, edge - endV, endU, edge - endV};
            break;
        case CubeFace::Bottom:
        case CubeFace::Left:
        case CubeFace::Front:
            repeatCoords = {edge - startU, edge - startV,
                            edge - endU, edge - startV,
                            edge - endU, edge - endV,
                            edge - startU, edge - endV};
            break;
    }
    addVertexLitFace(m_pMeshes->solidMesh, face, vertices, atlasCoords,
                     blockPosition, lighting, static_cast<float>(width),
                     static_cast<float>(height), &repeatCoords);
}

float ChunkMeshBuilder::shoreTintAt(const glm::ivec3 &corner) const
{
    const int index = corner.x + ShoreCornerSize *
        (corner.z + ShoreCornerSize * corner.y);
    auto &sample = m_shoreSamples[index];
    if (sample == 0) {
        int wetColumns = 0;
        for (int z = -1; z <= 0; ++z) {
            for (int x = -1; x <= 0; ++x) {
                if (m_pInput->getBlock(corner.x + x, corner.y - 1,
                                       corner.z + z) == BlockId::Water ||
                    m_pInput->getBlock(corner.x + x, corner.y,
                                       corner.z + z) == BlockId::Water) {
                    ++wetColumns;
                }
            }
        }
        sample = static_cast<std::uint8_t>(std::min(wetColumns, 2) + 1);
    }
    return 1.f - .09f * static_cast<float>(sample - 1);
}

void ChunkMeshBuilder::applyShoreTint(
    CubeFace face, const glm::ivec3 &blockPosition,
    VertexLightingQuad &lighting) const
{
    glm::ivec3 origin = blockPosition;
    glm::ivec3 u{0}, v{0};
    switch (face) {
        case CubeFace::Bottom:
        case CubeFace::Top:
            origin.y += face == CubeFace::Top ? 1 : 0;
            u.x = 1; v.z = 1;
            break;
        case CubeFace::Left:
        case CubeFace::Right:
            origin.x += face == CubeFace::Right ? 1 : 0;
            u.z = 1; v.y = 1;
            break;
        case CubeFace::Front:
        case CubeFace::Back:
            origin.z += face == CubeFace::Front ? 1 : 0;
            u.x = 1; v.y = 1;
            break;
    }
    const std::array<glm::ivec3, 4> corners{
        origin, origin + u, origin + u + v, origin + v};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        // Only the derived mesh shade changes. Stored sunlight, block light
        // and AO remain untouched; existing greedy checks retain this gradient.
        lighting.corners[i].finalLight *= shoreTintAt(corners[i]);
    }
}

VertexLightingQuad ChunkMeshBuilder::calculateVertexLighting(
    CubeFace face, const glm::ivec3 &blockPosition) const
{
    glm::ivec3 normal{0};
    glm::ivec3 tangentU{0};
    glm::ivec3 tangentV{0};
    float cardinalLight = LIGHT_TOP;
    switch (face) {
        case CubeFace::Bottom:
            normal = {0, -1, 0};
            tangentU = {1, 0, 0};
            tangentV = {0, 0, 1};
            cardinalLight = LIGHT_BOT;
            break;
        case CubeFace::Top:
            normal = {0, 1, 0};
            tangentU = {1, 0, 0};
            tangentV = {0, 0, 1};
            cardinalLight = LIGHT_TOP;
            break;
        case CubeFace::Left:
            normal = {-1, 0, 0};
            tangentU = {0, 0, 1};
            tangentV = {0, 1, 0};
            cardinalLight = LIGHT_X;
            break;
        case CubeFace::Right:
            normal = {1, 0, 0};
            tangentU = {0, 0, 1};
            tangentV = {0, 1, 0};
            cardinalLight = LIGHT_X;
            break;
        case CubeFace::Front:
            normal = {0, 0, 1};
            tangentU = {1, 0, 0};
            tangentV = {0, 1, 0};
            cardinalLight = LIGHT_Z;
            break;
        case CubeFace::Back:
            normal = {0, 0, -1};
            tangentU = {1, 0, 0};
            tangentV = {0, 1, 0};
            cardinalLight = LIGHT_Z;
            break;
    }

    constexpr int tangentUSign[4] = {-1, 1, 1, -1};
    constexpr int tangentVSign[4] = {-1, -1, 1, 1};
    const glm::ivec3 centre = blockPosition + normal;

    std::array<std::array<LightLevel, 3>, 3> neighbourhoodLight{};
    std::array<std::array<bool, 3>, 3> neighbourhoodOcclusion{};
    for (int u = -1; u <= 1; ++u) {
        for (int v = -1; v <= 1; ++v) {
            const glm::ivec3 sample =
                centre + tangentU * u + tangentV * v;
            sampleVertexLighting(
                sample, neighbourhoodLight[u + 1][v + 1],
                neighbourhoodOcclusion[u + 1][v + 1]);
        }
    }

    VertexLightingQuad lighting;
    for (std::size_t corner = 0; corner < 4; ++corner) {
        const int u = tangentUSign[corner] + 1;
        const int v = tangentVSign[corner] + 1;

        VertexLightCornerSamples samples;
        samples.centre = neighbourhoodLight[1][1];
        samples.sideU = neighbourhoodLight[u][1];
        samples.sideV = neighbourhoodLight[1][v];
        samples.diagonal = neighbourhoodLight[u][v];
        samples.sideUOccludes = neighbourhoodOcclusion[u][1];
        samples.sideVOccludes = neighbourhoodOcclusion[1][v];
        samples.diagonalOccludes = neighbourhoodOcclusion[u][v];
        lighting.corners[corner] =
            VertexLighting::evaluateCorner(
                cardinalLight, samples, m_ambientOcclusionEnabled);
    }
    lighting.flipDiagonal =
        VertexLighting::shouldFlipDiagonal(lighting.corners);
    return lighting;
}

void ChunkMeshBuilder::sampleVertexLighting(
    const glm::ivec3 &position, LightLevel &light, bool &occludes) const
{
    const int x = position.x + 1;
    const int y = position.y + 1;
    const int z = position.z + 1;
    assert(x >= 0 && x < VertexSampleSize && y >= 0 &&
           y < VertexSampleSize && z >= 0 && z < VertexSampleSize);
    const int index = x + VertexSampleSize *
                              (z + VertexSampleSize * y);
    CachedVertexSample &sample = m_vertexSamples[index];
    if (!sample.valid) {
        sample.light = m_pInput->getCombinedLight(
            position.x, position.y, position.z);
        sample.occludes = isAmbientOccluder(position);
        sample.valid = true;
    }
    light = sample.light;
    occludes = sample.occludes;
}

bool ChunkMeshBuilder::isAmbientOccluder(
    const glm::ivec3 &position) const
{
    const ChunkBlock block =
        m_pInput->getBlock(position.x, position.y, position.z);
    if (block == BlockId::Air) {
        return false;
    }

    // Water, glass and resource flora are transparent in the block contract,
    // so they do not become solid AO walls. Out-of-halo and unloaded samples
    // resolve to Air through SectionMeshInput and follow the same rule.
    return !BlockDatabase::get()
                .getDefinition(static_cast<BlockId>(block.id))
                .transparent;
}

void ChunkMeshBuilder::addVertexLitFace(
    ChunkMesh &mesh, CubeFace face,
    const std::array<float, 12> &blockFace,
    const std::array<float, 8> &textureCoords,
    const glm::ivec3 &blockPosition,
    const VertexLightingQuad &lighting, float textureRepeatWidth,
    float textureRepeatHeight,
    const std::array<float, 8> *textureRepeatCoords, bool shareRepeatVertices)
{
    std::array<float, 4> light{};
    bool flipDiagonal = lighting.flipDiagonal;
    switch (face) {
        case CubeFace::Bottom:
        case CubeFace::Left:
        case CubeFace::Front:
            light = {lighting.corners[0].finalLight,
                     lighting.corners[1].finalLight,
                     lighting.corners[2].finalLight,
                     lighting.corners[3].finalLight};
            break;
        case CubeFace::Top:
            light = {lighting.corners[3].finalLight,
                     lighting.corners[2].finalLight,
                     lighting.corners[1].finalLight,
                     lighting.corners[0].finalLight};
            flipDiagonal = !flipDiagonal;
            break;
        case CubeFace::Right:
        case CubeFace::Back:
            light = {lighting.corners[1].finalLight,
                     lighting.corners[0].finalLight,
                     lighting.corners[3].finalLight,
                     lighting.corners[2].finalLight};
            flipDiagonal = !flipDiagonal;
            break;
    }

    const auto tintedCoords = ecologyCoordinates(blockFace, textureCoords, blockPosition);
    if (textureRepeatCoords != nullptr && shareRepeatVertices) {
        mesh.addSharedFace(blockFace, tintedCoords,
                           m_pInput->getLocation(), blockPosition, light,
                           flipDiagonal, *textureRepeatCoords);
    }
    else if (textureRepeatCoords != nullptr) {
        mesh.addFace(blockFace, tintedCoords, m_pInput->getLocation(),
                     blockPosition, light, flipDiagonal, *textureRepeatCoords);
    }
    else {
        mesh.addFace(blockFace, tintedCoords, m_pInput->getLocation(),
                     blockPosition, light, flipDiagonal, textureRepeatWidth,
                     textureRepeatHeight);
    }
}

std::array<float, 8> ChunkMeshBuilder::ecologyCoordinates(
    const std::array<float, 12> &positions,
    const std::array<float, 8> &coordinates,
    const glm::ivec3 &blockPosition) const
{
    const float tiles = static_cast<float>(runtimeTerrainMaterialProfile().parameters().tilesPerRow);
    const int tx = static_cast<int>(std::floor(coordinates[0] * tiles));
    const int ty = static_cast<int>(std::floor(coordinates[1] * tiles));
    if (!TerrainEcologyColour::plantTile(tx, ty)) return coordinates;
    std::array<float, 8> result{};
    for (int i = 0; i < 4; ++i) {
        const auto climate = m_pInput->getEcologyColour(
            blockPosition.x + positions[i * 3], blockPosition.z + positions[i * 3 + 2]);
        const auto encoded = TerrainEcologyColour::encode(tx, ty, climate, tiles);
        result[i * 2] = encoded.x;
        result[i * 2 + 1] = encoded.y;
    }
    return result;
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

        case BlockShaderType::Transparent:
            m_pActiveMesh = &m_pMeshes->transparentMesh;
            break;
    }
}

void ChunkMeshBuilder::addResourceShapeToMesh(
    const BlockShape &shape, const glm::ivec2 &textureCoords,
    ChunkBlock block, const glm::ivec3 &blockPosition,
    float verticalScale)
{
    const TerrainTileSelection appearance = TerrainAppearance::select(
        static_cast<BlockId>(block.id), TerrainFaceKind::Resource,
        textureCoords,
        m_pInput->getBiome(blockPosition.x, blockPosition.z),
        m_pInput->getTerrainSeed(), worldPositionFor(blockPosition));
    const auto texCoords =
        BlockTextureCoordinates::get(appearance.coordinates.x,
                                     appearance.coordinates.y);
    const float light = combineTerrainLight(
        LIGHT_X, m_pInput->getCombinedLight(
                     blockPosition.x, blockPosition.y, blockPosition.z));
    if (WetlandGrassGeometry::applies(static_cast<BlockId>(block.id), appearance.biome, shape)) {
        const auto &database = BlockDatabase::get();
        const auto leafTile = TerrainAppearance::select(BlockId::Grass,
            TerrainFaceKind::Top, database.getDefinition(BlockId::Grass).render.texTopCoord,
            appearance.biome, m_pInput->getTerrainSeed(),
            worldPositionFor(blockPosition)).coordinates;
        const auto seedTile = database.getDefinition(BlockId::OakBark).render.texSideCoord;
        const auto model = WetlandGrassGeometry::build(appearance.variant,
            block.metadata >= BlockMetadata::TallGrass::Mature, verticalScale);
        for (std::size_t i = 0; i < model.count; ++i) {
            const auto &face = model.faces[i];
            const auto tile = face.seedHead ? seedTile : leafTile;
            m_pActiveMesh->addFace(face.positions,
                ecologyCoordinates(face.positions, BlockTextureCoordinates::get(tile.x, tile.y), blockPosition), m_pInput->getLocation(),
                blockPosition, {light, light, light, light}, false, face.repeat);
        }
        return;
    }
    for (const BlockShapeFace &face : shape.faces) {
        BlockShapeFace scaledFace = face;
        for (std::size_t y = 1; y < scaledFace.size(); y += 3) {
            scaledFace[y] *= verticalScale;
        }
        m_pActiveMesh->addFace(scaledFace, ecologyCoordinates(scaledFace, texCoords, blockPosition), m_pInput->getLocation(),
                               blockPosition, light);
    }
}

void ChunkMeshBuilder::tryAddFaceToMesh(
    const std::array<float, 12> &blockFace, const glm::ivec2 &textureCoords,
    ChunkBlock block, const glm::ivec3 &blockPosition,
    const glm::ivec3 &blockFacing, CubeFace face)
{
    if (shouldMakeFace(block, blockFacing)) {
        const TerrainTileSelection appearance =
            selectTerrainTile(block, face, textureCoords, blockPosition);
        const auto texCoords =
            BlockTextureCoordinates::get(appearance.coordinates.x,
                                         appearance.coordinates.y);

        if (block == BlockId::Water) {
            std::array<float, 8> waterData{};
            std::array<float, 8> waterDrift{};
            for (int corner = 0; corner < 4; ++corner) {
                const int cx = blockPosition.x + static_cast<int>(blockFace[corner * 3]);
                const int cz = blockPosition.z + static_cast<int>(blockFace[corner * 3 + 2]);
                const int y = blockPosition.y +
                    static_cast<int>(blockFace[corner * 3 + 1]) - 1;
                glm::vec2 bankGradient(0.f);
                float waterCoverage = 0.f;
                for (int dz = -1; dz <= 0; ++dz) {
                    for (int dx = -1; dx <= 0; ++dx) {
                        waterData[corner * 2] +=
                            m_pInput->getWaterDepth(cx + dx, y, cz + dz) * 0.25f;
                        const auto neighbour = m_pInput->getBlock(cx + dx, y, cz + dz);
                        if (neighbour == BlockId::Water) waterCoverage += 0.25f;
                        if (neighbour != BlockId::Air && neighbour != BlockId::Water &&
                            !BlockDatabase::get().getDefinition(
                                static_cast<BlockId>(neighbour.id)).transparent) {
                            waterData[corner * 2 + 1] += 0.25f;
                            bankGradient += glm::vec2(dx == 0 ? .5f : -.5f,
                                                      dz == 0 ? .5f : -.5f);
                        }
                    }
                }
                // Wind-driven surface drift bends along the actual resident
                // bank. E5 has level water, so this is not a river discharge
                // model. The same four columns define both sides of a seam.
                glm::vec2 drift(.8f, .6f);
                const float gradientSquared = glm::dot(bankGradient, bankGradient);
                if (gradientSquared > 0.f)
                    drift -= bankGradient * glm::dot(drift, bankGradient) / gradientSquared;
                drift *= waterCoverage;
                waterDrift[corner * 2] = drift.x;
                waterDrift[corner * 2 + 1] = drift.y;
            }
            // Water does not sample the atlas; uv0 carries surface drift while
            // uv1 retains depth/shore, with no extra vertices or vertex stride.
            addVertexLitFace(*m_pActiveMesh, face, blockFace, waterDrift,
                blockPosition, calculateVertexLighting(face, blockPosition),
                1.f, 1.f, &waterData, false);
            return;
        }

        addVertexLitFace(*m_pActiveMesh, face, blockFace, texCoords,
                         blockPosition,
                         calculateVertexLighting(face, blockPosition));
    }
}

TerrainTileSelection ChunkMeshBuilder::selectTerrainTile(
    ChunkBlock block, CubeFace face,
    const glm::ivec2 &baseCoordinates,
    const glm::ivec3 &blockPosition) const
{
    TerrainFaceKind faceKind = TerrainFaceKind::Side;
    if (face == CubeFace::Top) {
        faceKind = TerrainFaceKind::Top;
    }
    else if (face == CubeFace::Bottom) {
        faceKind = TerrainFaceKind::Bottom;
    }
    return TerrainAppearance::select(
        static_cast<BlockId>(block.id), faceKind, baseCoordinates,
        m_pInput->getBiome(blockPosition.x, blockPosition.z),
        m_pInput->getTerrainSeed(), worldPositionFor(blockPosition));
}

glm::ivec3 ChunkMeshBuilder::worldPositionFor(
    const glm::ivec3 &blockPosition) const
{
    return m_pInput->getLocation() * CHUNK_SIZE + blockPosition;
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

    if (!adjacentDefinition.transparent) {
        return false;
    }

    if (adjacentDefinition.render.meshType == BlockMeshType::Resource) {
        return true;
    }

    if (adjacentDefinition.id == currentDefinition.id) {
        return false;
    }

    // Transparent glass variants are one optical medium. Suppressing their
    // shared face prevents coplanar triangles and visible internal seams.
    const bool currentIsGlass =
        currentDefinition.render.shaderType == BlockShaderType::Transparent;
    const bool adjacentIsGlass =
        adjacentDefinition.render.shaderType == BlockShaderType::Transparent;
    return !(currentIsGlass && adjacentIsGlass);
}
