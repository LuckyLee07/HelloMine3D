#include "StructureBuilder.h"

#include "../../Chunk/Chunk.h"

#include <iostream>

void StructureBuilder::build(Chunk &chunk)
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
        chunk.setBlock(localX, block.y, localZ, block.id);
    }
}

void StructureBuilder::makeColumn(int x, int z, int yStart, int height,
                                  BlockId block)
{
    for (int y = yStart; y < yStart + height; y++) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::makeRowX(int xStart, int xEnd, int y, int z,
                                BlockId block)
{
    for (int x = xStart; x <= xEnd; ++x) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::makeRowZ(int zStart, int zEnd, int x, int y,
                                BlockId block)
{
    for (int z = zStart; z <= zEnd; ++z) {
        addBlock(x, y, z, block);
    }
}

void StructureBuilder::fill(int y, int xStart, int xEnd, int zStart, int zEnd,
                            BlockId block)
{
    for (int x = xStart; x < xEnd; ++x)
        for (int z = zStart; z < zEnd; ++z) {
            addBlock(x, y, z, block);
        }
}

void StructureBuilder::addBlock(int x, int y, int z, BlockId block)
{
    m_blocks.emplace_back(block, x, y, z);
}
