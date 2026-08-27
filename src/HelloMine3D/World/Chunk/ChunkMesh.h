#ifndef CHUNKMESH_H_INCLUDED
#define CHUNKMESH_H_INCLUDED

#include "../../Maths/glm.h"
#include <array>
#include <cstdint>
#include <vector>

struct Mesh {
    std::vector<float> vertexPositions;
    std::vector<float> textureCoords;
    std::vector<float> textureRepeatCoords;
    std::vector<std::uint32_t> indices;
};

class ChunkMesh {
  public:
    ChunkMesh() = default;

    void addFace(const std::array<float, 12> &blockFace,
                 const std::array<float, 8> &textureCoords,
                 const glm::ivec3 &chunkPosition,
                 const glm::ivec3 &blockPosition, float light,
                 float textureRepeatWidth = 1.f,
                 float textureRepeatHeight = 1.f);

    void addFace(const std::array<float, 12> &blockFace,
                 const std::array<float, 8> &textureCoords,
                 const glm::ivec3 &chunkPosition,
                 const glm::ivec3 &blockPosition,
                 const std::array<float, 4> &vertexLight,
                 bool flipDiagonal,
                 float textureRepeatWidth = 1.f,
                 float textureRepeatHeight = 1.f);

    const Mesh &getClientMesh() const;
    const std::vector<float> &getLight() const;

    void clearClientData();

    /// Takes over CPU mesh data built off the world lock. The GPU model is
    /// left alone: it belongs to the section, not to the freshly built data.
    void adoptClientData(ChunkMesh &source);

    int faces = 0;

  private:
    Mesh m_mesh;
    std::vector<float> m_light;
    std::uint32_t m_indexIndex = 0;
};

struct ChunkMeshCollection {
    ChunkMesh solidMesh;
    ChunkMesh transparentMesh;
    ChunkMesh waterMesh;
    ChunkMesh floraMesh;

    void adoptClientData(ChunkMeshCollection &source)
    {
        solidMesh.adoptClientData(source.solidMesh);
        transparentMesh.adoptClientData(source.transparentMesh);
        waterMesh.adoptClientData(source.waterMesh);
        floraMesh.adoptClientData(source.floraMesh);
    }
};

#endif // CHUNKMESH_H_INCLUDED
