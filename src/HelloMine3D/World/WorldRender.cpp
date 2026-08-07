#include "World.h"

#include "../Core/Camera.h"
#include "../Renderer/RenderMaster.h"

#include <mutex>
#include <vector>

void World::renderWorld(RenderMaster &renderer, const Camera &camera)
{
    constexpr int kMeshBufferBudgetPerFrame = 4;

    renderer.drawSky();

    std::unique_lock<std::mutex> lock(m_mainMutex);
    int meshBufferBudget = kMeshBufferBudgetPerFrame;

    auto &chunkMap = m_chunkManager.getChunks();
    std::vector<VectorXZ> chunksToUnload;
    for (auto itr = chunkMap.begin(); itr != chunkMap.end(); ++itr) {
        Chunk &chunk = itr->second;

        auto cameraChunk = getChunkXZ(toBlockCoord(camera.position.x),
                                      toBlockCoord(camera.position.z));

        int minX = cameraChunk.x - m_renderDistance;
        int minZ = cameraChunk.z - m_renderDistance;
        int maxX = cameraChunk.x + m_renderDistance;
        int maxZ = cameraChunk.z + m_renderDistance;

        auto location = chunk.getLocation();

        if (minX > location.x || minZ > location.y || maxZ < location.y ||
            maxX < location.x) {
            chunksToUnload.push_back({location.x, location.y});
            continue;
        }

        chunk.drawChunks(renderer, camera, meshBufferBudget);
    }

    for (const auto &location : chunksToUnload) {
        m_chunkManager.unloadChunk(location.x, location.z);
    }
}
