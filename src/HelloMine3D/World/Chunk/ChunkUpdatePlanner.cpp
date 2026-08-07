#include "ChunkUpdatePlanner.h"

#include "../WorldConstants.h"
#include "../WorldCoordinates.h"

namespace ChunkUpdatePlanner
{
    std::vector<ChunkUpdateKey> planForBlockEdit(int blockX, int blockY,
                                                 int blockZ)
    {
        std::vector<ChunkUpdateKey> updates;
        if (blockY < 0) {
            return updates;
        }

        const auto chunkPosition = WorldCoordinates::getChunkXZ(blockX, blockZ);
        const int chunkSectionY = blockY / CHUNK_SIZE;

        updates.push_back({chunkPosition.x, chunkSectionY, chunkPosition.z});

        const auto sectionBlockXZ = WorldCoordinates::getBlockXZ(blockX, blockZ);
        const int sectionBlockY =
            WorldCoordinates::floorMod(blockY, CHUNK_SIZE);

        if (sectionBlockXZ.x == 0) {
            updates.push_back(
                {chunkPosition.x - 1, chunkSectionY, chunkPosition.z});
        }
        else if (sectionBlockXZ.x == CHUNK_SIZE - 1) {
            updates.push_back(
                {chunkPosition.x + 1, chunkSectionY, chunkPosition.z});
        }

        if (sectionBlockY == 0) {
            updates.push_back(
                {chunkPosition.x, chunkSectionY - 1, chunkPosition.z});
        }
        else if (sectionBlockY == CHUNK_SIZE - 1) {
            updates.push_back(
                {chunkPosition.x, chunkSectionY + 1, chunkPosition.z});
        }

        if (sectionBlockXZ.z == 0) {
            updates.push_back(
                {chunkPosition.x, chunkSectionY, chunkPosition.z - 1});
        }
        else if (sectionBlockXZ.z == CHUNK_SIZE - 1) {
            updates.push_back(
                {chunkPosition.x, chunkSectionY, chunkPosition.z + 1});
        }

        return updates;
    }
}
