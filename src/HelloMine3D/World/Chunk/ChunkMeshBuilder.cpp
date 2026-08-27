#include "ChunkMeshBuilder.h"

#include "ChunkMesh.h"
#include "SectionMeshInput.h"

#include "../Block/BlockData.h"
#include "../Block/BlockBehavior.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockTextureCoordinates.h"
#include "../Block/BlockDefinition.h"
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
                                   renderInfo.texTopCoord, position,
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
        VertexLightingQuad lighting;
    };

    const auto sameMaterial = [](const FaceCell &left,
                                 const FaceCell &right) {
        return left.visible && right.visible && left.block == right.block &&
               left.textureCoords.x == right.textureCoords.x &&
               left.textureCoords.y == right.textureCoords.y;
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
            });
    };
    const auto hasSameConstantLighting =
        [&sameCorner, &isConstantLighting](const FaceCell &reference,
                                           const FaceCell &candidate) {
            return isConstantLighting(candidate) &&
                   sameCorner(reference.lighting.corners[0],
                              candidate.lighting.corners[0]);
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
            for (std::size_t corner = 0; corner < 4; ++corner) {
                outerFinal[corner] = rectangle.corners[corner].finalLight;
                outerSmooth[corner] = rectangle.corners[corner].smoothLight;
                outerAo[corner] = static_cast<float>(
                    rectangle.corners[corner].ambientOcclusion);
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
                        if (std::abs(actual.finalLight - expectedFinal) >
                                epsilon ||
                            std::abs(actual.smoothLight - expectedSmooth) >
                                epsilon ||
                            std::abs(static_cast<float>(
                                         actual.ambientOcclusion) -
                                     expectedAo) > epsilon) {
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
                FaceCell &cell = mask[v * CHUNK_SIZE + u];
                cell.visible = true;
                cell.block = block;
                cell.textureCoords = textureCoords;
                cell.lighting = calculateVertexLighting(face, position);
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
    const std::array<float, 8> *textureRepeatCoords)
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

    if (textureRepeatCoords != nullptr) {
        mesh.addSharedFace(blockFace, textureCoords,
                           m_pInput->getLocation(), blockPosition, light,
                           flipDiagonal, *textureRepeatCoords);
    }
    else {
        mesh.addFace(blockFace, textureCoords, m_pInput->getLocation(),
                     blockPosition, light, flipDiagonal, textureRepeatWidth,
                     textureRepeatHeight);
    }
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
    const glm::ivec3 &blockPosition, float verticalScale)
{
    const auto texCoords =
        BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);
    const float light = combineTerrainLight(
        LIGHT_X, m_pInput->getCombinedLight(
                     blockPosition.x, blockPosition.y, blockPosition.z));
    for (const BlockShapeFace &face : shape.faces) {
        BlockShapeFace scaledFace = face;
        for (std::size_t y = 1; y < scaledFace.size(); y += 3) {
            scaledFace[y] *= verticalScale;
        }
        m_pActiveMesh->addFace(scaledFace, texCoords, m_pInput->getLocation(),
                               blockPosition, light);
    }
}

void ChunkMeshBuilder::tryAddFaceToMesh(
    const std::array<float, 12> &blockFace, const glm::ivec2 &textureCoords,
    ChunkBlock block, const glm::ivec3 &blockPosition,
    const glm::ivec3 &blockFacing, CubeFace face)
{
    if (shouldMakeFace(block, blockFacing)) {
        const auto texCoords =
            BlockTextureCoordinates::get(textureCoords.x, textureCoords.y);

        addVertexLitFace(*m_pActiveMesh, face, blockFace, texCoords,
                         blockPosition,
                         calculateVertexLighting(face, blockPosition));
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
