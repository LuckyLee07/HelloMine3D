#ifndef WATERRENDERER_H_INCLUDED
#define WATERRENDERER_H_INCLUDED

#include <SFML/Graphics.hpp>
#include <vector>

#include "../Shaders/WaterShader.h"

struct RenderInfo;
class ChunkMesh;
class Camera;
class TextureAtlas;

/// @brief Renderer specifically targeting water and handling shader behaviors.
class WaterRenderer {
  public:
    void add(const ChunkMesh &mesh);
    void render(const Camera &camera, const TextureAtlas &textureAtlas);

  private:
    std::vector<const RenderInfo *> m_chunks;

    WaterShader m_shader;
};

#endif // WATERRENDERER_H_INCLUDED
