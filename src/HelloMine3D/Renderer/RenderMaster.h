#ifndef RENDERMASTER_H_INCLUDED
#define RENDERMASTER_H_INCLUDED

#include <SFML/Graphics.hpp>

#include "../Config.h"
#include "BlockOutlineRenderer.h"
#include "ChunkRenderer.h"
#include "FloraRenderer.h"
#include "SkyboxRenderer.h"
#include "WaterRenderer.h"
#include "../Texture/TextureAtlas.h"

class Camera;
class ChunkSection;

/// @brief Master rendering class that handles the sum of drawn in-game objects.
class RenderMaster {
  public:
    void drawChunk(const ChunkSection &chunk);
    void drawSky();
    void drawBlockOutline(const glm::ivec3 &blockPosition);

    void finishRender(sf::Window &window, const Camera &camera);

  private:
    // Chunks
    ChunkRenderer m_chunkRenderer;
    WaterRenderer m_waterRenderer;
    FloraRenderer m_floraRenderer;
    TextureAtlas m_textureAtlas{"DefaultPack"};

    // Detail
    SkyboxRenderer m_skyboxRenderer;
    BlockOutlineRenderer m_blockOutlineRenderer;

    bool m_drawBox = false;
    bool m_drawBlockOutline = false;
    glm::ivec3 m_blockOutlinePosition{0};
};

#endif // RENDERMASTER_H_INCLUDED
