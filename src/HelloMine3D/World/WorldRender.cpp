#include "World.h"

#include "../Core/Camera.h"
#include "../Renderer/RenderMaster.h"

#include <mutex>
#include <vector>

void World::renderWorld(RenderMaster &renderer, const Camera &camera)
{
    // A section with a CPU mesh but no GPU buffer cannot be drawn, so this
    // budget directly bounds how fast terrain becomes visible. At 4 per frame
    // it took 5.5 s to upload one render-distance worth of sections.
    constexpr int kMeshBufferBudgetPerFrame = 32;

    renderer.drawSky();

    std::unique_lock<std::mutex> lock(m_mainMutex);
    int meshBufferBudget = kMeshBufferBudgetPerFrame;

    // Hoisted out of the loop: the camera does not move while we draw.
    const auto cameraChunk = getChunkXZ(toBlockCoord(camera.position.x),
                                        toBlockCoord(camera.position.z));
    const int minX = cameraChunk.x - m_renderDistance;
    const int minZ = cameraChunk.z - m_renderDistance;
    const int maxX = cameraChunk.x + m_renderDistance;
    const int maxZ = cameraChunk.z + m_renderDistance;

    for (auto &entry : m_chunkManager.getChunks()) {
        Chunk &chunk = entry.second;
        const auto location = chunk.getLocation();

        // Out-of-range chunks are dropped by unloadDistantChunks() during the
        // update step. Unloading here would put a synchronous chunk save on
        // the render path while the world lock is held.
        if (minX > location.x || minZ > location.y || maxZ < location.y ||
            maxX < location.x) {
            continue;
        }

        chunk.drawChunks(renderer, camera, meshBufferBudget);
    }
}

void World::unloadDistantChunks(const Camera &camera)
{
    // Each unload saves the chunk to disk when it is dirty, so this is
    // budgeted: a fast-moving player must not stall on a burst of file writes.
    constexpr std::size_t kMaxUnloadsPerUpdate = 8;

    const auto cameraChunk = getChunkXZ(toBlockCoord(camera.position.x),
                                        toBlockCoord(camera.position.z));

    // Nothing can leave the view distance until the camera crosses into a new
    // chunk, so scanning every frame is pure lock pressure. Re-run while there
    // is still a backlog from the last move.
    if (m_unloadScanValid && cameraChunk == m_lastUnloadScanChunk &&
        !m_unloadBacklog) {
        return;
    }
    m_lastUnloadScanChunk = cameraChunk;
    m_unloadScanValid = true;

    std::unique_lock<std::mutex> lock(m_mainMutex);
    const int minX = cameraChunk.x - m_renderDistance;
    const int minZ = cameraChunk.z - m_renderDistance;
    const int maxX = cameraChunk.x + m_renderDistance;
    const int maxZ = cameraChunk.z + m_renderDistance;

    std::vector<VectorXZ> chunksToUnload;
    for (const auto &entry : m_chunkManager.getChunks()) {
        const auto location = entry.second.getLocation();
        if (minX > location.x || minZ > location.y || maxZ < location.y ||
            maxX < location.x) {
            chunksToUnload.push_back({location.x, location.y});
            if (chunksToUnload.size() >= kMaxUnloadsPerUpdate) {
                break;
            }
        }
    }

    // The budget may have cut the list short; remember to come back next
    // update even if the camera stays in the same chunk.
    m_unloadBacklog = chunksToUnload.size() >= kMaxUnloadsPerUpdate;

    for (const auto &location : chunksToUnload) {
        m_chunkManager.unloadChunk(location.x, location.z);
    }
}
