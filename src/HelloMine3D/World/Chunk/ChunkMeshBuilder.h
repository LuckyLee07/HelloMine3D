#ifndef CHUNKMESHBUILDER_H_INCLUDED
#define CHUNKMESHBUILDER_H_INCLUDED

#include "../../Maths/glm.h"

#include <array>
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
    enum class CubeFace {
        Bottom,
        Top,
        Left,
        Right,
        Front,
        Back,
    };

    void buildGreedySolidMesh();
    void buildGreedyFaces(CubeFace face);
    void addGreedyFace(CubeFace face, const glm::ivec2 &textureCoords,
                       int slice, int u, int v, int width, int height);
    bool isGreedySolidBlock(ChunkBlock block) const;

    void setActiveMesh(ChunkBlock block);

    void addXBlockToMesh(const glm::ivec2 &textureCoords,
                         const glm::ivec3 &blockPosition);

    void tryAddFaceToMesh(const std::array<float, 12> &blockFace,
                          const glm::ivec2 &textureCoords,
                          ChunkBlock block,
                          const glm::ivec3 &blockPosition,
                          const glm::ivec3 &blockFacing,
                          float cardinalLight);

    bool shouldMakeFace(ChunkBlock block,
                        const glm::ivec3 &blockPosition) const;

    const SectionMeshInput *m_pInput = nullptr;
    ChunkMeshCollection *m_pMeshes = nullptr;
    ChunkMesh *m_pActiveMesh = nullptr;
};

#endif // CHUNKMESHBUILDER_H_INCLUDED
