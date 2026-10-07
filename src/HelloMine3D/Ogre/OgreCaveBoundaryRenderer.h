#pragma once

#include <OgreVector2.h>
#include <OgreVector3.h>
#include <cstddef>
#include <memory>
#include <vector>

namespace Ogre
{
    class SceneManager;
}

struct WorldBoundaryMaskFace;

struct OgreCaveBoundaryRendererStats
{
    std::size_t gpuBytes = 0;
    std::size_t maxGpuBytes = 0;
    std::size_t liveFaces = 0;
    std::size_t deferredFaces = 0;
    std::size_t updatesThisSync = 0;
    std::size_t removalsThisSync = 0;
    std::size_t hiddenFacesThisSync = 0;
    std::size_t rejectedFacesThisSync = 0;
    std::size_t texturePatchBytesThisSync = 0;
    std::size_t vertexPatchBytesThisSync = 0;
};

// A fixed-size representation of unknown underground space at the current
// demand boundary. It owns no World data and never changes collision or demand.
// Every inactive slot is degenerate; a tile is uploaded before its quad becomes
// visible, so deferred replacements cannot display a previous owner's mask.
class OgreCaveBoundaryRenderer
{
  public:
    static constexpr const char* MaterialName = "HelloMine3D/CaveBoundary";
    static constexpr std::size_t MaxFaces = 2048;
    static constexpr std::size_t MaxUpdatesPerSync = 8;
    static constexpr std::size_t TileEdge = 16;
    static constexpr std::size_t AtlasWidth = 512;
    static constexpr std::size_t AtlasHeight = 1024;
    static constexpr std::size_t VertexStrideBytes = 20;
    static constexpr std::size_t AtlasBytes = AtlasWidth * AtlasHeight;
    static constexpr std::size_t VertexBytes = MaxFaces * 4 * VertexStrideBytes;
    static constexpr std::size_t IndexBytes = MaxFaces * 6 * sizeof(unsigned short);
    static constexpr std::size_t MaxGpuBytes = AtlasBytes + VertexBytes + IndexBytes;

    explicit OgreCaveBoundaryRenderer(Ogre::SceneManager& sceneManager);
    ~OgreCaveBoundaryRenderer();

    OgreCaveBoundaryRenderer(const OgreCaveBoundaryRenderer&) = delete;
    OgreCaveBoundaryRenderer& operator=(const OgreCaveBoundaryRenderer&) = delete;

    // Confirmation calls can invalidate old quads without consuming a second
    // atlas/activation budget in the same render frame.
    void sync(const std::vector<WorldBoundaryMaskFace>& faces,
              bool uploadNewMasks = true);
    // Copies the existing logical residency-centred visual range to this
    // renderer's owned material clone; no World query or allocation.
    void setViewRange(const Ogre::Vector2& range, const Ogre::Vector2& centre,
                      float strength, const Ogre::Vector3& authoredFog);
    void clear();
    const OgreCaveBoundaryRendererStats& stats() const noexcept;
    const OgreCaveBoundaryRendererStats& getStats() const noexcept { return stats(); }

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
