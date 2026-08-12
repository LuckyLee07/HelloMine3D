#pragma once

#include <cstddef>
#include <cstdint>

struct TerrainBufferMetrics
{
    static constexpr std::size_t VertexStrideBytes = sizeof(float) * 8;
    static constexpr std::size_t IndexStrideBytes = sizeof(std::uint32_t);

    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;

    void add(std::size_t vertices, std::size_t indices) noexcept
    {
        vertexCount += vertices;
        indexCount += indices;
    }

    std::size_t vertexBytes() const noexcept
    {
        return vertexCount * VertexStrideBytes;
    }

    std::size_t indexBytes() const noexcept
    {
        return indexCount * IndexStrideBytes;
    }

    std::size_t totalBytes() const noexcept
    {
        return vertexBytes() + indexBytes();
    }
};

static_assert(TerrainBufferMetrics::VertexStrideBytes == 32,
              "The measured terrain vertex stride changed.");
static_assert(TerrainBufferMetrics::IndexStrideBytes == 4,
              "The measured terrain index stride changed.");
