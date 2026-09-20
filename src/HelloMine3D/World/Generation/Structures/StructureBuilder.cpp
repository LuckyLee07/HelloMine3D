#include "StructureBuilder.h"

#include "../../Chunk/Chunk.h"

#include <iostream>

void StructureBuilder::build(Chunk &chunk, bool vegetationOnly)
{
    const glm::ivec2 location = chunk.getLocation();
    const int worldMinX = location.x * CHUNK_SIZE;
    const int worldMinZ = location.y * CHUNK_SIZE;
    for (auto &block : m_blocks) {
        const int localX = block.x - worldMinX;
        const int localZ = block.z - worldMinZ;
        if (localX < 0 || localX >= CHUNK_SIZE ||
            localZ < 0 || localZ >= CHUNK_SIZE) {
            continue;
        }
        if (vegetationOnly) {
            const auto existing = static_cast<BlockId>(chunk.getBlock(localX, block.y, localZ).id);
            // New vegetation must not replace steep banks, water or a previous
            // trunk. Legacy generation and landmark projection keep their rules.
            const bool replaceable = existing == BlockId::Air || existing == BlockId::OakLeaf ||
                existing == BlockId::TallGrass || existing == BlockId::Rose || existing == BlockId::DeadShrub;
            if (!replaceable) { continue; }
        }
        chunk.setBlock(localX, block.y, localZ, block.id);
    }
}

void StructureBuilder::makeColumn(int x, int z, int yStart, int height,
                                  ChunkBlock block)
{
    for (int y = yStart; y < yStart + height; y++) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::makeRowX(int xStart, int xEnd, int y, int z,
                                ChunkBlock block)
{
    for (int x = xStart; x <= xEnd; ++x) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::makeRowZ(int zStart, int zEnd, int x, int y,
                                ChunkBlock block)
{
    for (int z = zStart; z <= zEnd; ++z) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::fill(int y, int xStart, int xEnd, int zStart, int zEnd,
                            ChunkBlock block)
{
    for (int x = xStart; x < xEnd; ++x)
        for (int z = zStart; z < zEnd; ++z) {
            addBlock(x, y, z, block);
        }
}

void StructureBuilder::addBlock(int x, int y, int z, ChunkBlock block)
{
    m_blocks.emplace_back(block, x, y, z);
}
