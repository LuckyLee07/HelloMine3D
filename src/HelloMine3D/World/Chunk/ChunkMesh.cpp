#include "ChunkMesh.h"

#include "../WorldConstants.h"

#include <iostream>

void ChunkMesh::addFace(const std::array<float, 12> &blockFace,
                        const std::array<float, 8> &textureCoords,
                        const glm::ivec3 &chunkPosition,
                        const glm::ivec3 &blockPosition, float cardinalLight)
{
    faces++;
    auto &verticies = m_mesh.vertexPositions;
    auto &texCoords = m_mesh.textureCoords;
    auto &indices = m_mesh.indices;

    texCoords.insert(texCoords.end(), textureCoords.begin(),
                     textureCoords.end());

    /// Vertex: The current vertex in the "blockFace" vector, 4 vertex in total
    /// hence "< 4" Index: X, Y, Z
    for (int i = 0, index = 0; i < 4; ++i) {
        verticies.push_back(blockFace[index++] + chunkPosition.x * CHUNK_SIZE +
                            blockPosition.x);
        verticies.push_back(blockFace[index++] + chunkPosition.y * CHUNK_SIZE +
                            blockPosition.y);
        verticies.push_back(blockFace[index++] + chunkPosition.z * CHUNK_SIZE +
                            blockPosition.z);
        m_light.push_back(cardinalLight);
    }

    indices.insert(indices.end(),
                   {m_indexIndex, m_indexIndex + 1, m_indexIndex + 2,

                    m_indexIndex + 2, m_indexIndex + 3, m_indexIndex});
    m_indexIndex += 4;
}

void ChunkMesh::clearClientData()
{
    m_mesh.vertexPositions.clear();
    m_mesh.textureCoords.clear();
    m_mesh.indices.clear();
    m_light.clear();

    m_indexIndex = 0;
    faces = 0;
}

void ChunkMesh::adoptClientData(ChunkMesh &source)
{
    m_mesh.vertexPositions = std::move(source.m_mesh.vertexPositions);
    m_mesh.textureCoords = std::move(source.m_mesh.textureCoords);
    m_mesh.indices = std::move(source.m_mesh.indices);
    m_light = std::move(source.m_light);
    m_indexIndex = source.m_indexIndex;
    faces = source.faces;

    source.clearClientData();
}

const Mesh &ChunkMesh::getClientMesh() const
{
    return m_mesh;
}

const std::vector<float> &ChunkMesh::getCardinalLight() const
{
    return m_light;
}
