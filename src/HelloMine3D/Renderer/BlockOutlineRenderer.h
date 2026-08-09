#ifndef BLOCKOUTLINERENDERER_H_INCLUDED
#define BLOCKOUTLINERENDERER_H_INCLUDED

#include "../Maths/glm.h"
#include "../Shaders/BasicShader.h"
#include "Model.h"

class Camera;

class BlockOutlineRenderer {
  public:
    BlockOutlineRenderer();

    void render(const Camera &camera, const glm::ivec3 &blockPosition);

  private:
    Model m_outline;
    BasicShader m_shader{"Outline", "Outline"};
};

#endif // BLOCKOUTLINERENDERER_H_INCLUDED
