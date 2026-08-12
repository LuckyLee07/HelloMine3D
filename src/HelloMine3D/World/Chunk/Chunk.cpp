#include "Chunk.h"

#include "../../Core/Camera.h"
#include "../../Maths/NoiseGenerator.h"
#include "../../Util/Random.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "../Block/BlockDatabase.h"
#include "../World.h"

#include <array>
#include <deque>
#include <utility>

Chunk::Chunk(World &world, const glm::ivec2 &location)
    : m_location(location)
    , m_pWorld(&world)
{
    m_highestBlocks.setAll(0);
}

int Chunk::findDirtySection(int preferredSectionY) const
{
    const int sectionCount = static_cast<int>(m_chunks.size());
    if (sectionCount <= 0) {
        return -1;
    }

    auto isDirty = [&](int index) {
        return index >= 0 && index < sectionCount &&
               m_chunks[index].isMeshDirty();
    };

    if (preferredSectionY < 0) {
        for (int index = sectionCount - 1; index >= 0; --index) {
            if (isDirty(index)) {
                return index;
            }
        }
        return -1;
    }

    if (preferredSectionY >= sectionCount) {
        preferredSectionY = sectionCount - 1;
    }

    // Search outwards from the section the camera is in, so the sections the
    // player is most likely looking at are rebuilt first.
    for (int offset = 0; offset < sectionCount; ++offset) {
        if (offset == 0) {
            if (isDirty(preferredSectionY)) {
                return preferredSectionY;
            }
            continue;
        }

        if (isDirty(preferredSectionY + offset)) {
            return preferredSectionY + offset;
        }
        if (isDirty(preferredSectionY - offset)) {
            return preferredSectionY - offset;
        }
    }

    return -1;
}

void Chunk::setBlock(int x, int y, int z, ChunkBlock block)
{
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || z < 0 || z >= CHUNK_SIZE) {
        return;
    }

    addSectionsBlockTarget(y);

    int bY = y % CHUNK_SIZE;
    auto &section = m_chunks[y / CHUNK_SIZE];
    const auto previousBlock = section.getBlock(x, bY, z);
    if (previousBlock == block) {
        return;
    }

    const int previousHighest = m_highestBlocks.get(x, z);
    section.setBlock(x, bY, z, block);

    if (block.getData().isOpaque && y > previousHighest) {
        m_highestBlocks.get(x, z) = y;
    }
    else if (!block.getData().isOpaque && y == previousHighest) {
        int newHighest = 0;
        for (int scanY = y - 1; scanY >= 0; --scanY) {
            if (getBlock(x, scanY, z).getData().isOpaque) {
                newHighest = scanY;
                break;
            }
        }

        m_highestBlocks.get(x, z) = newHighest;
    }

    if (hasLoaded()) {
        m_saveDirty = true;
    }
}

// Chunk block to SECTION BLOCK positions
ChunkBlock Chunk::getBlock(int x, int y, int z) const noexcept
{
    if (outOfBound(x, y, z)) {
        return BlockId::Air;
    }

    int bY = y % CHUNK_SIZE;

    return m_chunks[y / CHUNK_SIZE].getBlock(x, bY, z);
}

LightLevel Chunk::getSunlight(int x, int y, int z) const noexcept
{
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE || y < 0) {
        return MIN_LIGHT_LEVEL;
    }

    if (y >= static_cast<int>(m_chunks.size()) * CHUNK_SIZE) {
        return MAX_LIGHT_LEVEL;
    }

    return m_chunks[y / CHUNK_SIZE].getSunlight(x, y % CHUNK_SIZE, z);
}

void Chunk::rebuildSunlight()
{
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            rebuildSunlightColumn(x, z);
        }
    }
}

std::vector<int> Chunk::rebuildSunlightColumn(int x, int z)
{
    std::vector<int> changedY;
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
        return changedY;
    }

    const int topY = static_cast<int>(m_chunks.size()) * CHUNK_SIZE - 1;
    bool skyVisible = true;
    for (int y = topY; y >= 0; --y) {
        if (getBlock(x, y, z).getData().isOpaque) {
            skyVisible = false;
        }
        if (m_chunks[y / CHUNK_SIZE].setSunlight(
                x, y % CHUNK_SIZE, z,
                skyVisible ? MAX_LIGHT_LEVEL : MIN_LIGHT_LEVEL)) {
            changedY.push_back(y);
        }
    }
    return changedY;
}

