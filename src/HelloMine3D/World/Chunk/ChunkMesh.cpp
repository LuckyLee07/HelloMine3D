#include "ChunkMesh.h"

#include "../WorldConstants.h"

#include <algorithm>
#include <cstring>
#include <iostream>

void ChunkMesh::addFace(const std::array<float, 12> &blockFace,
                        const std::array<float, 8> &textureCoords,
                        const glm::ivec3 &chunkPosition,
                        const glm::ivec3 &blockPosition, float light,
                        float textureRepeatWidth,
                        float textureRepeatHeight)
{
    addFace(blockFace, textureCoords, chunkPosition, blockPosition,
            {light, light, light, light}, false, textureRepeatWidth,
            textureRepeatHeight);
}

void ChunkMesh::addFace(const std::array<float, 12> &blockFace,
                        const std::array<float, 8> &textureCoords,
                        const glm::ivec3 &chunkPosition,
                        const glm::ivec3 &blockPosition,
                        const std::array<float, 4> &vertexLight,
                        bool flipDiagonal, float textureRepeatWidth,
                        float textureRepeatHeight)
{
    const std::array<float, 8> textureRepeatCoords = {
        textureRepeatWidth, textureRepeatHeight, 0.f,
        textureRepeatHeight, 0.f, 0.f, textureRepeatWidth, 0.f};
    addFace(blockFace, textureCoords, chunkPosition, blockPosition,
            vertexLight, flipDiagonal, textureRepeatCoords);
}

void ChunkMesh::addFace(
    const std::array<float, 12> &blockFace,
    const std::array<float, 8> &textureCoords,
    const glm::ivec3 &chunkPosition, const glm::ivec3 &blockPosition,
    const std::array<float, 4> &vertexLight, bool flipDiagonal,
    const std::array<float, 8> &textureRepeatCoords)
{
    addFaceInternal(blockFace, textureCoords, chunkPosition, blockPosition,
                    vertexLight, flipDiagonal, textureRepeatCoords, false);
}

void ChunkMesh::addSharedFace(
    const std::array<float, 12> &blockFace,
    const std::array<float, 8> &textureCoords,
    const glm::ivec3 &chunkPosition, const glm::ivec3 &blockPosition,
    const std::array<float, 4> &vertexLight, bool flipDiagonal,
    const std::array<float, 8> &textureRepeatCoords)
{
    addFaceInternal(blockFace, textureCoords, chunkPosition, blockPosition,
                    vertexLight, flipDiagonal, textureRepeatCoords, true);
}

void ChunkMesh::beginSharedFaces()
{
    constexpr int edge = CHUNK_SIZE + 1;
    constexpr int gridSize = edge * edge * edge;
    if (m_sharedVertexHeads.size() != gridSize) {
        m_sharedVertexHeads.assign(gridSize, -1);
        m_sharedVertexGenerations.assign(gridSize, 0);
    }
    ++m_sharedVertexGeneration;
    if (m_sharedVertexGeneration == 0) {
        std::fill(m_sharedVertexGenerations.begin(),
                  m_sharedVertexGenerations.end(), 0);
        m_sharedVertexGeneration = 1;
    }
    m_sharedVertexEntries.clear();
    if (m_sharedVertexEntries.capacity() < CHUNK_VOLUME) {
        m_sharedVertexEntries.reserve(CHUNK_VOLUME);
    }
}

