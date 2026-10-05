#pragma once

#include "ChunkSectionRenderable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Explicit render-thread diagnostic of original production upload storage.
// Bootstrap supplies copies from its actual uploader and locked live revisions.
// No World access, render replay, draw query, context, or retained object pointer.
class ShoreEditCapture final {
public:
    struct Part {
        glm::ivec3 section{0};
        std::uint32_t uploadRevision = 0, liveRevision = 0;
        bool liveKnown = false, gpuResident = false, stillCpuReady = true;
    };
    struct Binding {
        ChunkSectionRenderable* renderable = nullptr;
        glm::ivec3 origin{0};
        std::string layer; // "water" or "solid"
        std::string ownerKey;
        PackedTerrainRenderBatch cpu;
        std::vector<Part> parts; // Original constructor order, at most four.
        std::uint64_t uploadSerial = 0; // Bootstrap's diagnostic object lifetime.
    };

    // Requires a fresh directory. At most six phases, eight original objects per
    // phase, 16 MiB VBO+IBO per object, and 256 MiB total observer writes.
    explicit ShoreEditCapture(const std::string& newOutputDirectory);
    ~ShoreEditCapture();
    ShoreEditCapture(const ShoreEditCapture&) = delete;
    ShoreEditCapture& operator=(const ShoreEditCapture&) = delete;

    // Synchronous, current context. Root independently retains World/map facts
    // and saves the actual window to phasePngPath() in this same original frame.
    // Raw storage agreement does not prove inner VAO fetch or visible pixels.
    void capturePhase(const std::string& phase, const std::string& mode,
                      std::uint64_t frameId, const std::vector<Binding>& bindings);
    std::size_t phaseCount() const noexcept;
    std::string phaseJsonPath() const;
    std::string phasePngPath() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