LightLevel Chunk::getBlockLight(int x, int y, int z) const noexcept
{
    if (outOfBound(x, y, z)) {
        return MIN_LIGHT_LEVEL;
    }

    return m_chunks[y / CHUNK_SIZE].getBlockLight(x, y % CHUNK_SIZE, z);
}

bool Chunk::setBlockLight(int x, int y, int z, LightLevel level) noexcept
{
    if (outOfBound(x, y, z)) {
        return false;
    }
    return m_chunks[y / CHUNK_SIZE].setBlockLight(x, y % CHUNK_SIZE, z,
                                                  level);
}

void Chunk::rebuildBlockLight()
{
    const int height = static_cast<int>(m_chunks.size()) * CHUNK_SIZE;
    std::deque<glm::ivec3> pending;

    for (int y = 0; y < height; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                auto &section = m_chunks[y / CHUNK_SIZE];
                section.setBlockLight(x, y % CHUNK_SIZE, z,
                                      MIN_LIGHT_LEVEL);
                const ChunkBlock block = getBlock(x, y, z);
                const int emission =
                    BlockDatabase::get()
                        .getDefinition(static_cast<BlockId>(block.id))
                        .light;
                if (emission > 0) {
                    section.setBlockLight(
                        x, y % CHUNK_SIZE, z,
                        clampLightLevel(static_cast<LightLevel>(emission)));
                    pending.emplace_back(x, y, z);
                }
            }
        }
    }

    const std::array<glm::ivec3, 6> offsets = {
        glm::ivec3{1, 0, 0},  glm::ivec3{-1, 0, 0},
        glm::ivec3{0, 1, 0},  glm::ivec3{0, -1, 0},
        glm::ivec3{0, 0, 1},  glm::ivec3{0, 0, -1},
    };
    while (!pending.empty()) {
        const glm::ivec3 position = pending.front();
        pending.pop_front();
        const LightLevel current =
            getBlockLight(position.x, position.y, position.z);
        if (current <= MIN_LIGHT_LEVEL + 1) {
            continue;
        }

        const LightLevel propagated = static_cast<LightLevel>(current - 1);
        for (const glm::ivec3 &offset : offsets) {
            const glm::ivec3 adjacent = position + offset;
            if (outOfBound(adjacent.x, adjacent.y, adjacent.z) ||
                getBlock(adjacent.x, adjacent.y,
                         adjacent.z).getData().isOpaque ||
                getBlockLight(adjacent.x, adjacent.y, adjacent.z) >=
                    propagated) {
                continue;
            }

            m_chunks[adjacent.y / CHUNK_SIZE].setBlockLight(
                adjacent.x, adjacent.y % CHUNK_SIZE, adjacent.z,
                propagated);
            pending.push_back(adjacent);
        }
    }
}

int Chunk::getHeightAt(int x, int z) const
{
    return m_highestBlocks.get(x, z);
}

bool Chunk::outOfBound(int x, int y, int z) const noexcept
{
    if (x >= CHUNK_SIZE)
        return true;
    if (z >= CHUNK_SIZE)
        return true;

    if (x < 0)
        return true;
    if (y < 0)
        return true;
    if (z < 0)
        return true;

    if (y >= (int)m_chunks.size() * CHUNK_SIZE) {
        return true;
    }

    return false;
}

bool Chunk::hasLoaded() const noexcept
{
    return m_loadState == ChunkLoadState::Loaded;
}

bool Chunk::hasGenerated() const noexcept
{
    return m_loadState == ChunkLoadState::Generating ||
           m_loadState == ChunkLoadState::Loaded;
}

bool Chunk::needsSave() const noexcept
{
    return m_saveDirty;
}

ChunkLoadState Chunk::getLoadState() const noexcept
{
    return m_loadState;
}

void Chunk::clearSaveDirty() noexcept
{
    m_saveDirty = false;
}

std::size_t Chunk::getSectionCount() const noexcept
{
    return m_chunks.size();
}

