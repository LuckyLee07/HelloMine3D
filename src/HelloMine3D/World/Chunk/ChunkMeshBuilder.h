#ifndef CHUNKMESHBUILDER_H_INCLUDED
#define CHUNKMESHBUILDER_H_INCLUDED

#include <glad/glad.h>

#include <SFML/Graphics.hpp>
#include <vector>

#include "../Block/ChunkBlock.h"

class ChunkMesh;
class BlockData;
class SectionMeshInput;
struct BlockDefinition;

struct ChunkMeshCollection;

/// @brief Builds the CPU side mesh of one chunk section.
///
/// The builder reads only from a `SectionMeshInput` snapshot, never from the
/// world, so it can run without holding the world lock.
class ChunkMeshBuilder {
  public:
    ChunkMeshBuilder(const SectionMeshInput &input,
                     ChunkMeshCollection &meshes);

    void buildMesh();

  private:
    void setActiveMesh(ChunkBlock block);

    void addXBlockToMesh(const sf::Vector2i &textureCoords,
                         const sf::Vector3i &blockPosition);

    void tryAddFaceToMesh(const std::array<GLfloat, 12> &blockFace,
                          const sf::Vector2i &textureCoords,
                          const sf::Vector3i &blockPosition,
                          const sf::Vector3i &blockFacing,
                          GLfloat cardinalLight);

    bool shouldMakeFace(const sf::Vector3i &blockPosition);

    const SectionMeshInput *m_pInput = nullptr;
    ChunkMeshCollection *m_pMeshes = nullptr;
    ChunkMesh *m_pActiveMesh = nullptr;
    const BlockDefinition *m_pBlockDefinition = nullptr;
};

#endif // CHUNKMESHBUILDER_H_INCLUDED
