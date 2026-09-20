#ifndef STRUCTUREBUILDER_H_INCLUDED
#define STRUCTUREBUILDER_H_INCLUDED

#include "../../Block/ChunkBlock.h"
#include <vector>

class Chunk;

class StructureBuilder {
    struct Block {
        Block(ChunkBlock id, int x, int y, int z)
            : id(id)
            , x(x)
            , y(y)
            , z(z)
        {
        }

        ChunkBlock id;
        int x, y, z;
    };

  public:
    /// Projects world-space structure blocks into the supplied target chunk.
    void build(Chunk &chunk, bool vegetationOnly = false);

    void makeColumn(int x, int z, int yStart, int height, ChunkBlock block);
    void makeRowX(int xStart, int xEnd, int y, int z, ChunkBlock block);
    void makeRowZ(int zStart, int zEnd, int x, int y, ChunkBlock block);

    void fill(int y, int xStart, int xEnd, int zStart, int zEnd, ChunkBlock block);

    /// Adds one block in world coordinates to the reusable structure plan.
    void addBlock(int x, int y, int z, ChunkBlock block);

  private:
    std::vector<Block> m_blocks;
};

#endif // STRUCTUREBUILDER_H_INCLUDED
