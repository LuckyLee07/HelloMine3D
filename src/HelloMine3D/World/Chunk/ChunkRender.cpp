#include "Chunk.h"

#include "../../Core/Camera.h"
#include "../../Renderer/RenderMaster.h"

void Chunk::drawChunks(RenderMaster &renderer, const Camera &camera,
                       int &meshBufferBudget)
{
    for (auto &chunk : m_chunks) {
        if (chunk.hasMesh()) {
            if (!chunk.hasBuffered()) {
                if (meshBufferBudget <= 0) {
                    continue;
                }

                chunk.bufferMesh();
                --meshBufferBudget;
            }

            if (camera.getFrustum().isBoxInFrustum(chunk.m_aabb)) {
                renderer.drawChunk(chunk);
            }
        }
    }
}
