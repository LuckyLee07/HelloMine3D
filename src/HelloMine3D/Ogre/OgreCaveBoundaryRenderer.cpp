#include "OgreCaveBoundaryRenderer.h"

#include <OgreCamera.h>
#include <OgreHardwareBufferManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreMaterialManager.h>
#include <OgrePass.h>
#include <OgreRenderQueue.h>
#include <OgreResourceGroupManager.h>
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreSimpleRenderable.h>
#include <OgreTechnique.h>
#include <OgreTextureManager.h>
#include <OgreTextureUnitState.h>
#include <OgreVertexIndexData.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../World/Chunk/ChunkRuntime.h"
#include "../World/WorldConstants.h"

namespace
{
    using Renderer = OgreCaveBoundaryRenderer;
    constexpr std::size_t VerticesPerFace = 4;
    constexpr std::size_t IndicesPerFace = 6;
    constexpr std::size_t TileBytes = Renderer::TileEdge * Renderer::TileEdge;
    constexpr std::size_t AtlasColumns = Renderer::AtlasWidth / Renderer::TileEdge;

    struct BoundaryVertex
    {
        float x, y, z, u, v;
    };

    static_assert(sizeof(BoundaryVertex) == Renderer::VertexStrideBytes,
                  "Boundary vertices contain only position and atlas UV");
    static_assert(sizeof(std::uint16_t) == sizeof(unsigned short),
                  "Boundary indices are 16-bit");
    static_assert(Renderer::MaxFaces * VerticesPerFace <=
                      std::numeric_limits<std::uint16_t>::max(),
                  "Every fixed boundary slot must fit 16-bit indices");
    static_assert(Renderer::MaxFaces * TileBytes == Renderer::AtlasBytes,
                  "The atlas has exactly one tile per slot");
    static_assert(Renderer::MaxGpuBytes == 696 * 1024,
                  "Boundary GPU allocation has a fixed 696 KiB ceiling");

    struct FaceKey
    {
        glm::ivec3 location;
        std::uint8_t face;

        bool operator==(const FaceKey& other) const noexcept
        {
            return location.x == other.location.x &&
                location.y == other.location.y && location.z == other.location.z &&
                face == other.face;
        }
    };

