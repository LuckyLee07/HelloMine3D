#pragma once

#include "../World/Chunk/ChunkMesh.h"
#include "../World/WorldConstants.h"
#include "../Maths/glm.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

// Renderer-only packing. Blocks, section revisions and transparent sorting
// remain per section; at most four vertical neighbours share an opaque draw.
constexpr int TerrainRenderBatchSections = 4;

// The built-in programs derive effects from world coordinates. Unknown or
// blended passes keep their original section origins and sorting granularity.
inline bool canBatchTerrainMaterial(std::size_t passes, bool depthWrite,
                                    bool transparent, std::string_view vertex,
                                    std::string_view fragment)
{
    const bool knownVertex = vertex == "HelloMine3D/TerrainVertex" ||
        vertex == "HelloMine3D/TerrainShadowVertex" ||
        vertex == "HelloMine3D/FloraVertex" ||
        vertex == "HelloMine3D/FloraShadowVertex";
    const bool knownFragment = fragment == "HelloMine3D/TerrainFragment" ||
        fragment == "HelloMine3D/TerrainShadowFragment" ||
        fragment == "HelloMine3D/TerrainArrayFragment" ||
        fragment == "HelloMine3D/TerrainShadowArrayFragment";
    return passes == 1 && depthWrite && !transparent && knownVertex && knownFragment;
}
inline glm::ivec3 terrainRenderBatchOrigin(glm::ivec3 section)
{
    int group = section.y / TerrainRenderBatchSections;
    if (section.y < 0 && section.y % TerrainRenderBatchSections != 0) --group;
    section.y = group * TerrainRenderBatchSections;
    return section;
}
struct TerrainRenderBatchPart {
    glm::ivec3 location;
    const ChunkMesh* mesh;
};
struct TerrainRenderVertex {
    float x, y, z, u, v, repeatU, repeatV, light;
};
static_assert(sizeof(TerrainRenderVertex) == 32, "Terrain vertex format stays unchanged");
struct PackedTerrainRenderBatch {
    std::vector<TerrainRenderVertex> vertices;
    std::vector<std::uint32_t> indices;
    int heightSections = 1;
};
inline void validateTerrainRenderPart(const TerrainRenderBatchPart& part)
{
    if (!part.mesh) throw std::runtime_error("missing terrain mesh");
    const auto& mesh = part.mesh->getClientMesh();
    const auto& light = part.mesh->getLight();
    const auto count = mesh.vertexPositions.size() / 3;
    if (mesh.vertexPositions.size() % 3 != 0 || mesh.textureCoords.size() != count * 2 ||
        mesh.textureRepeatCoords.size() != count * 2 || light.size() != count || mesh.indices.size() % 3 != 0)
        throw std::runtime_error("terrain vertex attribute or index count mismatch");
    for (const auto index : mesh.indices)
        if (index >= count) throw std::runtime_error("terrain index references a missing vertex");
    for (std::size_t i = 0; i < count; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            const double v = mesh.vertexPositions[i * 3 + axis] -
                static_cast<double>(part.location[axis]) * CHUNK_SIZE;
            if (!std::isfinite(v) || v < -0.001f || v > CHUNK_SIZE + 0.001f)
                throw std::runtime_error("a vertex lies outside its section bounds");
        }
        if (!std::isfinite(mesh.textureCoords[i*2]) || !std::isfinite(mesh.textureCoords[i*2+1]) ||
            !std::isfinite(mesh.textureRepeatCoords[i*2]) || !std::isfinite(mesh.textureRepeatCoords[i*2+1]) ||
            !std::isfinite(light[i]) || light[i] < 0.f || light[i] > 1.f)
            throw std::runtime_error("terrain vertex contains an invalid attribute");
    }
}
inline PackedTerrainRenderBatch packTerrainRenderBatch(
    const std::vector<TerrainRenderBatchPart>& parts, const glm::ivec3& origin)
{
    if (parts.size() > TerrainRenderBatchSections) throw std::runtime_error("terrain batch exceeds four sections");
    PackedTerrainRenderBatch result;
    std::size_t vertices = 0, indices = 0;
    unsigned slots = 0;
    for (const auto& part : parts) {
        const std::int64_t y = static_cast<std::int64_t>(part.location.y) - origin.y;
        if (part.location.x != origin.x || part.location.z != origin.z || y < 0 || y >= TerrainRenderBatchSections)
            throw std::runtime_error("terrain batch part is outside its column group");
        if (slots & (1u << y)) throw std::runtime_error("duplicate terrain batch section");
        slots |= 1u << y;
        validateTerrainRenderPart(part);
        vertices += part.mesh->getClientMesh().vertexPositions.size() / 3;
        indices += part.mesh->getClientMesh().indices.size();
        result.heightSections = std::max(result.heightSections, static_cast<int>(y) + 1);
    }
    if (vertices > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("terrain batch index overflow");
    result.vertices.reserve(vertices);
    result.indices.reserve(indices);
    for (const auto& part : parts) {
        const auto& mesh = part.mesh->getClientMesh();
        const auto& light = part.mesh->getLight();
        const auto base = static_cast<std::uint32_t>(result.vertices.size());
        for (std::size_t i = 0; i < light.size(); ++i) {
            result.vertices.push_back({
                mesh.vertexPositions[i*3] - static_cast<float>(origin.x)*CHUNK_SIZE,
                mesh.vertexPositions[i*3+1] - static_cast<float>(origin.y)*CHUNK_SIZE,
                mesh.vertexPositions[i*3+2] - static_cast<float>(origin.z)*CHUNK_SIZE,
                mesh.textureCoords[i*2], mesh.textureCoords[i*2+1],
                mesh.textureRepeatCoords[i*2], mesh.textureRepeatCoords[i*2+1], light[i]});
        }
        for (const auto index : mesh.indices) result.indices.push_back(base + index);
    }
    return result;
}
