#include "Chunk.h"

#include "../../Core/Camera.h"
#include "../../Maths/NoiseGenerator.h"
#include "../../Renderer/RenderMaster.h"
#include "../../Util/Random.h"
#include "../Generation/Terrain/TerrainGenerator.h"
#include "../World.h"

#include <utility>

Chunk::Chunk(World &world, const sf::Vector2i &location)
    : m_location(location)
    , m_pWorld(&world)
{
    m_highestBlocks.setAll(0);
}

bool Chunk::makeMesh()
{
    return makeMeshes(1) > 0;
}

int Chunk::makeMeshes(int maxSections, int preferredSectionY)
{
    if (maxSections <= 0) {
        return 0;
    }

    int builtSections = 0;
    const int sectionCount = static_cast<int>(m_chunks.size());
    if (sectionCount <= 0) {
        return 0;
    }

    auto tryBuildSection = [&](int sectionIndex) {
        if (sectionIndex < 0 || sectionIndex >= sectionCount) {
            return false;
        }

        ChunkSection &section = m_chunks[sectionIndex];
        if (!section.isMeshDirty()) {
            return false;
        }

        section.makeMesh();
        ++builtSections;
        --maxSections;
        return maxSections <= 0;
    };

    if (preferredSectionY >= 0) {
        if (preferredSectionY >= sectionCount) {
            preferredSectionY = sectionCount - 1;
        }

        for (int offset = 0; offset < sectionCount && maxSections > 0;
             ++offset) {
            if (offset == 0) {
                tryBuildSection(preferredSectionY);
                continue;
            }

            if (tryBuildSection(preferredSectionY + offset)) {
                break;
            }
            tryBuildSection(preferredSectionY - offset);
        }

        return builtSections;
    }

    for (int sectionIndex = sectionCount - 1;
         sectionIndex >= 0 && maxSections > 0; --sectionIndex) {
        tryBuildSection(sectionIndex);
    }

    return builtSections;
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

int Chunk::getHeightAt(int x, int z)
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
    m_chunks.emplace_back(sf::Vector3i(m_location.x, y, m_location.y),
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