std::size_t Chunk::countSections(ChunkSectionMeshState state) const noexcept
{
    std::size_t count = 0;
    for (const auto &section : m_chunks) {
        if (section.getMeshState() == state) {
            ++count;
        }
    }

    return count;
}

void Chunk::collectBlockData(std::vector<Block_t> &blockIds,
                             std::vector<BlockMetadata_t> &metadata) const
{
    blockIds.clear();
    metadata.clear();
    blockIds.reserve(m_chunks.size() * CHUNK_VOLUME);
    metadata.reserve(m_chunks.size() * CHUNK_VOLUME);

    for (std::size_t sectionIndex = 0; sectionIndex < m_chunks.size();
         ++sectionIndex) {
        const int yBase = static_cast<int>(sectionIndex) * CHUNK_SIZE;
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const auto block = getBlock(x, yBase + y, z);
                    blockIds.push_back(block.id);
                    metadata.push_back(block.metadata);
                }
            }
        }
    }
}

void Chunk::loadBlockData(std::size_t sectionCount,
                          const std::vector<Block_t> &blockIds,
                          const std::vector<BlockMetadata_t> &metadata)
{
    m_pWorld->removeRandomTickSectionsForChunk(m_location.x, m_location.y);
    m_chunks.clear();
    m_highestBlocks.setAll(0);
    m_loadState = ChunkLoadState::Generating;
    m_saveDirty = false;

    const auto expectedBlockCount = sectionCount * CHUNK_VOLUME;
    if (blockIds.size() < expectedBlockCount) {
        m_loadState = ChunkLoadState::Empty;
        return;
    }

    if (sectionCount > 0) {
        addSectionsIndexTarget(static_cast<int>(sectionCount - 1));
    }

    std::size_t index = 0;
    for (std::size_t sectionIndex = 0; sectionIndex < sectionCount;
         ++sectionIndex) {
        const int yBase = static_cast<int>(sectionIndex) * CHUNK_SIZE;
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    const BlockMetadata_t blockMetadata =
                        index < metadata.size() ? metadata[index] : 0;
                    setBlock(x, yBase + y, z,
                             ChunkBlock(blockIds[index], blockMetadata));
                    ++index;
                }
            }
        }
    }

    m_loadState = ChunkLoadState::Loaded;
    rebuildSunlight();
    rebuildBlockLight();
    m_saveDirty = false;
}

const std::vector<BlockEntityRecord> &Chunk::getBlockEntities() const
{
    return m_blockEntities;
}

void Chunk::loadBlockEntities(std::vector<BlockEntityRecord> blockEntities)
{
    m_blockEntities = std::move(blockEntities);
}

void Chunk::load(TerrainGenerator &generator)
{
    if (hasLoaded())
        return;

    m_loadState = ChunkLoadState::Generating;
    generator.generateTerrainFor(*this);
    rebuildSunlight();
    rebuildBlockLight();
    m_loadState = ChunkLoadState::Loaded;
    m_saveDirty = false;
}

ChunkSection &Chunk::getSection(int index)
{
    static ChunkSection errorSection({444, 444, 444}, *m_pWorld);

    if (index >= (int)m_chunks.size() || index < 0)
        return errorSection;

    return m_chunks[index];
}

ChunkSection *Chunk::findSection(int index)
{
    if (!hasSection(index)) {
        return nullptr;
    }

    return &m_chunks[index];
}

const ChunkSection *Chunk::findSection(int index) const
{
    if (!hasSection(index)) {
        return nullptr;
    }

    return &m_chunks[index];
}

bool Chunk::hasSection(int index) const noexcept
{
    return index >= 0 && index < (int)m_chunks.size();
}

void Chunk::deleteMeshes()
{
    for (unsigned i = 0; i < m_chunks.size(); i++) {
        m_chunks[i].deleteMeshes();
    }
}

void Chunk::addSection()
{
    int y = static_cast<int>(m_chunks.size());
    m_chunks.emplace_back(glm::ivec3(m_location.x, y, m_location.y),
                          *m_pWorld);
}

void Chunk::addSectionsBlockTarget(int blockY)
{
    int index = blockY / CHUNK_SIZE;
    addSectionsIndexTarget(index);
}

void Chunk::addSectionsIndexTarget(int index)
{
    while ((int)m_chunks.size() < index + 1) {
        addSection();
    }
}
