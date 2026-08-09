#include "RenderMaster.h"

#include <SFML/Graphics.hpp>
#include <iostream>

#include "../Application.h"
#include "../World/Chunk/ChunkMesh.h"
#include "../World/Chunk/ChunkSection.h"

void RenderMaster::drawChunk(const ChunkSection &chunk)
{
    const auto &solidMesh = chunk.getMeshes().solidMesh;
    const auto &waterMesh = chunk.getMeshes().waterMesh;
    const auto &floraMesh = chunk.getMeshes().floraMesh;

    if (solidMesh.getModel().getIndicesCount() > 0)
        m_chunkRenderer.add(solidMesh);

    if (waterMesh.getModel().getIndicesCount() > 0)
        m_waterRenderer.add(waterMesh);

    if (floraMesh.getModel().getIndicesCount() > 0)
        m_floraRenderer.add(floraMesh);
}

void RenderMaster::drawSky()
{
    m_drawBox = true;
}

void RenderMaster::finishRender(sf::Window &window, const Camera &camera)
{
    (void)window;

   // glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    if (m_drawBox) {
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        m_skyboxRenderer.render(camera);
        glDepthMask(GL_TRUE);
        m_drawBox = false;
    }

    // glEnable(GL_CULL_FACE);
    m_chunkRenderer.render(camera, m_textureAtlas);
    m_waterRenderer.render(camera, m_textureAtlas);
    m_floraRenderer.render(camera, m_textureAtlas);
}
