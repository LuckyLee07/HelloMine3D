#include "ChunkSection.h"

#include "../Block/BlockBehavior.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockId.h"

#include "../World.h"
#include "ChunkMeshBuilder.h"
#include "SectionMeshInput.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <thread>

ChunkSection::ChunkSection(const glm::ivec3 &location, World &world,
                           bool updateWorldIndex)
    : m_aabb({CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE})
    , m_location(location)
    , m_pWorld(&world)
    , m_worldIndexUpdatesEnabled(updateWorldIndex)
{
    m_sunlight.fill(MAX_LIGHT_LEVEL);
    m_blockLight.fill(MIN_LIGHT_LEVEL);
    m_aabb.update({location.x * CHUNK_SIZE, location.y * CHUNK_SIZE,
                   location.z * CHUNK_SIZE});
}

void ChunkSection::setBlock(int x, int y, int z, ChunkBlock block)
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        auto location = toWorldPosition(x, y, z);
        m_pWorld->setBlock(location.x, location.y, location.z, block);
        return;
    }

    auto &currentBlock = m_blocks[getIndex(x, y, z)];
    if (currentBlock == block) {
        return;
    }

    const int blockIndex = getIndex(x, y, z);
    const auto receivesRandomTicks = [](const ChunkBlock &candidate) {
        const auto &definition = BlockDatabase::get().getDefinition(
            static_cast<BlockId>(candidate.id));
        return definition.behavior != nullptr &&
               definition.behavior->receivesRandomTicks(definition,
                                                         candidate);
    };
    const bool sectionWasActive = !m_randomTickBlocks.empty();
    if (receivesRandomTicks(currentBlock)) {
        const auto found = std::find(m_randomTickBlocks.begin(),
                                     m_randomTickBlocks.end(), blockIndex);
        if (found != m_randomTickBlocks.end()) {
            *found = m_randomTickBlocks.back();
            m_randomTickBlocks.pop_back();
        }
    }
    if (receivesRandomTicks(block)) {
        m_randomTickBlocks.push_back(
            static_cast<std::uint16_t>(blockIndex));
    }

    m_layers[y].update(currentBlock, block);
    currentBlock = block;
    invalidateMeshInput();

    const bool sectionIsActive = !m_randomTickBlocks.empty();
    if (m_worldIndexUpdatesEnabled &&
        sectionWasActive != sectionIsActive) {
        m_pWorld->updateRandomTickSection(m_location, sectionIsActive);
    }
}

void ChunkSection::setWorldIndexUpdatesEnabled(bool enabled) noexcept
{
    m_worldIndexUpdatesEnabled = enabled;
}

ChunkBlock ChunkSection::getBlock(int x, int y, int z) const
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        auto location = toWorldPosition(x, y, z);
        // Mesh snapshots already hold the world lock. Calling the public
        // accessor here would try to acquire the same non-recursive mutex.
        return m_pWorld->getBlockUnlocked(location.x, location.y, location.z);
    }

    return m_blocks[getIndex(x, y, z)];
}

bool ChunkSection::setSunlight(int x, int y, int z, LightLevel level)
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        return false;
    }

    LightLevel &current = m_sunlight[getIndex(x, y, z)];
    const LightLevel bounded = clampLightLevel(level);
    if (current == bounded) {
        return false;
    }
    current = bounded;
    return true;
}

LightLevel ChunkSection::getSunlight(int x, int y, int z) const
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        const auto location = toWorldPosition(x, y, z);
        return m_pWorld->getSunlightUnlocked(location.x, location.y,
                                             location.z);
    }

    return m_sunlight[getIndex(x, y, z)];
}

bool ChunkSection::setBlockLight(int x, int y, int z, LightLevel level)
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        return false;
    }

    LightLevel &current = m_blockLight[getIndex(x, y, z)];
    const LightLevel bounded = clampLightLevel(level);
    if (current == bounded) {
        return false;
    }
    current = bounded;
    return true;
}

LightLevel ChunkSection::getBlockLight(int x, int y, int z) const
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        const auto location = toWorldPosition(x, y, z);
        return m_pWorld->getBlockLightUnlocked(location.x, location.y,
                                               location.z);
    }

    return m_blockLight[getIndex(x, y, z)];
}

glm::ivec3 ChunkSection::getLocation() const
{
    return m_location;
}

bool ChunkSection::hasMesh() const
{
    return m_meshState == ChunkMeshState::CpuReady ||
           m_meshState == ChunkMeshState::Clean;
}

bool ChunkSection::isMeshDirty() const
{
    return m_meshState == ChunkMeshState::Dirty;
}

ChunkMeshState ChunkSection::getMeshState() const
{
    return m_meshState;
}

bool ChunkSection::transitionMeshState(ChunkMeshState state) noexcept
{
    const bool legal = canTransition(m_meshState, state);
    assert(legal && "illegal ChunkSection mesh transition");
    if (!legal) {
        return false;
    }
    m_meshState = state;
    return true;
}

