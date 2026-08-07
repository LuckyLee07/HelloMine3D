#ifndef CHUNKUPDATEPLANNER_H_INCLUDED
#define CHUNKUPDATEPLANNER_H_INCLUDED

#include <vector>

struct ChunkUpdateKey {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const ChunkUpdateKey &other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

namespace ChunkUpdatePlanner
{
    std::vector<ChunkUpdateKey> planForBlockEdit(int blockX, int blockY,
                                                 int blockZ);
}

#endif // CHUNKUPDATEPLANNER_H_INCLUDED
