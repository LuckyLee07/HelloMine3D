#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ogre { class Renderable; class Pass; class Camera; }
struct ImDrawList;
struct ImDrawCmd;
struct ImDrawData;

// Explicit, render-thread-only diagnostic observer. Construct after the real
// client has initialized its resources/context; absence is the normal mode.
// It consumes existing objects and never creates a context or world state.
class MaterialIdentityCapture {
public:
    using UiCallback = void (*)(const ImDrawList*, const ImDrawCmd*);
    struct Facts {
        std::string consumer;
        std::string materialName;
        int materialId = -1, blockId = -1, slot = -1, amount = 0;
        int face = -1, tileX = -1, tileY = -1;
        std::uint64_t actorId = 0, revision = 0;
        int mapCell = -1, height = 0;
        bool top = false, known = false;
        int worldX = 0, worldY = 0, worldZ = 0;
    };
    struct UiRange {
        const ImDrawList* list = nullptr;
        std::size_t vertexBegin = 0, indexBegin = 0, commandBegin = 0;
    };
    struct MapSample {
        std::size_t cell = 0;
        int worldX = 0, worldZ = 0, height = 0, blockId = -1;
        bool known = false, sampleAvailable = false;
    };
    struct MapBatch {
        int centreX = 0, centreZ = 0, countX = 0, countZ = 0, step = 0;
        std::uint64_t revision = 0;
        std::size_t queryCount = 0, sampleCount = 0;
        bool sizeMatched = false;
    };

    // Requires a new directory. Bound:24 frames,256 UI marks/frame,32 original
    // operations/frame,64 texture identities,512 MiB total written bytes.
    explicit MaterialIdentityCapture(const std::string& newOutputDirectory);
    ~MaterialIdentityCapture();
    MaterialIdentityCapture(const MaterialIdentityCapture&) = delete;
    MaterialIdentityCapture& operator=(const MaterialIdentityCapture&) = delete;
    static MaterialIdentityCapture* active() noexcept;
    bool isFrameOpen() const noexcept;
    bool wantsMaterial(int materialId) const noexcept;
    const std::vector<MapSample>& lastMapSamples() const noexcept;

    void beginFrame(const std::string& phase, const std::string& mode,
                    std::uint64_t frameId);
    UiRange beginUiRange(const ImDrawList* list) const;
    void recordUiRange(const UiRange& range, const Facts& facts,
                       UiCallback nearestCallback = nullptr,
                       UiCallback restoreCallback = nullptr);
    void observeMapQueries(const MapBatch& batch,
                           const std::vector<MapSample>& samples);

    // selectedPass is the actual production pass chosen by Bootstrap. Capture
    // records it before replay. Replay shares its real GL VBO/IBO and texture;
    // it uses loaded production shader source with neutral diagnostic uniforms.
    // firstIndices are offsets relative to the original operation. Empty selects
    // actual six-index faces, bounded to the observed block for consumer=world.
    void observeRenderable(const Facts& facts, Ogre::Renderable& renderable,
                           Ogre::Pass& selectedPass, const Ogre::Camera& camera,
                           const std::vector<std::size_t>& firstIndices = {});

    // Call after actual ImGui backend submission and before swap, while the
    // original frame's context/framebuffer is current. Its state is restored.
    void finishFrame(const ImDrawData* drawData, bool captureFramebuffer = true);
    std::size_t frameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
