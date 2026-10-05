#pragma once

#include <OgreSimpleRenderable.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "../Maths/glm.h"
#include "../Presentation/TerrainRenderBatch.h"

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
                           const glm::ivec3 &sectionLocation,
                           const Ogre::String &materialName,
                           std::uint8_t renderQueueGroup);
    ChunkSectionRenderable(const Ogre::String &name,
                           const std::vector<TerrainRenderBatchPart>& parts,
                           const glm::ivec3 &batchOrigin,
                           const Ogre::String &materialName,
                           std::uint8_t renderQueueGroup);
    ~ChunkSectionRenderable() override;

    static ChunkMeshValidation
    validateCpuMesh(const ChunkMesh &mesh,
                    const glm::ivec3 &sectionLocation);

    // Optional diagnostic attachment; Root owns the observer. Keep it alive until
    // matching postRender returns, and detach before destroying the observer.
    class NativeDrawObserver {
      public:
        virtual ~NativeDrawObserver() = default;
        virtual void beforeNativeDraw(ChunkSectionRenderable &, Ogre::SceneManager *,
                                      Ogre::RenderSystem *) = 0;
        virtual void afterNativeDraw(ChunkSectionRenderable &, Ogre::SceneManager *,
                                     Ogre::RenderSystem *) = 0;
    };
    void setNativeDrawObserver(NativeDrawObserver *observer) noexcept;
    bool preRender(Ogre::SceneManager *, Ogre::RenderSystem *) override;
    void postRender(Ogre::SceneManager *, Ogre::RenderSystem *) override;

    std::size_t vertexCount() const noexcept;
    std::size_t indexCount() const noexcept;

    Ogre::Real getBoundingRadius() const override;
    Ogre::Real
    getSquaredViewDepth(const Ogre::Camera *camera) const override;

  private:
    Ogre::Real m_boundingRadius = 0.0f;
    NativeDrawObserver *m_nativeDrawObserver = nullptr;
    NativeDrawObserver *m_activeNativeDrawObserver = nullptr;
};