    struct FaceKeyHash
    {
        std::size_t operator()(const FaceKey& key) const noexcept
        {
            std::size_t hash = std::hash<int>{}(key.location.x);
            for (const int coordinate : {key.location.y, key.location.z,
                                         static_cast<int>(key.face)})
                hash ^= std::hash<int>{}(coordinate) + 0x9e3779b9u +
                    (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    FaceKey faceKey(const WorldBoundaryMaskFace& face)
    {
        return {face.location, face.face};
    }

    bool sameMask(const WorldBoundaryMaskFace& left,
                  const WorldBoundaryMaskFace& right)
    {
        return faceKey(left) == faceKey(right) &&
            left.incarnation == right.incarnation &&
            left.blockRevision == right.blockRevision && left.rows == right.rows;
    }

    bool hasMask(const WorldBoundaryMaskFace& face)
    {
        return std::any_of(face.rows.begin(), face.rows.end(),
                           [](std::uint16_t row) { return row != 0; });
    }

    class BoundaryRenderable final : public Ogre::SimpleRenderable
    {
      public:
        BoundaryRenderable(const Ogre::String& name, const Ogre::String& material)
            : Ogre::SimpleRenderable(name)
        {
            auto vertices = std::make_unique<Ogre::VertexData>();
            auto indices = std::make_unique<Ogre::IndexData>();
            vertices->vertexDeclaration->addElement(
                0, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
            vertices->vertexDeclaration->addElement(
                0, sizeof(float) * 3, Ogre::VET_FLOAT2,
                Ogre::VES_TEXTURE_COORDINATES, 0);
            m_vertices = Ogre::HardwareBufferManager::getSingleton()
                .createVertexBuffer(sizeof(BoundaryVertex),
                    Renderer::MaxFaces * VerticesPerFace,
                    Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY);
            // The sole complete VBO write is allocation-time initialization.
            // Atlas contents need no initialization: all slots start degenerate.
            const std::vector<BoundaryVertex> empty(
                Renderer::MaxFaces * VerticesPerFace, BoundaryVertex{});
            m_vertices->writeData(0, Renderer::VertexBytes, empty.data(), true);
            vertices->vertexBufferBinding->setBinding(0, m_vertices);
            vertices->vertexStart = 0;
            vertices->vertexCount = empty.size();

            std::vector<std::uint16_t> fixedIndices(Renderer::MaxFaces * IndicesPerFace);
            for (std::size_t slot = 0; slot < Renderer::MaxFaces; ++slot)
            {
                const auto first = static_cast<std::uint16_t>(slot * VerticesPerFace);
                const std::array<std::uint16_t, IndicesPerFace> quad{
                    first, static_cast<std::uint16_t>(first + 1),
                    static_cast<std::uint16_t>(first + 2), first,
                    static_cast<std::uint16_t>(first + 2),
                    static_cast<std::uint16_t>(first + 3)};
                std::copy(quad.begin(), quad.end(),
                          fixedIndices.begin() + slot * IndicesPerFace);
            }
            indices->indexBuffer = Ogre::HardwareBufferManager::getSingleton()
                .createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT,
                    fixedIndices.size(), Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
            indices->indexBuffer->writeData(
                0, Renderer::IndexBytes, fixedIndices.data(), true);
            indices->indexStart = 0;
            indices->indexCount = fixedIndices.size();

            setMaterial(material);
            setRenderQueueGroup(Ogre::RENDER_QUEUE_SKIES_EARLY + 1);
            setCastShadows(false);
            // One bounded draw includes all fixed slots. An infinite box avoids
            // rebuilding aggregate bounds whenever a slot is patched or removed.
            setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
            setVisible(false);
            mRenderOp.operationType = Ogre::RenderOperation::OT_TRIANGLE_LIST;
            mRenderOp.useIndexes = true;
            mRenderOp.srcRenderable = this;
            mRenderOp.vertexData = vertices.release();
            mRenderOp.indexData = indices.release();
        }

        ~BoundaryRenderable() override
        {
            OGRE_DELETE mRenderOp.vertexData;
            OGRE_DELETE mRenderOp.indexData;
            mRenderOp.vertexData = nullptr;
            mRenderOp.indexData = nullptr;
        }

        void patch(std::size_t slot,
                   const std::array<BoundaryVertex, VerticesPerFace>& quad)
        {
            // discardWholeBuffer=false retains every untouched slot.
            m_vertices->writeData(slot * sizeof(quad), sizeof(quad), quad.data(), false);
        }

        Ogre::Real getBoundingRadius() const override
        {
            return std::numeric_limits<Ogre::Real>::max();
        }

        Ogre::Real getSquaredViewDepth(const Ogre::Camera*) const override { return 0.f; }

      private:
        Ogre::HardwareVertexBufferSharedPtr m_vertices;
    };

    std::array<BoundaryVertex, VerticesPerFace> faceQuad(
        const WorldBoundaryMaskFace& face, std::size_t slot)
    {
        const float x = static_cast<float>(static_cast<double>(face.location.x) * CHUNK_SIZE);
        const float y = static_cast<float>(static_cast<double>(face.location.y) * CHUNK_SIZE);
        const float z = static_cast<float>(static_cast<double>(face.location.z) * CHUNK_SIZE);
        const float u0 = static_cast<float>((slot % AtlasColumns) * Renderer::TileEdge) /
            Renderer::AtlasWidth;
        const float v0 = static_cast<float>((slot / AtlasColumns) * Renderer::TileEdge) /
            Renderer::AtlasHeight;
        const float u1 = u0 + static_cast<float>(Renderer::TileEdge) / Renderer::AtlasWidth;
        const float v1 = v0 + static_cast<float>(Renderer::TileEdge) / Renderer::AtlasHeight;
        if (face.face < 2)
        {
            const float plane = x + (face.face == 1 ? CHUNK_SIZE : 0);
            return {{{plane, y, z, u0, v0}, {plane, y, z + CHUNK_SIZE, u1, v0},
                     {plane, y + CHUNK_SIZE, z + CHUNK_SIZE, u1, v1},
                     {plane, y + CHUNK_SIZE, z, u0, v1}}};
        }
        const float plane = z + (face.face == 3 ? CHUNK_SIZE : 0);
        return {{{x, y, plane, u0, v0}, {x + CHUNK_SIZE, y, plane, u1, v0},
                 {x + CHUNK_SIZE, y + CHUNK_SIZE, plane, u1, v1},
                 {x, y + CHUNK_SIZE, plane, u0, v1}}};
    }
}

class OgreCaveBoundaryRenderer::Impl
{
  public:
    explicit Impl(Ogre::SceneManager& scene) : m_scene(&scene)
    {
        static std::atomic<std::uint64_t> nextId{0};
        const std::string name = "HelloMine3D/CaveBoundaryInstance" +
            std::to_string(++nextId);
        m_atlasName = name + "/Mask";
        m_materialName = name + "/Material";
        m_slotsByKey.reserve(MaxFaces);
        m_freeSlots.reserve(MaxFaces);
        try
        {
            m_atlas = Ogre::TextureManager::getSingleton().createManual(
                m_atlasName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                Ogre::TEX_TYPE_2D, AtlasWidth, AtlasHeight, 0, Ogre::PF_L8,
                Ogre::TU_DYNAMIC_WRITE_ONLY);
            if (m_atlas.isNull() || m_atlas->getFormat() != Ogre::PF_L8 ||
                m_atlas->getNumMipmaps() != 0)
                throw std::runtime_error("Cave boundary atlas requires unmipped L8/R8 storage");
            const auto source = Ogre::MaterialManager::getSingleton().getByName(MaterialName);
            if (source.isNull())
                throw std::runtime_error("Missing cave boundary material");
            m_material = source->clone(m_materialName);
            if (m_material->getNumTechniques() != 1 ||
                m_material->getTechnique(0)->getNumPasses() != 1)
                throw std::runtime_error("Cave boundary material requires one technique and one pass");
            auto* pass = m_material->getTechnique(0)->getPass(0);
            if (m_material->getReceiveShadows() || pass->getLightingEnabled() ||
                !pass->getDepthCheckEnabled() || pass->getDepthWriteEnabled() ||
                pass->getCullingMode() != Ogre::CULL_NONE)
                throw std::runtime_error("Cave boundary material requires unlit, uncullable, depth-tested background without depth writes or received shadows");
            if (pass->getNumTextureUnitStates() != 1)
                throw std::runtime_error("Cave boundary material requires one mask texture unit");
            const auto parameters = pass->getFragmentProgramParameters();
            const std::array<const char*, 4> rangeNames{{
                "viewRange", "viewRangeCentre", "viewRangeStrength", "fogColour"}};
            const std::array<Ogre::GpuConstantType, 4> rangeTypes{{
                Ogre::GCT_FLOAT2, Ogre::GCT_FLOAT2, Ogre::GCT_FLOAT1, Ogre::GCT_FLOAT3}};
            const std::array<std::size_t, 4> rangeSizes{{2, 2, 1, 3}};
            unsigned declared = 0;
            for (std::size_t i = 0; i < rangeNames.size(); ++i)
            {
                const auto* definition = parameters->_findNamedConstantDefinition(rangeNames[i], false);
                if (!definition) continue;
                ++declared;
                if (definition->constType != rangeTypes[i] || definition->arraySize != 1 ||
                    definition->elementSize != rangeSizes[i])
                    throw std::runtime_error(std::string("Invalid cave boundary view-range type/size for ") + rangeNames[i]);
            }
            if (declared != 0 && declared != rangeNames.size())
                throw std::runtime_error("Partial cave boundary view-range interface");
            m_rangeInterface = declared == rangeNames.size();
            auto* unit = pass->getTextureUnitState("caveBoundaryMask");
            if (unit == nullptr)
                throw std::runtime_error("Missing caveBoundaryMask texture unit");
            unit->setTextureName(m_atlasName, Ogre::TEX_TYPE_2D);
            unit->setTextureFiltering(Ogre::TFO_NONE);
            unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
            m_material->setReceiveShadows(false);
            pass->setDepthCheckEnabled(true);
            pass->setDepthWriteEnabled(false);
            pass->setCullingMode(Ogre::CULL_NONE);
            pass->setLightingEnabled(false);
            m_renderable = std::make_unique<BoundaryRenderable>(name, m_materialName);
            m_node = scene.getRootSceneNode()->createChildSceneNode(name + "/Node");
            m_node->attachObject(m_renderable.get());
            m_stats.gpuBytes = MaxGpuBytes;
            m_stats.maxGpuBytes = MaxGpuBytes;
            resetSlots();
        }
        catch (...)
        {
            releaseResources();
            throw;
        }
    }

    ~Impl() { releaseResources(); }

    void sync(const std::vector<WorldBoundaryMaskFace>& faces,
              bool uploadNewMasks)
    {
        resetFrameStats();
        using Desired = std::unordered_map<FaceKey, const WorldBoundaryMaskFace*, FaceKeyHash>;
        Desired desired;
        desired.reserve(std::min(faces.size(), MaxFaces));
        for (const auto& face : faces)
        {
            if (face.face > 3)
            {
                ++m_stats.rejectedFacesThisSync;
                continue;
            }
            if (!hasMask(face)) continue;
            const auto found = desired.find(faceKey(face));
            if (found != desired.end())
            {
                // Conflicting duplicate identities are malformed, rather than
                // a choice of which stale owner should remain on screen.
                if (!sameMask(*found->second, face))
                    throw std::runtime_error("Conflicting cave boundary face identities");
                continue;
            }
            if (desired.size() == MaxFaces)
            {
                ++m_stats.rejectedFacesThisSync;
                continue;
            }
            desired.emplace(faceKey(face), &face);
        }

        for (std::size_t index = 0; index < MaxFaces; ++index)
        {
            auto& slot = m_slots[index];
            if (!slot.occupied) continue;
            const auto wanted = desired.find(faceKey(slot.mask));
            if (wanted == desired.end())
            {
                hide(index);
                m_slotsByKey.erase(faceKey(slot.mask));
                slot = Slot{};
                m_freeSlots.push_back(index);
                ++m_stats.removalsThisSync;
            }
            else if (!sameMask(slot.mask, *wanted->second))
            {
                hide(index);
                slot.mask = *wanted->second;
            }
        }
        // Assign in caller order for stable selection when the bounded list is
        // first published; upload scheduling is round-robin to avoid starvation.
        for (const auto& face : faces)
        {
            const auto wanted = desired.find(faceKey(face));
            if (wanted == desired.end() || m_slotsByKey.find(faceKey(face)) != m_slotsByKey.end())
                continue;
            const std::size_t index = m_freeSlots.back();
            m_freeSlots.pop_back();
            auto& slot = m_slots[index];
            slot.occupied = true;
            slot.mask = *wanted->second;
            m_slotsByKey.emplace(faceKey(face), index);
        }

        for (std::size_t scanned = 0;
             uploadNewMasks && scanned < MaxFaces &&
                 m_stats.updatesThisSync < MaxUpdatesPerSync; ++scanned)
        {
            const std::size_t index = m_nextUpload;
            m_nextUpload = (m_nextUpload + 1) % MaxFaces;
            auto& slot = m_slots[index];
            if (!slot.occupied || slot.visible) continue;
            std::array<std::uint8_t, TileBytes> pixels{};
            for (std::size_t y = 0; y < TileEdge; ++y)
                for (std::size_t u = 0; u < TileEdge; ++u)
                    pixels[y * TileEdge + u] =
                        (slot.mask.rows[y] & (std::uint16_t{1} << u)) != 0 ? 255 : 0;
            const std::size_t left = (index % AtlasColumns) * TileEdge;
            const std::size_t top = (index / AtlasColumns) * TileEdge;
            const Ogre::PixelBox source(TileEdge, TileEdge, 1, Ogre::PF_L8, pixels.data());
            m_atlas->getBuffer(0, 0)->blitFromMemory(source,
                Ogre::Image::Box(static_cast<Ogre::uint32>(left),
                                 static_cast<Ogre::uint32>(top),
                                 static_cast<Ogre::uint32>(left + TileEdge),
                                 static_cast<Ogre::uint32>(top + TileEdge)));
            m_stats.texturePatchBytesThisSync += TileBytes;
            // No stale tile can be drawn: geometry becomes non-degenerate only
            // after this slot's complete 256-byte patch has succeeded.
            m_renderable->patch(index, faceQuad(slot.mask, index));
            m_stats.vertexPatchBytesThisSync += VerticesPerFace * VertexStrideBytes;
            slot.visible = true;
            ++m_stats.updatesThisSync;
        }
        refreshLiveStats();
    }

    void setViewRange(const Ogre::Vector2& range, const Ogre::Vector2& centre,
                      float strength, const Ogre::Vector3& authoredFog)
    {
        // A coherent complete old cave shader retains its existing behaviour.
        if (!m_rangeInterface) return;
        for (const float value : {range.x, range.y, centre.x, centre.y,
                                  strength, authoredFog.x, authoredFog.y, authoredFog.z})
            if (!std::isfinite(value))
                throw std::runtime_error("Non-finite cave boundary view-range parameter");
        auto parameters = m_material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
        parameters->setNamedConstant("viewRange", range);
        parameters->setNamedConstant("viewRangeCentre", centre);
        parameters->setNamedConstant("viewRangeStrength", strength);
        parameters->setNamedConstant("fogColour", authoredFog);
    }

    void clear()
    {
        resetFrameStats();
        for (std::size_t index = 0; index < MaxFaces; ++index)
        {
            if (m_slots[index].occupied) ++m_stats.removalsThisSync;
            hide(index);
        }
        resetSlots();
        refreshLiveStats();
    }

    const OgreCaveBoundaryRendererStats& stats() const noexcept { return m_stats; }

  private:
    struct Slot
    {
        WorldBoundaryMaskFace mask{};
        bool occupied = false;
        bool visible = false;
    };

    void hide(std::size_t index)
    {
        auto& slot = m_slots[index];
        if (!slot.visible) return;
        m_renderable->patch(index, {});
        slot.visible = false;
        ++m_stats.hiddenFacesThisSync;
        m_stats.vertexPatchBytesThisSync += VerticesPerFace * VertexStrideBytes;
    }

    void refreshLiveStats()
    {
        for (const auto& slot : m_slots)
        {
            if (slot.visible) ++m_stats.liveFaces;
            else if (slot.occupied) ++m_stats.deferredFaces;
        }
        m_renderable->setVisible(m_stats.liveFaces != 0);
    }

    void resetFrameStats()
    {
        m_stats = {};
        m_stats.gpuBytes = MaxGpuBytes;
        m_stats.maxGpuBytes = MaxGpuBytes;
    }

    void resetSlots()
    {
        m_slotsByKey.clear();
        m_freeSlots.clear();
        for (std::size_t index = MaxFaces; index > 0; --index)
        {
            m_slots[index - 1] = Slot{};
            m_freeSlots.push_back(index - 1);
        }
        m_nextUpload = 0;
    }

    void releaseResources()
    {
        if (m_node != nullptr)
        {
            m_node->detachAllObjects();
            m_scene->destroySceneNode(m_node);
            m_node = nullptr;
        }
        m_renderable.reset();
        if (!m_material.isNull())
        {
            Ogre::MaterialManager::getSingleton().remove(m_materialName);
            m_material.setNull();
        }
        if (!m_atlas.isNull())
        {
            Ogre::TextureManager::getSingleton().remove(m_atlasName);
            m_atlas.setNull();
        }
    }

    Ogre::SceneManager* m_scene;
    Ogre::SceneNode* m_node = nullptr;
    Ogre::TexturePtr m_atlas;
    Ogre::MaterialPtr m_material;
    bool m_rangeInterface = false;
    std::unique_ptr<BoundaryRenderable> m_renderable;
    std::string m_atlasName;
    std::string m_materialName;
    std::array<Slot, MaxFaces> m_slots{};
    std::unordered_map<FaceKey, std::size_t, FaceKeyHash> m_slotsByKey;
    std::vector<std::size_t> m_freeSlots;
    std::size_t m_nextUpload = 0;
    OgreCaveBoundaryRendererStats m_stats;
};

OgreCaveBoundaryRenderer::OgreCaveBoundaryRenderer(Ogre::SceneManager& sceneManager)
    : m_impl(std::make_unique<Impl>(sceneManager))
{
}

OgreCaveBoundaryRenderer::~OgreCaveBoundaryRenderer() = default;

void OgreCaveBoundaryRenderer::sync(const std::vector<WorldBoundaryMaskFace>& faces,
                                    bool uploadNewMasks)
{
    m_impl->sync(faces, uploadNewMasks);
}

void OgreCaveBoundaryRenderer::setViewRange(const Ogre::Vector2& range,
    const Ogre::Vector2& centre, float strength, const Ogre::Vector3& authoredFog)
{
    m_impl->setViewRange(range, centre, strength, authoredFog);
}

void OgreCaveBoundaryRenderer::clear() { m_impl->clear(); }

const OgreCaveBoundaryRendererStats& OgreCaveBoundaryRenderer::stats() const noexcept
{
    return m_impl->stats();
}
