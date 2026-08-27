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

    void addFace(const std::array<float, 12> &blockFace,
                 const std::array<float, 8> &textureCoords,
                 const glm::ivec3 &chunkPosition,
                 const glm::ivec3 &blockPosition,
                 const std::array<float, 4> &vertexLight,
                 bool flipDiagonal,
                 const std::array<float, 8> &textureRepeatCoords);

    /// Emits a face while reusing byte-identical vertices already present in
    /// this mesh. Indices and face topology stay unchanged.
    void addSharedFace(
        const std::array<float, 12> &blockFace,
        const std::array<float, 8> &textureCoords,
        const glm::ivec3 &chunkPosition,
        const glm::ivec3 &blockPosition,
        const std::array<float, 4> &vertexLight, bool flipDiagonal,
        const std::array<float, 8> &textureRepeatCoords);

    /// Starts a coplanar face group. The fixed 17^3 section-corner cache is
    /// reset without touching vertices emitted by previous groups.
    void beginSharedFaces();

    const Mesh &getClientMesh() const;
    const std::vector<float> &getLight() const;

    void clearClientData();

    /// Takes over CPU mesh data built off the world lock. The GPU model is
    /// left alone: it belongs to the section, not to the freshly built data.
    void adoptClientData(ChunkMesh &source);

    int faces = 0;

  private:
    struct SharedVertexEntry
    {
        std::array<std::uint32_t, 5> attributes{};
        std::uint32_t vertexIndex = 0;
        std::int32_t next = -1;
    };

    void addFaceInternal(
        const std::array<float, 12> &blockFace,
        const std::array<float, 8> &textureCoords,
        const glm::ivec3 &chunkPosition,
        const glm::ivec3 &blockPosition,
        const std::array<float, 4> &vertexLight, bool flipDiagonal,
        const std::array<float, 8> &textureRepeatCoords,
        bool shareVertices);

    Mesh m_mesh;
    std::vector<float> m_light;
    std::vector<std::int32_t> m_sharedVertexHeads;
    std::vector<std::uint32_t> m_sharedVertexGenerations;
    std::vector<SharedVertexEntry> m_sharedVertexEntries;
    std::uint32_t m_sharedVertexGeneration = 0;
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