void ChunkMesh::addFaceInternal(
    const std::array<float, 12> &blockFace,
    const std::array<float, 8> &textureCoords,
    const glm::ivec3 &chunkPosition, const glm::ivec3 &blockPosition,
    const std::array<float, 4> &vertexLight, bool flipDiagonal,
    const std::array<float, 8> &textureRepeatCoords, bool shareVertices)
{
    faces++;
    auto &verticies = m_mesh.vertexPositions;
    auto &texCoords = m_mesh.textureCoords;
    auto &indices = m_mesh.indices;

    if (!shareVertices) {
        texCoords.insert(texCoords.end(), textureCoords.begin(),
                         textureCoords.end());
        m_mesh.textureRepeatCoords.insert(
            m_mesh.textureRepeatCoords.end(), textureRepeatCoords.begin(),
            textureRepeatCoords.end());
        for (int vertex = 0, index = 0; vertex < 4; ++vertex) {
            verticies.push_back(
                blockFace[index++] + chunkPosition.x * CHUNK_SIZE +
                blockPosition.x);
            verticies.push_back(
                blockFace[index++] + chunkPosition.y * CHUNK_SIZE +
                blockPosition.y);
            verticies.push_back(
                blockFace[index++] + chunkPosition.z * CHUNK_SIZE +
                blockPosition.z);
            m_light.push_back(vertexLight[vertex]);
        }
        if (flipDiagonal) {
            indices.insert(indices.end(),
                           {m_indexIndex, m_indexIndex + 1,
                            m_indexIndex + 3, m_indexIndex + 1,
                            m_indexIndex + 2, m_indexIndex + 3});
        }
        else {
            indices.insert(indices.end(),
                           {m_indexIndex, m_indexIndex + 1,
                            m_indexIndex + 2, m_indexIndex + 2,
                            m_indexIndex + 3, m_indexIndex});
        }
        m_indexIndex += 4;
        return;
    }

    std::array<std::uint32_t, 4> faceIndices{};

    /// Vertex: The current vertex in the "blockFace" vector, 4 vertex in total
    /// hence "< 4" Index: X, Y, Z
    for (int i = 0, index = 0; i < 4; ++i) {
        const int localX = static_cast<int>(blockFace[index]) +
                           blockPosition.x;
        const int localY = static_cast<int>(blockFace[index + 1]) +
                           blockPosition.y;
        const int localZ = static_cast<int>(blockFace[index + 2]) +
                           blockPosition.z;
        const std::array<float, 8> values = {
            blockFace[index++] + chunkPosition.x * CHUNK_SIZE +
                blockPosition.x,
            blockFace[index++] + chunkPosition.y * CHUNK_SIZE +
                blockPosition.y,
            blockFace[index++] + chunkPosition.z * CHUNK_SIZE +
                blockPosition.z,
            textureCoords[i * 2],
            textureCoords[i * 2 + 1],
            textureRepeatCoords[i * 2],
            textureRepeatCoords[i * 2 + 1],
            vertexLight[i],
        };

        std::array<std::uint32_t, 5> attributes{};
        std::memcpy(attributes.data(), values.data() + 3,
                    sizeof(attributes));
        std::int32_t positionHead = -1;
        std::size_t positionIndex = 0;
        if (m_sharedVertexHeads.empty()) {
            beginSharedFaces();
        }
        constexpr int edge = CHUNK_SIZE + 1;
        if (localX >= 0 && localX < edge && localY >= 0 &&
            localY < edge && localZ >= 0 && localZ < edge) {
            positionIndex = static_cast<std::size_t>(
                localX + edge * (localZ + edge * localY));
            if (m_sharedVertexGenerations[positionIndex] !=
                m_sharedVertexGeneration) {
                m_sharedVertexGenerations[positionIndex] =
                    m_sharedVertexGeneration;
                m_sharedVertexHeads[positionIndex] = -1;
            }
            positionHead = m_sharedVertexHeads[positionIndex];
            for (std::int32_t entryIndex = positionHead; entryIndex >= 0;
                 entryIndex = m_sharedVertexEntries[entryIndex].next) {
                const SharedVertexEntry &entry =
                    m_sharedVertexEntries[entryIndex];
                if (entry.attributes == attributes) {
                    faceIndices[i] = entry.vertexIndex;
                    positionHead = -2;
                    break;
                }
            }
            if (positionHead == -2) {
                continue;
            }
        }

        const std::uint32_t vertexIndex =
            static_cast<std::uint32_t>(m_light.size());
        faceIndices[i] = vertexIndex;
        if (!m_sharedVertexHeads.empty() && localX >= 0 &&
            localX <= CHUNK_SIZE && localY >= 0 &&
            localY <= CHUNK_SIZE && localZ >= 0 &&
            localZ <= CHUNK_SIZE) {
            SharedVertexEntry entry;
            entry.attributes = attributes;
            entry.vertexIndex = vertexIndex;
            entry.next = m_sharedVertexHeads[positionIndex];
            m_sharedVertexHeads[positionIndex] =
                static_cast<std::int32_t>(m_sharedVertexEntries.size());
            m_sharedVertexEntries.push_back(entry);
        }
        verticies.insert(verticies.end(), values.begin(), values.begin() + 3);
        texCoords.insert(texCoords.end(), values.begin() + 3,
                         values.begin() + 5);
        m_mesh.textureRepeatCoords.insert(
            m_mesh.textureRepeatCoords.end(), values.begin() + 5,
            values.begin() + 7);
        m_light.push_back(values[7]);
    }

    if (flipDiagonal) {
        indices.insert(indices.end(),
                       {faceIndices[0], faceIndices[1], faceIndices[3],
                        faceIndices[1], faceIndices[2], faceIndices[3]});
    }
    else {
        indices.insert(indices.end(),
                       {faceIndices[0], faceIndices[1], faceIndices[2],
                        faceIndices[2], faceIndices[3], faceIndices[0]});
    }
    m_indexIndex = static_cast<std::uint32_t>(m_light.size());
}

void ChunkMesh::clearClientData()
{
    m_mesh.vertexPositions.clear();
    m_mesh.textureCoords.clear();
    m_mesh.textureRepeatCoords.clear();
    m_mesh.indices.clear();
    m_light.clear();
    m_sharedVertexHeads.clear();
    m_sharedVertexGenerations.clear();
    m_sharedVertexEntries.clear();
    m_sharedVertexGeneration = 0;

    m_indexIndex = 0;
    faces = 0;
}

void ChunkMesh::adoptClientData(ChunkMesh &source)
{
    m_mesh.vertexPositions = std::move(source.m_mesh.vertexPositions);
    m_mesh.textureCoords = std::move(source.m_mesh.textureCoords);
    m_mesh.textureRepeatCoords =
        std::move(source.m_mesh.textureRepeatCoords);
    m_mesh.indices = std::move(source.m_mesh.indices);
    m_light = std::move(source.m_light);
    m_sharedVertexHeads.clear();
    m_sharedVertexGenerations.clear();
    m_sharedVertexEntries.clear();
    m_sharedVertexGeneration = 0;
    m_indexIndex = source.m_indexIndex;
    faces = source.faces;

    source.clearClientData();
}

const Mesh &ChunkMesh::getClientMesh() const
{
    return m_mesh;
}

const std::vector<float> &ChunkMesh::getLight() const
{
    return m_light;
}
