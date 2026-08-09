#include "ChunkSectionRenderable.h"

#include <OgreCamera.h>
#include <OgreHardwareBufferManager.h>
#include <OgreNode.h>
#include <OgreVertexIndexData.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../World/Chunk/ChunkMesh.h"
#include "../World/WorldConstants.h"

namespace
{
    struct TerrainVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
        float light;
    };

    static_assert(sizeof(TerrainVertex) == sizeof(float) * 6,
                  "Terrain vertices must be tightly packed.");

    std::vector<TerrainVertex>
    buildVertexStream(const ChunkMesh &mesh,
                      const glm::ivec3 &sectionLocation)
    {
        const auto &clientMesh = mesh.getClientMesh();
        const auto &positions = clientMesh.vertexPositions;
        const auto &textureCoordinates = clientMesh.textureCoords;
        const auto &cardinalLight = mesh.getCardinalLight();
        const std::size_t vertexCount = positions.size() / 3;

        std::vector<TerrainVertex> vertices;
        vertices.reserve(vertexCount);

        const float originX =
            static_cast<float>(sectionLocation.x * CHUNK_SIZE);
        const float originY =
            static_cast<float>(sectionLocation.y * CHUNK_SIZE);
        const float originZ =
            static_cast<float>(sectionLocation.z * CHUNK_SIZE);
        for (std::size_t index = 0; index < vertexCount; ++index)
        {
            vertices.push_back(
                {positions[index * 3] - originX,
                 positions[index * 3 + 1] - originY,
                 positions[index * 3 + 2] - originZ,
                 textureCoordinates[index * 2],
                 textureCoordinates[index * 2 + 1], cardinalLight[index]});
        }
        return vertices;
    }
}

ChunkSectionRenderable::ChunkSectionRenderable(
    const Ogre::String &name, const ChunkMesh &mesh,
    const glm::ivec3 &sectionLocation, const Ogre::String &materialName,
    std::uint8_t renderQueueGroup)
    : Ogre::SimpleRenderable(name)
{
    const ChunkMeshValidation validation =
        validateCpuMesh(mesh, sectionLocation);
    if (!validation.valid || validation.indexCount == 0)
    {
        throw std::runtime_error("Invalid chunk mesh: " +
                                 validation.message);
    }

    const std::vector<TerrainVertex> vertices =
        buildVertexStream(mesh, sectionLocation);
    const auto &indices = mesh.getClientMesh().indices;

    mRenderOp.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
    mRenderOp.useIndexes = true;
    mRenderOp.srcRenderable = this;
    mRenderOp.vertexData = OGRE_NEW Ogre::VertexData();
    mRenderOp.indexData = OGRE_NEW Ogre::IndexData();

    Ogre::VertexDeclaration *declaration =
        mRenderOp.vertexData->vertexDeclaration;
    declaration->addElement(0, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
    declaration->addElement(0, sizeof(float) * 3, Ogre::VET_FLOAT2,
                            Ogre::VES_TEXTURE_COORDINATES, 0);
    declaration->addElement(0, sizeof(float) * 5, Ogre::VET_FLOAT1,
                            Ogre::VES_TEXTURE_COORDINATES, 1);

    Ogre::HardwareVertexBufferSharedPtr vertexBuffer =
        Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
            sizeof(TerrainVertex), vertices.size(),
            Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
    vertexBuffer->writeData(0, sizeof(TerrainVertex) * vertices.size(),
                            vertices.data(), true);
    mRenderOp.vertexData->vertexBufferBinding->setBinding(0, vertexBuffer);
    mRenderOp.vertexData->vertexStart = 0;
    mRenderOp.vertexData->vertexCount = vertices.size();

    Ogre::HardwareIndexBufferSharedPtr indexBuffer =
        Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
            Ogre::HardwareIndexBuffer::IT_32BIT, indices.size(),
            Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
    indexBuffer->writeData(0, sizeof(std::uint32_t) * indices.size(),
                           indices.data(), true);
    mRenderOp.indexData->indexBuffer = indexBuffer;
    mRenderOp.indexData->indexStart = 0;
    mRenderOp.indexData->indexCount = indices.size();

    setMaterial(materialName);
    setRenderQueueGroup(renderQueueGroup);
    setBoundingBox(Ogre::AxisAlignedBox(
        Ogre::Vector3::ZERO,
        Ogre::Vector3(static_cast<Ogre::Real>(CHUNK_SIZE))));
    m_boundingRadius =
        Ogre::Math::Sqrt(static_cast<Ogre::Real>(CHUNK_SIZE * CHUNK_SIZE * 3));
}

ChunkSectionRenderable::~ChunkSectionRenderable()
{
    OGRE_DELETE mRenderOp.vertexData;
    OGRE_DELETE mRenderOp.indexData;
    mRenderOp.vertexData = nullptr;
    mRenderOp.indexData = nullptr;
}

ChunkMeshValidation ChunkSectionRenderable::validateCpuMesh(
    const ChunkMesh &mesh, const glm::ivec3 &sectionLocation)
{
    const auto &clientMesh = mesh.getClientMesh();
    const auto &positions = clientMesh.vertexPositions;
    const auto &textureCoordinates = clientMesh.textureCoords;
    const auto &indices = clientMesh.indices;
    const auto &cardinalLight = mesh.getCardinalLight();

    ChunkMeshValidation result;
    if (positions.size() % 3 != 0)
    {
        result.message = "position count is not divisible by three";
        return result;
    }

    result.vertexCount = positions.size() / 3;
    result.indexCount = indices.size();
    if (textureCoordinates.size() != result.vertexCount * 2)
    {
        result.message = "texture coordinate count does not match vertices";
        return result;
    }
    if (cardinalLight.size() != result.vertexCount)
    {
        result.message = "cardinal light count does not match vertices";
        return result;
    }
    if (indices.size() % 3 != 0)
    {
        result.message = "index count is not divisible by three";
        return result;
    }
    if (!indices.empty() &&
        *std::max_element(indices.begin(), indices.end()) >=
            result.vertexCount)
    {
        result.message = "an index references a missing vertex";
        return result;
    }

    const std::vector<TerrainVertex> vertices =
        buildVertexStream(mesh, sectionLocation);
    const float epsilon = 0.001f;
    for (const TerrainVertex &vertex : vertices)
    {
        if (vertex.x < -epsilon || vertex.y < -epsilon ||
            vertex.z < -epsilon || vertex.x > CHUNK_SIZE + epsilon ||
            vertex.y > CHUNK_SIZE + epsilon ||
            vertex.z > CHUNK_SIZE + epsilon)
        {
            result.message = "a vertex lies outside its section bounds";
            return result;
        }
        if (!std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
            !std::isfinite(vertex.light))
        {
            result.message = "a vertex contains a non-finite attribute";
            return result;
        }
    }

    result.valid = true;
    result.message = "ok";
    return result;
}

Ogre::Real ChunkSectionRenderable::getBoundingRadius() const
{
    return m_boundingRadius;
}

Ogre::Real ChunkSectionRenderable::getSquaredViewDepth(
    const Ogre::Camera *camera) const
{
    if (mParentNode == nullptr)
    {
        return 0.0f;
    }

    const Ogre::Vector3 center =
        mParentNode->_getFullTransform() * mBox.getCenter();
    return (camera->getDerivedPosition() - center).squaredLength();
}