void ChunkSection::markMeshDirty()
{
    if (m_meshState == ChunkMeshState::Dirty) {
        return;
    }

    m_meshes.solidMesh.clearClientData();
    m_meshes.transparentMesh.clearClientData();
    m_meshes.waterMesh.clearClientData();
    m_meshes.floraMesh.clearClientData();
    transitionMeshState(ChunkMeshState::Dirty);
}

void ChunkSection::markMeshQueued()
{
    transitionMeshState(ChunkMeshState::Queued);
}

void ChunkSection::beginMeshBuild()
{
    transitionMeshState(ChunkMeshState::Building);
}

void ChunkSection::invalidateMeshInput()
{
    ++m_blockRevision;
    markMeshDirty();
}

glm::ivec3 ChunkSection::toWorldPosition(int x, int y, int z) const
{
    return {m_location.x * CHUNK_SIZE + x, m_location.y * CHUNK_SIZE + y,
            m_location.z * CHUNK_SIZE + z};
}

bool ChunkSection::makeMesh()
{
    // Synchronous path, used by main-thread block edits. It still holds the
    // world lock for the whole build, which is fine for the handful of
    // sections a single edit touches. Streaming uses the split path below.
    if (m_meshState == ChunkMeshState::CpuReady ||
        m_meshState == ChunkMeshState::Clean) {
        return false;
    }
    if (m_meshState == ChunkMeshState::Dirty) {
        markMeshQueued();
    }
    beginMeshBuild();

    SectionMeshInput input;
    captureMeshInput(input);

    m_meshes.solidMesh.clearClientData();
    m_meshes.transparentMesh.clearClientData();
    m_meshes.waterMesh.clearClientData();
    m_meshes.floraMesh.clearClientData();
    const bool built = input.needsMeshBuild();
    if (built) {
        ChunkMeshBuilder(input, m_meshes).buildMesh();
    }
    transitionMeshState(ChunkMeshState::CpuReady);
    return built;
}

void ChunkSection::captureMeshInput(SectionMeshInput &input)
{
    ChunkManager &manager = m_pWorld->getChunkManager();
    input.capture(*this, manager.getTerrainGenerator(),
                  manager.getTerrainSeed());
}

void ChunkSection::adoptMesh(ChunkMeshCollection &built)
{
    m_meshes.solidMesh.clearClientData();
    m_meshes.transparentMesh.clearClientData();
    m_meshes.waterMesh.clearClientData();
    m_meshes.floraMesh.clearClientData();
    m_meshes.adoptClientData(built);
    transitionMeshState(ChunkMeshState::CpuReady);
}

std::uint32_t ChunkSection::getBlockRevision() const
{
    return m_blockRevision;
}

std::size_t ChunkSection::getRandomTickBlockCount() const noexcept
{
    return m_randomTickBlocks.size();
}

bool ChunkSection::selectRandomTickBlock(std::size_t selection,
                                         glm::ivec3 &worldPosition,
                                         ChunkBlock &block) const
{
    if (m_randomTickBlocks.empty()) {
        return false;
    }

    const int index = static_cast<int>(m_randomTickBlocks[
        selection % m_randomTickBlocks.size()]);
    const int y = index / CHUNK_AREA;
    const int remainder = index % CHUNK_AREA;
    const int z = remainder / CHUNK_SIZE;
    const int x = remainder % CHUNK_SIZE;
    worldPosition = toWorldPosition(x, y, z);
    block = m_blocks[index];
    return true;
}

void ChunkSection::markMeshClean()
{
    transitionMeshState(ChunkMeshState::Clean);
}

const ChunkSection::Layer &ChunkSection::getLayer(int y) const
{
    if (y == -1) {
        return m_pWorld->getChunkManager()
            .getChunk(m_location.x, m_location.z)
            .getSection(m_location.y - 1)
            .getLayer(CHUNK_SIZE - 1);
    }
    else if (y == CHUNK_SIZE) {
        return m_pWorld->getChunkManager()
            .getChunk(m_location.x, m_location.z)
            .getSection(m_location.y + 1)
            .getLayer(0);
    }
    else {
        return m_layers[y];
    }
}

void ChunkSection::deleteMeshes()
{
    markMeshDirty();
}

const ChunkSection *ChunkSection::findAdjacent(int dx, int dz) const
{
    const int newX = m_location.x + dx;
    const int newZ = m_location.z + dz;

    const Chunk *chunk = m_pWorld->getChunkManager().findChunk(newX, newZ);
    return chunk != nullptr && chunk->hasLoaded()
               ? chunk->findSection(m_location.y)
               : nullptr;
}

bool ChunkSection::outOfBounds(int value)
{
    return value >= CHUNK_SIZE || value < 0;
}

int ChunkSection::getIndex(int x, int y, int z)
{
    return y * CHUNK_AREA + z * CHUNK_SIZE + x;
}
