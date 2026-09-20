#pragma once

#include "../World/Block/BlockBehavior.h"
#include "../World/Block/BlockDefinition.h"
#include "../World/Block/ChunkBlock.h"
#include "../World/Block/TerrainAppearance.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/WetlandGrassGeometry.h"
#include "../World/Block/ForestFernGeometry.h"

struct BlockSurfaceFace
{
    BlockShapeFace positions;
    glm::ivec2 tile{0};
    std::array<float, 8> repeat{{1,1, 0,1, 0,0, 1,0}};
};

// Presentation geometry uses the registered shape, crop scale and world tile
// selection. It never substitutes the collision box for a plant or a door.
inline std::vector<BlockSurfaceFace> blockSurfaceGeometry(
    const BlockDefinition &definition, ChunkBlock block,
    const glm::ivec3 &position, TerrainBiome biome, int seed)
{
    const auto &render = definition.render;
    const auto tile = [&](TerrainFaceKind kind, const glm::ivec2 &base)
    {
        return TerrainAppearance::select(definition.id, kind, base,
                                         biome, seed, position, block.metadata).coordinates;
    };
    std::vector<BlockSurfaceFace> result;
    if (render.meshType == BlockMeshType::Resource)
    {
        const float height = definition.behavior->verticalRenderScale(definition, block);
        const bool fern = ForestFernGeometry::applies(block,render.shape);
        if (fern || WetlandGrassGeometry::applies(static_cast<BlockId>(block.id), biome, render.shape,block.metadata))
        {
            const auto &database = BlockDatabase::get();
            const auto leafTile = TerrainAppearance::select(BlockId::Grass,
                TerrainFaceKind::Top, database.getDefinition(BlockId::Grass).render.texTopCoord,
                biome, seed, position, block.metadata).coordinates;
            const auto seedTile = database.getDefinition(BlockId::OakBark).render.texSideCoord;
            const auto model = fern ? ForestFernGeometry::build(
                TerrainAppearance::coordinateVariant(seed,position,BlockId::TallGrass),height) : WetlandGrassGeometry::build(
                TerrainAppearance::coordinateVariant(seed, position, BlockId::TallGrass),
                block.metadata >= BlockMetadata::TallGrass::Mature, height);
            for (std::size_t i = 0; i < model.count; ++i)
            {
                const auto &face = model.faces[i];
                result.push_back({face.positions, face.seedHead ? seedTile : leafTile, face.repeat});
            }
            return result;
        }
        for (BlockShapeFace face : render.shape.faces)
        {
            for (std::size_t y = 1; y < face.size(); y += 3)
                face[y] *= height;
            result.push_back({face, tile(TerrainFaceKind::Resource, render.texTopCoord)});
        }
        return result;
    }
    // Same face order and winding as ChunkMeshBuilder (front/back/left/right/top/bottom).
    const std::array<BlockShapeFace, 6> faces{{
        {{0,0,1, 1,0,1, 1,1,1, 0,1,1}},
        {{1,0,0, 0,0,0, 0,1,0, 1,1,0}},
        {{0,0,0, 0,0,1, 0,1,1, 0,1,0}},
        {{1,0,1, 1,0,0, 1,1,0, 1,1,1}},
        {{0,1,1, 1,1,1, 1,1,0, 0,1,0}},
        {{0,0,0, 1,0,0, 1,0,1, 0,0,1}}
    }};
    for (std::size_t i = 0; i < faces.size(); ++i)
    {
        const auto kind = i == 4 ? TerrainFaceKind::Top :
                          i == 5 ? TerrainFaceKind::Bottom : TerrainFaceKind::Side;
        const auto &base = i == 4 ? render.texTopCoord :
                           i == 5 ? render.texBottomCoord : render.texSideCoord;
        result.push_back({faces[i], tile(kind, base)});
    }
    return result;
}
