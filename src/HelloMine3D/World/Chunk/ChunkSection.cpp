#include "ChunkSection.h"

#include "../Block/BlockId.h"

#include "../World.h"
#include "ChunkMeshBuilder.h"
#include "SectionMeshInput.h"

#include <fstream>
#include <iostream>
#include <thread>

ChunkSection::ChunkSection(const glm::ivec3 &location, World &world)
    : m_aabb({CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE})
    , m_location(location)
    , m_pWorld(&world)
{
    m_sunlight.fill(MAX_LIGHT_LEVEL);
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

    m_layers[y].update(currentBlock, block);
    currentBlock = block;
    ++m_blockRevision;
    markMeshDirty();
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

void ChunkSection::setSunlight(int x, int y, int z, LightLevel level)
{
    if (outOfBounds(x) || outOfBounds(y) || outOfBounds(z)) {
        return;
    }

    m_sunlight[getIndex(x, y, z)] = clampLightLevel(level);
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

glm::ivec3 ChunkSection::getLocation() const
{
    return m_location;
}

bool ChunkSection::hasMesh() const
{
    return m_meshState != ChunkSectionMeshState::Dirty;
}

bool ChunkSection::hasBuffered() const
{
    return m_meshState == ChunkSectionMeshState::GpuBuffered;
}

bool ChunkSection::isMeshDirty() const
{
    return m_meshState == ChunkSectionMeshState::Dirty;
}

ChunkSectionMeshState ChunkSection::getMeshState() const
{
    return m_meshState;
}

void ChunkSection::markMeshDirty()
{
    if (m_meshState != ChunkSectionMeshState::Dirty) {
        deleteMeshes();
    }

    m_meshState = ChunkSectionMeshState::Dirty;
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
    SectionMeshInput input;
    captureMeshInput(input);

    m_meshes.solidMesh.clearClientData();
    m_meshes.waterMesh.clearClientData();
    m_meshes.floraMesh.clearClientData();
    const bool built = input.needsMeshBuild();
    if (built) {
        ChunkMeshBuilder(input, m_meshes).buildMesh();
    }
    m_meshState = ChunkSectionMeshState::CpuReady;
    return built;
}

void ChunkSection::captureMeshInput(SectionMeshInput &input)
{
    input.capture(*this);
}

void ChunkSection::adoptMesh(ChunkMeshCollection &built)
{
    m_meshes.solidMesh.clearClientData();
    m_meshes.waterMesh.clearClientData();
    m_meshes.floraMesh.clearClientData();
    m_meshes.adoptClientData(built);
    m_meshState = ChunkSectionMeshState::CpuReady;
}

std::uint32_t ChunkSection::getBlockRevision() const
{
    return m_blockRevision;
}

void ChunkSection::markGpuBuffered()
{
    if (!hasMesh()) {
        return;
    }

    m_meshState = ChunkSectionMeshState::GpuBuffered;
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
    if (m_meshState != ChunkSectionMeshState::Dirty) {
        m_meshes.solidMesh.clearClientData();
        m_meshes.waterMesh.clearClientData();
        m_meshes.floraMesh.clearClientData();
    }

    m_meshState = ChunkSectionMeshState::Dirty;
}

ChunkSection &ChunkSection::getAdjacent(int dx, int dz)
{
    int newX = m_location.x + dx;
    int newZ = m_location.z + dz;

    return m_pWorld->getChunkManager()
        .getChunk(newX, newZ)
        .getSection(m_location.y);
}

bool ChunkSection::outOfBounds(int value)
{
    return value >= CHUNK_SIZE || value < 0;
}

int ChunkSection::getIndex(int x, int y, int z)
{
    return y * CHUNK_AREA + z * CHUNK_SIZE + x;
}
