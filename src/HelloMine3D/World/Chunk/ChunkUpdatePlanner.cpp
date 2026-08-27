#include "ChunkUpdatePlanner.h"

#include "../WorldConstants.h"
#include "../WorldCoordinates.h"

#include <array>

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

        const auto sectionBlockXZ = WorldCoordinates::getBlockXZ(blockX, blockZ);
        const int sectionBlockY =
            WorldCoordinates::floorMod(blockY, CHUNK_SIZE);

        std::array<int, 2> sectionX = {chunkPosition.x, chunkPosition.x};
        std::array<int, 2> sectionY = {chunkSectionY, chunkSectionY};
        std::array<int, 2> sectionZ = {chunkPosition.z, chunkPosition.z};
        int sectionXCount = 1;
        int sectionYCount = 1;
        int sectionZCount = 1;

        if (sectionBlockXZ.x == 0) {
            sectionX[sectionXCount++] = chunkPosition.x - 1;
        }
        else if (sectionBlockXZ.x == CHUNK_SIZE - 1) {
            sectionX[sectionXCount++] = chunkPosition.x + 1;
        }

        if (sectionBlockY == 0) {
            sectionY[sectionYCount++] = chunkSectionY - 1;
        }
        else if (sectionBlockY == CHUNK_SIZE - 1) {
            sectionY[sectionYCount++] = chunkSectionY + 1;
        }

        if (sectionBlockXZ.z == 0) {
            sectionZ[sectionZCount++] = chunkPosition.z - 1;
        }
        else if (sectionBlockXZ.z == CHUNK_SIZE - 1) {
            sectionZ[sectionZCount++] = chunkPosition.z + 1;
        }

        // Vertex AO samples face-normal, edge and diagonal neighbours. At a
        // section corner one edit can therefore affect all eight touching
        // sections, not only the three axial neighbours.
        for (int zIndex = 0; zIndex < sectionZCount; ++zIndex) {
            for (int yIndex = 0; yIndex < sectionYCount; ++yIndex) {
                for (int xIndex = 0; xIndex < sectionXCount; ++xIndex) {
                    updates.push_back({sectionX[xIndex], sectionY[yIndex],
                                       sectionZ[zIndex]});
                }
            }
        }

        return updates;
    }
}
