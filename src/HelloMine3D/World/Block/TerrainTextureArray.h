#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

/// Portable, bounded CPU payload; no graphics types cross the World boundary.
struct TerrainTextureArray
{
    std::uint32_t edge = 0;
    std::uint32_t layers = 0;
    std::uint32_t mipCount = 0;
    std::uint64_t contentHash = 0;
    std::vector<unsigned char> rgba;

    static TerrainTextureArray load(const std::string &path)
    {
        const auto fail = [&path](const char *reason)
        {
            throw std::runtime_error("Invalid terrain texture array '" +
                                     path + "': " + reason + ".");
        };
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) fail("cannot open resource");
        const auto length = input.tellg();
        if (length < 36 || length > 24 * 1024 * 1024)
            fail("file size outside bounded format");
        input.seekg(0);
        std::array<unsigned char, 36> header{};
        if (!input.read(reinterpret_cast<char *>(header.data()), header.size()))
            fail("truncated header");
        if (std::string(reinterpret_cast<char *>(header.data()), 8) != "HMTARRAY")
            fail("invalid magic");
        const auto u32 = [&header](std::size_t offset)
        {
            std::uint32_t result = 0;
            for (unsigned i = 0; i < 4; ++i)
                result |= std::uint32_t(header[offset + i]) << (8u * i);
            return result;
        };
        if (u32(8) != 1) fail("unsupported version");
        TerrainTextureArray result;
        result.edge = u32(12);
        result.layers = u32(16);
        result.mipCount = u32(20);
        if ((result.edge != 64 && result.edge != 128) || result.layers != 256 ||
            result.mipCount != (result.edge == 64 ? 7u : 8u))
            fail("invalid dimensions or mip chain");
        std::size_t expected = 0;
        for (auto edge = result.edge; edge; edge >>= 1u)
            expected += std::size_t(edge) * edge * result.layers * 4u;
        if (u32(24) != expected || length != std::streamoff(expected + 36u))
            fail("payload size mismatch or trailing data");
        for (unsigned i = 0; i < 8; ++i)
            result.contentHash |= std::uint64_t(header[28 + i]) << (8u * i);
        result.rgba.resize(expected);
        if (!input.read(reinterpret_cast<char *>(result.rgba.data()), expected))
            fail("truncated payload");
        std::uint64_t hash = UINT64_C(14695981039346656037);
        for (unsigned char byte : result.rgba)
            hash = (hash ^ byte) * UINT64_C(1099511628211);
        if (hash != result.contentHash) fail("content checksum mismatch");
        return result;
    }
};
