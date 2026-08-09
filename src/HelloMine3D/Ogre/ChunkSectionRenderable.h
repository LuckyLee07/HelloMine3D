#pragma once

#include <OgreSimpleRenderable.h>

#include <cstddef>
#include <string>

#include "../Maths/glm.h"

class ChunkMesh;

struct ChunkMeshValidation
{
    bool valid = false;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
    std::string message;
};

class ChunkSectionRenderable final : public Ogre::SimpleRenderable
{
  public:
    ChunkSectionRenderable(const Ogre::String &name, const ChunkMesh &mesh,
                           const glm::ivec3 &sectionLocation);
    ~ChunkSectionRenderable() override;

    static ChunkMeshValidation
    validateCpuMesh(const ChunkMesh &mesh,
                    const glm::ivec3 &sectionLocation);

    Ogre::Real getBoundingRadius() const override;
    Ogre::Real
    getSquaredViewDepth(const Ogre::Camera *camera) const override;

  private:
    Ogre::Real m_boundingRadius = 0.0f;
};
