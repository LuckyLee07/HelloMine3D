#ifndef SECTIONMESHINPUT_H_INCLUDED
#define SECTIONMESHINPUT_H_INCLUDED

#include "../../Maths/glm.h"
#include <array>
#include <cstdint>

#include "../Block/ChunkBlock.h"
#include "../WorldConstants.h"

class ChunkSection;

/// @brief Snapshot of everything a section mesh build reads.
///
/// The mesh builder needs the section's own blocks plus a one block halo, and
/// the layer solidity flags used to skip fully enclosed layers. Reading those
/// straight from the world requires the world lock, which meant mesh building
/// held that lock for its whole duration.
///
/// Capturing the neighbourhood once (cheap, under the lock) lets the build
/// itself run without any world access at all.
class SectionMeshInput {
  public:
    static constexpr int Size = CHUNK_SIZE + 2;
    static constexpr int Volume = Size * Size * Size;

    /// Must be called while the world lock is held.
    void capture(ChunkSection &section);

    /// Valid for coordinates in [-1, CHUNK_SIZE].
    ChunkBlock getBlock(int x, int y, int z) const;

    /// Valid for y in [0, CHUNK_SIZE).
    bool shouldMakeLayer(int y) const;

    const glm::ivec3 &getLocation() const;

  private:
    static int index(int x, int y, int z);

    std::array<ChunkBlock, Volume> m_blocks{};

    /// Own layers for y in [-1, CHUNK_SIZE], stored at y + 1.
    std::array<bool, CHUNK_SIZE + 2> m_ownLayerAllSolid{};

    /// Layers of the four horizontal neighbours, y in [0, CHUNK_SIZE).
    std::array<std::array<bool, CHUNK_SIZE>, 4> m_neighbourLayerAllSolid{};

    glm::ivec3 m_location{};
};

#endif // SECTIONMESHINPUT_H_INCLUDED
