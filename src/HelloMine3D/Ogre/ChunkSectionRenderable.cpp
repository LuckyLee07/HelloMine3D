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

#include "../Diagnostics/TerrainBufferMetrics.h"
#include "../World/Chunk/ChunkMesh.h"
#include "../World/WorldConstants.h"

ChunkSectionRenderable::ChunkSectionRenderable(
    const Ogre::String &name, const ChunkMesh &mesh,
    const glm::ivec3 &sectionLocation, const Ogre::String &materialName,
    std::uint8_t renderQueueGroup)
    : ChunkSectionRenderable(name, std::vector<TerrainRenderBatchPart>{{sectionLocation, &mesh}},
                             sectionLocation, materialName, renderQueueGroup)
{
}

ChunkSectionRenderable::ChunkSectionRenderable(
    const Ogre::String &name, const std::vector<TerrainRenderBatchPart>& parts,
    const glm::ivec3 &sectionLocation, const Ogre::String &materialName,
    std::uint8_t renderQueueGroup)
    : Ogre::SimpleRenderable(name)
{
    const auto packed = packTerrainRenderBatch(parts, sectionLocation);
    if (packed.indices.empty()) throw std::runtime_error("Invalid empty terrain batch");
    const auto& vertices = packed.vertices;
    const auto& indices = packed.indices;
    static_assert(sizeof(TerrainRenderVertex) == TerrainBufferMetrics::VertexStrideBytes,
                  "Terrain vertices must be tightly packed");

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
    declaration->addElement(0, sizeof(float) * 5, Ogre::VET_FLOAT2,
                            Ogre::VES_TEXTURE_COORDINATES, 1);
    declaration->addElement(0, sizeof(float) * 7, Ogre::VET_FLOAT1,
                            Ogre::VES_TEXTURE_COORDINATES, 2);

    Ogre::HardwareVertexBufferSharedPtr vertexBuffer =
        Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
            TerrainBufferMetrics::VertexStrideBytes, vertices.size(),
            Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
    vertexBuffer->writeData(0,
                            TerrainBufferMetrics::VertexStrideBytes *
                                vertices.size(),
                            vertices.data(), true);
    mRenderOp.vertexData->vertexBufferBinding->setBinding(0, vertexBuffer);
    mRenderOp.vertexData->vertexStart = 0;
    mRenderOp.vertexData->vertexCount = vertices.size();

    Ogre::HardwareIndexBufferSharedPtr indexBuffer =
        Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
            Ogre::HardwareIndexBuffer::IT_32BIT, indices.size(),
            Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
    indexBuffer->writeData(
        0, TerrainBufferMetrics::IndexStrideBytes * indices.size(),
        indices.data(), true);
    mRenderOp.indexData->indexBuffer = indexBuffer;
    mRenderOp.indexData->indexStart = 0;
    mRenderOp.indexData->indexCount = indices.size();

    setMaterial(materialName);
    setRenderQueueGroup(renderQueueGroup);
    setBoundingBox(Ogre::AxisAlignedBox(
        Ogre::Vector3::ZERO,
        Ogre::Vector3(CHUNK_SIZE, CHUNK_SIZE * packed.heightSections, CHUNK_SIZE)));
    m_boundingRadius =
        Ogre::Math::Sqrt(static_cast<Ogre::Real>(CHUNK_SIZE * CHUNK_SIZE *
            (2 + packed.heightSections * packed.heightSections)));
}

ChunkSectionRenderable::~ChunkSectionRenderable()
{
    OGRE_DELETE mRenderOp.vertexData;
    OGRE_DELETE mRenderOp.indexData;
    mRenderOp.vertexData = nullptr;
    mRenderOp.indexData = nullptr;
}

std::size_t ChunkSectionRenderable::vertexCount() const noexcept
{
    return mRenderOp.vertexData == nullptr
               ? 0
               : mRenderOp.vertexData->vertexCount;
}

std::size_t ChunkSectionRenderable::indexCount() const noexcept
{
    return mRenderOp.indexData == nullptr
               ? 0
               : mRenderOp.indexData->indexCount;
}

ChunkMeshValidation ChunkSectionRenderable::validateCpuMesh(
    const ChunkMesh &mesh, const glm::ivec3 &sectionLocation)
{
    ChunkMeshValidation result;
    result.vertexCount = mesh.getClientMesh().vertexPositions.size() / 3;
    result.indexCount = mesh.getClientMesh().indices.size();
    try {
        validateTerrainRenderPart({sectionLocation, &mesh});
        result.valid = true;
        result.message = "ok";
    }
    catch (const std::exception& error) { result.message = error.what(); }
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
