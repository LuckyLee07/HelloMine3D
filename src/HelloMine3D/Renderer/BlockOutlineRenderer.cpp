#include "BlockOutlineRenderer.h"

#include <vector>

#include "../Core/Camera.h"

BlockOutlineRenderer::BlockOutlineRenderer()
{
    const std::vector<GLfloat> vertices{
        0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 0.f, 1.f, 1.f,
    };
    const std::vector<GLuint> indices{
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
    };

    m_outline.genVAO();
    m_outline.addVBO(3, vertices);
    m_outline.addEBO(indices);
}

void BlockOutlineRenderer::render(const Camera &camera,
                                  const glm::ivec3 &blockPosition)
{
    constexpr float expansion = 0.002f;
    glm::mat4 modelMatrix(1.f);
    modelMatrix = glm::translate(
        modelMatrix, glm::vec3(blockPosition) - glm::vec3(expansion));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(1.f + expansion * 2.f));

    m_shader.useProgram();
    m_shader.loadProjectionViewMatrix(camera.getProjectionViewMatrix());
    m_shader.loadModelMatrix(modelMatrix);

    glDisable(GL_BLEND);
    glLineWidth(2.f);
    m_outline.bindVAO();
    glDrawElements(GL_LINES, m_outline.getIndicesCount(), GL_UNSIGNED_INT,
                   nullptr);
    glLineWidth(1.f);
}
