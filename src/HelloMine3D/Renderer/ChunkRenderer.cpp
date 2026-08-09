#include "ChunkRenderer.h"

#include "../Texture/TextureAtlas.h"
#include "../World/Chunk/ChunkMesh.h"

#include "../Core/Camera.h"

#include <iostream>

void ChunkRenderer::add(const ChunkMesh &mesh)
{
    m_chunks.push_back(&mesh.getModel().getRenderInfo());
}

void ChunkRenderer::render(const Camera &camera,
                           const TextureAtlas &textureAtlas)
{
    if (m_chunks.empty()) {
        return;
    }

    glDisable(GL_BLEND);
    // glEnable(GL_CULL_FACE);

    m_shader.useProgram();
    textureAtlas.bindTexture();

    m_shader.loadProjectionViewMatrix(camera.getProjectionViewMatrix());

    for (auto mesh : m_chunks) {
        GL::bindVAO(mesh->vao);
        GL::drawElements(mesh->indicesCount);
    }

    m_chunks.clear();
}
